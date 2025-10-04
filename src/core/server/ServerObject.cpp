#include "ServerObject.hpp"
#include "../../helpers/Log.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstdarg>
#include <cstring>
#include <ffi.h>

using namespace Hyprwire;

CServerObject::CServerObject(SP<CServerClient> client) : m_client(client) {
    ;
}

// FIXME: this is the same as client!!11!1!
uint32_t CServerObject::call(uint32_t id, ...) {
    if (m_spec->s2c().size() <= id) {
        Debug::log(ERR, "server core protocol error: invalid method {} for object {}", id, m_id);
        m_client->m_error = true;
        return 0;
    }

    va_list va;
    va_start(va, id);

    const auto params = m_spec->s2c().at(id).params;

    // encode the message
    std::vector<uint8_t> data;
    data.emplace_back(HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE);
    data.emplace_back(HW_MESSAGE_MAGIC_TYPE_OBJECT);

    data.resize(data.size() + 4);
    *rc<uint32_t*>(&data[data.size() - 4]) = m_id;

    data.emplace_back(HW_MESSAGE_MAGIC_TYPE_UINT);

    data.resize(data.size() + 4);
    *rc<uint32_t*>(&data[data.size() - 4]) = id;

    for (size_t i = 0; i < params.size(); ++i) {
        switch (sc<eMessageType>(params.at(i))) {
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
        }
    }

    data.emplace_back(HW_MESSAGE_MAGIC_END);

    auto msg = makeShared<CGenericProtocolMessage>(std::move(data));
    m_client->sendMessage(msg);

    return 0;
}

void CServerObject::listen(uint32_t id, void* fn) {
    if (m_listeners.size() <= id)
        m_listeners.resize(id + 1);

    m_listeners.at(id) = fn;
}