#include "IWireObject.hpp"

#include "../../helpers/Log.hpp"
#include "../../helpers/FFI.hpp"
#include "../client/ClientObject.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/MessageMagic.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include <hyprwire/core/types/MessageMagic.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <cstdarg>
#include <cstring>
#include <ffi.h>

using namespace Hyprwire;
using namespace Hyprutils::Utils;

IWireObject::~IWireObject() = default;

uint32_t IWireObject::call(uint32_t id, ...) {
    const auto METHODS = methodsOut();
    if (METHODS.size() <= id) {
        const auto MSG = std::format("core protocol error: invalid method {} for object {}", id, m_id);
        Debug::log(ERR, "core protocol error: {}", MSG);
        error(m_id, MSG);
        return 0;
    }

    va_list va;
    va_start(va, id);

    const auto& method = METHODS.at(id);
    const auto  params = method.params;

    if (method.since > m_version) {
        const auto MSG = std::format("method {} since {} but has {}", id, method.since, m_version);
        Debug::log(ERR, "core protocol error: {}", MSG);
        error(m_id, MSG);
        return 0;
    }

    if (!method.returnsType.empty() && server()) {
        const auto MSG = std::format("invalid method spec {} for object {} -> server cannot call returnsType methods", id, m_id);
        Debug::log(ERR, "core protocol error: {}", MSG);
        error(m_id, MSG);
        return 0;
    }

    // encode the message
    std::vector<uint8_t> data;
    std::vector<int>     fds;
    data.emplace_back(HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE);
    data.emplace_back(HW_MESSAGE_MAGIC_TYPE_OBJECT);

    data.resize(data.size() + 4);
    *rc<uint32_t*>(&data[data.size() - 4]) = m_id;

    data.emplace_back(HW_MESSAGE_MAGIC_TYPE_UINT);

    data.resize(data.size() + 4);
    *rc<uint32_t*>(&data[data.size() - 4]) = id;

    size_t waitOnSeq = 0;

    if (!method.returnsType.empty()) {
        data.emplace_back(HW_MESSAGE_MAGIC_TYPE_SEQ);

        data.resize(data.size() + 4);
        auto selfClient                        = reinterpretPointerCast<CClientObject>(m_self.lock());
        *rc<uint32_t*>(&data[data.size() - 4]) = ++selfClient->m_client->m_seq;
        waitOnSeq                              = selfClient->m_client->m_seq;
    }

    for (size_t i = 0; i < params.size(); ++i) {
        switch (sc<eMessageMagic>(params.at(i))) {
            case HW_MESSAGE_MAGIC_TYPE_UINT: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_UINT);
                data.resize(data.size() + 4);
                *rc<uint32_t*>(&data[data.size() - 4]) = va_arg(va, uint32_t);
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_INT: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_INT);
                data.resize(data.size() + 4);
                *rc<int32_t*>(&data[data.size() - 4]) = va_arg(va, int32_t);
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_OBJECT);
                data.resize(data.size() + 4);
                *rc<uint32_t*>(&data[data.size() - 4]) = va_arg(va, uint32_t);
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_F32: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_F32);
                data.resize(data.size() + 4);
                *rc<float*>(&data[data.size() - 4]) = va_arg(va, double);
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_VARCHAR);
                auto str = va_arg(va, const char*);
                data.append_range(g_messageParser->encodeVarInt(std::string_view(str).size()));
                data.append_range(std::string_view(str));
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                const auto arrType = sc<eMessageMagic>(params.at(++i));
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_ARRAY);
                data.emplace_back(arrType);

                auto arrayData = va_arg(va, void*);
                auto arrayLen  = va_arg(va, uint32_t);
                data.append_range(g_messageParser->encodeVarInt(arrayLen));

                switch (arrType) {
                    case HW_MESSAGE_MAGIC_TYPE_UINT:
                    case HW_MESSAGE_MAGIC_TYPE_INT:
                    case HW_MESSAGE_MAGIC_TYPE_F32:
                    case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
                        for (size_t i = 0; i < arrayLen; ++i) {
                            data.resize(data.size() + 4);
                            *rc<uint32_t*>(&data[data.size() - 4]) = rc<uint32_t*>(arrayData)[i];
                        }
                        break;
                    }
                    case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                        for (size_t i = 0; i < arrayLen; ++i) {
                            const char* element = rc<const char**>(arrayData)[i];
                            data.append_range(g_messageParser->encodeVarInt(std::string_view(element).size()));
                            data.append_range(std::string_view(element));
                        }
                        break;
                    }
                    default: {
                        Debug::log(ERR, "core protocol error: failed marshaling array type");
                        errd();
                        return 0;
                    }
                }

                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_FD: {
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_FD);

                // add fd to our message
                fds.emplace_back(va_arg(va, int32_t));
                break;
            }

            default: break;
        }
    }

    data.emplace_back(HW_MESSAGE_MAGIC_END);

    auto msg = CGenericProtocolMessage(std::move(data), std::move(fds));
    sendMessage(msg);

    if (waitOnSeq) {
        // we are a client
        auto selfClient = reinterpretPointerCast<CClientObject>(m_self.lock());
        auto obj        = selfClient->m_client->makeObject(m_protocolName, method.returnsType, waitOnSeq);
        selfClient->m_client->waitForObject(obj);
        return obj->m_id;
    }

    return 0;
}

void IWireObject::listen(uint32_t id, void* fn) {
    if (m_listeners.size() <= id)
        m_listeners.resize(id + 1);

    m_listeners.at(id) = fn;
}

void IWireObject::called(uint32_t id, const std::span<const uint8_t>& data, const std::vector<int>& fds) {
    const auto METHODS = methodsIn();
    if (METHODS.size() <= id) {
        const auto MSG = std::format("invalid method {} for object {}", id, m_id);
        Debug::log(ERR, "core protocol error: {}", MSG);
        error(m_id, MSG);
        return;
    }

    if (m_listeners.size() <= id || m_listeners.at(id) == nullptr)
        return;

    const auto&          method = METHODS.at(id);
    std::vector<uint8_t> params;

    if (!method.returnsType.empty())
        params.emplace_back(HW_MESSAGE_MAGIC_TYPE_SEQ);

    params.append_range(method.params);

    if (method.since > m_version) {
        const auto MSG = std::format("method {} since {} but has {}", id, method.since, m_version);
        Debug::log(ERR, "core protocol error: {}", MSG);
        error(m_id, MSG);
        return;
    }

    std::vector<ffi_type*> ffiTypes = {&ffi_type_pointer};
    size_t                 dataI    = 0;
    for (size_t i = 0; i < params.size(); ++i) {
        const auto PARAM      = sc<eMessageMagic>(params.at(i));
        const auto WIRE_PARAM = sc<eMessageMagic>(data[dataI]);

        if (PARAM != WIRE_PARAM) {
            // raise protocol error
            const auto MSG = std::format("method {} param idx {} should be {} but was {}", id, i, magicToString(PARAM), magicToString(WIRE_PARAM));
            Debug::log(ERR, "core protocol error: {}", MSG);
            error(m_id, MSG);
            return;
        }

        auto ffiType = FFI::ffiTypeFrom(PARAM);
        ffiTypes.emplace_back(ffiType);

        switch (PARAM) {
            case HW_MESSAGE_MAGIC_END: ++i; break; // BUG if this happens or malformed message
            case HW_MESSAGE_MAGIC_TYPE_FD: dataI++; break;
            case HW_MESSAGE_MAGIC_TYPE_UINT:
            case HW_MESSAGE_MAGIC_TYPE_F32:
            case HW_MESSAGE_MAGIC_TYPE_INT:
            case HW_MESSAGE_MAGIC_TYPE_OBJECT:
            case HW_MESSAGE_MAGIC_TYPE_SEQ: dataI += 5; break;
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                auto [a, b] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[dataI], data.size() - dataI});
                dataI += a + b + 1;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                const auto arrType  = sc<eMessageMagic>(params.at(++i));
                const auto wireType = sc<eMessageMagic>(data[dataI + 1]);

                if (arrType != wireType) {
                    // raise protocol error
                    const auto MSG = std::format("method {} param idx {} should be {} but was {}", id, i, magicToString(PARAM), magicToString(WIRE_PARAM));
                    Debug::log(ERR, "core protocol error: {}", MSG);
                    error(m_id, MSG);
                    return;
                }

                auto [arrLen, lenLen] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[dataI + 2], data.size() - i});
                size_t arrMessageLen  = 2 + lenLen;

                ffiTypes.emplace_back(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_UINT /* length */));

                switch (arrType) {
                    case HW_MESSAGE_MAGIC_TYPE_UINT:
                    case HW_MESSAGE_MAGIC_TYPE_F32:
                    case HW_MESSAGE_MAGIC_TYPE_INT:
                    case HW_MESSAGE_MAGIC_TYPE_OBJECT:
                    case HW_MESSAGE_MAGIC_TYPE_SEQ: {
                        arrMessageLen += 4 * arrLen;
                        break;
                    }
                    case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                        for (size_t j = 0; j < arrLen; ++j) {
                            if (dataI + arrMessageLen > data.size()) {
                                const auto MSG = std::format("failed demarshaling array message");
                                Debug::log(ERR, "core protocol error: {}", MSG);
                                error(m_id, MSG);
                                return;
                            }
                            auto [strLen, strlenLen] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[dataI + arrMessageLen], data.size() - dataI - arrMessageLen});
                            arrMessageLen += strLen + strlenLen;
                        }
                        break;
                    }
                    default: {
                        const auto MSG = std::format("failed demarshaling array message");
                        Debug::log(ERR, "core protocol error: {}", MSG);
                        error(m_id, MSG);
                        return;
                    }
                }

                dataI += arrMessageLen;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_OBJECT_ID: {
                const auto MSG = std::format("object type is not impld");
                Debug::log(ERR, "core protocol error: {}", MSG);
                error(m_id, MSG);
                return;
            }
        }
    }

    ffi_cif cif;
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, ffiTypes.size(), &ffi_type_void, ffiTypes.data())) {
        Debug::log(ERR, "core protocol error: ffi failed");
        errd();
        return;
    }

    std::vector<void*> avalues, otherBuffers;
    avalues.reserve(ffiTypes.size());
    std::vector<SP<std::string>> strings;

    auto                         ptrBuf = malloc(sizeof(IObject*));
    avalues.emplace_back(ptrBuf);
    *rc<IObject**>(ptrBuf) = m_self.get();

    size_t      fdNo = 0;

    CScopeGuard x([&] {
        for (const auto& v : avalues) {
            free(v);
        }
        for (const auto& v : otherBuffers) {
            free(v);
        }
    });

    for (size_t i = 0; i < data.size(); ++i) {
        void*      buf   = nullptr;
        const auto PARAM = sc<eMessageMagic>(data[i]);
        // FIXME: add type checking

        if (PARAM == HW_MESSAGE_MAGIC_END)
            break;

        switch (PARAM) {
            case HW_MESSAGE_MAGIC_END: break;
            case HW_MESSAGE_MAGIC_TYPE_UINT: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_F32: {
                buf              = malloc(sizeof(float));
                *rc<float*>(buf) = *rc<const float*>(&data[i + 1]);
                i += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_INT: {
                buf                = malloc(sizeof(int32_t));
                *rc<int32_t*>(buf) = *rc<const int32_t*>(&data[i + 1]);
                i += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_SEQ: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                auto [strLen, len]     = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[i + 1], data.size() - i - 1});
                buf                    = malloc(sizeof(const char*));
                auto& str              = strings.emplace_back(makeShared<std::string>(std::string_view{rc<const char*>(&data[i + len + 1]), strLen}));
                *rc<const char**>(buf) = str->c_str();
                i += strLen + len;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                const auto arrType    = sc<eMessageMagic>(data[i + 1]);
                auto [arrLen, lenLen] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[i + 2], data.size() - i});
                size_t arrMessageLen  = 2 + lenLen;

                switch (arrType) {
                    case HW_MESSAGE_MAGIC_TYPE_UINT:
                    case HW_MESSAGE_MAGIC_TYPE_F32:
                    case HW_MESSAGE_MAGIC_TYPE_INT:
                    case HW_MESSAGE_MAGIC_TYPE_OBJECT:
                    case HW_MESSAGE_MAGIC_TYPE_SEQ: {
                        auto dataPtr  = rc<uint32_t*>(malloc(sizeof(uint32_t) * (arrLen == 0 ? 1 : arrLen)));
                        auto dataSlot = rc<uint32_t**>(malloc(sizeof(uint32_t**)));
                        auto sizeSlot = rc<uint32_t*>(malloc(sizeof(uint32_t)));

                        *dataSlot = dataPtr;
                        *sizeSlot = arrLen;

                        avalues.emplace_back(dataSlot);
                        avalues.emplace_back(sizeSlot);
                        otherBuffers.emplace_back(dataPtr);

                        for (size_t j = 0; j < arrLen; ++j) {
                            dataPtr[j] = *rc<const uint32_t*>(&data[i + arrMessageLen]);
                            arrMessageLen += 4;
                        }
                        break;
                    }
                    case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                        auto dataPtr  = rc<const char**>(malloc(sizeof(const char*) * (arrLen == 0 ? 1 : arrLen)));
                        auto dataSlot = rc<const char***>(malloc(sizeof(const char***)));
                        auto sizeSlot = rc<uint32_t*>(malloc(sizeof(uint32_t)));

                        *dataSlot = dataPtr;
                        *sizeSlot = arrLen;

                        avalues.emplace_back(dataSlot);
                        avalues.emplace_back(sizeSlot);
                        otherBuffers.emplace_back(dataPtr);

                        for (size_t j = 0; j < arrLen; ++j) {
                            auto [strLen, strlenLen] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[i + arrMessageLen], data.size() - i});
                            auto& str  = strings.emplace_back(makeShared<std::string>(std::string_view{rc<const char*>(&data[i + arrMessageLen + strlenLen]), strLen}));
                            dataPtr[j] = str->c_str();
                            arrMessageLen += strlenLen + strLen;
                        }
                        break;
                    }
                    default: {
                        const auto MSG = std::format("failed demarshaling array message");
                        Debug::log(ERR, "core protocol error: {}", MSG);
                        error(m_id, MSG);
                        return;
                    }
                }

                i += arrMessageLen - 1 /* for loop does ++i*/;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_OBJECT_ID: {
                const auto MSG = std::format("object type is not impld");
                Debug::log(ERR, "core protocol error: {}", MSG);
                error(m_id, MSG);
                return;
            }
            case HW_MESSAGE_MAGIC_TYPE_FD: {
                buf                = malloc(sizeof(int32_t));
                *rc<int32_t*>(buf) = fds.at(fdNo++);
                break;
            }
        }
        if (buf)
            avalues.emplace_back(buf);
    }

    auto fptr = reinterpret_cast<void (*)()>(m_listeners.at(id));
    ffi_call(&cif, fptr, nullptr, avalues.data());
}
