#include "IWireObject.hpp"

#include "../../helpers/Log.hpp"
#include "../../helpers/FFI.hpp"
#include "../client/ClientObject.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstdarg>
#include <cstring>
#include <ffi.h>

using namespace Hyprwire;

uint32_t IWireObject::call(uint32_t id, ...) {
    const auto METHODS = methodsOut();
    if (METHODS.size() <= id) {
        Debug::log(ERR, "core protocol error: invalid method {} for object {}", id, m_id);
        errd();
        return 0;
    }

    va_list va;
    va_start(va, id);

    const auto& method = METHODS.at(id);
    const auto  params = method.params;

    if (method.since > m_version) {
        Debug::log(ERR, "core protocol error: method {} since {} but has {}", id, method.since, m_version);
        errd();
        return 0;
    }

    if (!method.returnsType.empty() && server()) {
        Debug::log(ERR, "core protocol error: invalid method spec {} for object {} -> server cannot call returnsType methods", id, m_id);
        errd();
        return 0;
    }

    // encode the message
    std::vector<uint8_t> data;
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
                data.append_range(g_messageParser->encodeVarInt(std::strlen(str)));
                data.append_range(std::string_view(str));
                break;
            }

            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                // FIXME:
                break;
            }

            default: break;
        }
    }

    data.emplace_back(HW_MESSAGE_MAGIC_END);

    auto msg = makeShared<CGenericProtocolMessage>(std::move(data));
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

void IWireObject::called(uint32_t id, const std::span<const uint8_t>& data) {
    const auto METHODS = methodsIn();
    if (METHODS.size() <= id) {
        Debug::log(ERR, "core protocol error: invalid method {} for object {}", id, m_id);
        errd();
        return;
    }

    if (m_listeners.size() <= id || m_listeners.at(id) == nullptr)
        return;

    const auto& method = METHODS.at(id);
    const auto  params = method.params;

    if (method.since > m_version) {
        Debug::log(ERR, "core protocol error: method {} since {} but has {}", id, method.since, m_version);
        errd();
        return;
    }

    std::vector<ffi_type*> ffiTypes = {&ffi_type_pointer};
    if (!method.returnsType.empty())
        ffiTypes.emplace_back(&ffi_type_uint32);
    for (size_t i = 0; i < params.size(); ++i) {
        const auto PARAM   = sc<eMessageMagic>(params.at(i));
        auto       ffiType = FFI::ffiTypeFrom(PARAM);
        // FIXME: this will break with arrays
        ffiTypes.emplace_back(ffiType);

        switch (PARAM) {
            case HW_MESSAGE_MAGIC_END: ++i; break; // BUG if this happens or malformed message
            case HW_MESSAGE_MAGIC_TYPE_UINT:
            case HW_MESSAGE_MAGIC_TYPE_F32:
            case HW_MESSAGE_MAGIC_TYPE_INT:
            case HW_MESSAGE_MAGIC_TYPE_OBJECT:
            case HW_MESSAGE_MAGIC_TYPE_SEQ: i += 5; break;
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR:
                auto [a, b] = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[i], data.size() - i});
                i += a + b + 1;
                break;
        }
    }

    ffi_cif cif;
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, ffiTypes.size(), &ffi_type_void, ffiTypes.data())) {
        Debug::log(ERR, "core protocol error: ffi failed");
        errd();
        return;
    }

    std::vector<void*> avalues;
    std::vector<void*> buffers;
    avalues.reserve(ffiTypes.size());
    buffers.reserve(ffiTypes.size());
    std::vector<SP<std::string>> strings;

    auto                         ptrBuf = malloc(sizeof(IObject*));
    buffers.emplace_back(ptrBuf);
    avalues.emplace_back(ptrBuf);
    *rc<IObject**>(ptrBuf) = m_self.get();

    for (size_t i = 0; i < data.size(); ++i) {
        void*      buf   = nullptr;
        const auto PARAM = sc<eMessageMagic>(data[i]);
        // FIXME: this will break with arrays
        // FIXME: add type checking

        switch (PARAM) {
            case HW_MESSAGE_MAGIC_END: ++i; break; // BUG if this happens or malformed message
            case HW_MESSAGE_MAGIC_TYPE_UINT: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 5;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_F32: {
                buf              = malloc(sizeof(float));
                *rc<float*>(buf) = *rc<const float*>(&data[i + 1]);
                i += 5;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_INT: {
                buf                = malloc(sizeof(int32_t));
                *rc<int32_t*>(buf) = *rc<const int32_t*>(&data[i + 1]);
                i += 5;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 5;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_SEQ: {
                buf                 = malloc(sizeof(uint32_t));
                *rc<uint32_t*>(buf) = *rc<const uint32_t*>(&data[i + 1]);
                i += 5;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR:
                auto [strLen, len]     = g_messageParser->parseVarInt(std::span<const uint8_t>{&data[i + 1], data.size() - i - 1});
                buf                    = malloc(sizeof(const char*));
                auto str               = strings.emplace_back(makeShared<std::string>(std::string_view{rc<const char*>(&data[i + len + 1]), strLen}));
                *rc<const char**>(buf) = str->c_str();
                i += strLen + len + 1;
                break;
        }
        buffers.emplace_back(buf);
        avalues.emplace_back(buf);
    }

    auto fptr = reinterpret_cast<void (*)()>(m_listeners.at(id));
    ffi_call(&cif, fptr, nullptr, avalues.data());

    for (const auto& v : avalues) {
        free(v);
    }
}
