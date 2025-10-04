#include "MessageParser.hpp"
#include "MessageType.hpp"
#include "../server/ServerClient.hpp"
#include "../server/ServerSocket.hpp"
#include "../server/ServerObject.hpp"
#include "../client/ClientSocket.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/Defines.hpp"
#include "../../Macros.hpp"

#include "messages/HandshakeAck.hpp"
#include "messages/HandshakeBegin.hpp"
#include "messages/Hello.hpp"
#include "messages/HandshakeProtocols.hpp"
#include "messages/BindProtocol.hpp"
#include "messages/NewObject.hpp"
#include "messages/GenericProtocolMessage.hpp"

#include <hyprwire/core/implementation/ServerImpl.hpp>
#include <hyprwire/core/implementation/Spec.hpp>
#include <algorithm>

using namespace Hyprwire;

eMessageParsingResult CMessageParser::handleMessage(const std::vector<uint8_t>& data, SP<CServerClient> client) {
    size_t needle = 0;
    while (needle < data.size()) {
        auto ret = parseSingleMessage(data, needle, client);
        if (ret == 0)
            return MESSAGE_PARSED_ERROR;

        needle += ret;
    }
    return MESSAGE_PARSED_OK;
}

eMessageParsingResult CMessageParser::handleMessage(const std::vector<uint8_t>& data, SP<CClientSocket> client) {
    size_t needle = 0;
    while (needle < data.size()) {
        auto ret = parseSingleMessage(data, needle, client);
        if (ret == 0)
            return MESSAGE_PARSED_ERROR;

        needle += ret;
    }
    return MESSAGE_PARSED_OK;
}

size_t CMessageParser::parseSingleMessage(const std::vector<uint8_t>& data, size_t off, SP<CServerClient> client) {

    switch (sc<eMessageType>(data.at(off))) {
        case HW_MESSAGE_TYPE_SUP: {
            auto msg = makeShared<CHelloMessage>(data, off);
            if (!msg->m_len) {
                Debug::log(ERR, "client at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_SUP)", client->m_fd.get());
                return 0;
            }
            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));
            client->sendMessage(makeShared<CHandshakeBeginMessage>(std::vector<uint32_t>{HYPRWIRE_PROTOCOL_VER}));
            return msg->m_len;
        }
        case HW_MESSAGE_TYPE_HANDSHAKE_BEGIN: {
            client->m_error = true;
            Debug::log(ERR, "client at fd {} core protocol error: invalid message recvd (HANDSHAKE_BEGIN)", client->m_fd.get());
            return 0;
        }
        case HW_MESSAGE_TYPE_HANDSHAKE_ACK: {
            auto msg = makeShared<CHandshakeAckMessage>(data, off);
            if (!msg->m_len) {
                Debug::log(ERR, "client at fd {} core protocol error: malformed message recvd (HW_MESSAGE_HANDSHAKE_ACK)", client->m_fd.get());
                return 0;
            }
            client->m_version = msg->m_version;

            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

            std::vector<std::string> protocolNames;
            protocolNames.reserve(client->m_server->m_impls.size());
            for (const auto& impl : client->m_server->m_impls) {
                protocolNames.emplace_back(std::format("{}@{}", impl->protocol()->specName(), impl->protocol()->specVer()));
            }
            client->sendMessage(makeShared<CHandshakeProtocolsMessage>(protocolNames));

            return msg->m_len;
        }
        case HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS: {
            client->m_error = true;
            Debug::log(ERR, "client at fd {} core protocol error: invalid message recvd (HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS)", client->m_fd.get());
            return 0;
        }
        case HW_MESSAGE_TYPE_BIND_PROTOCOL: {
            auto msg = makeShared<CBindProtocolMessage>(data, off);
            if (!msg->m_len) {
                Debug::log(ERR, "client at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_BIND_PROTOCOL)", client->m_fd.get());
                return 0;
            }

            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

            // create a protocol object and send it back
            auto obj = client->createObject(msg->m_protocol, "", msg->m_version);

            auto ret = makeShared<CNewObjectMessage>(msg->m_seq, obj->m_id);
            client->sendMessage(ret);

            client->onBind(obj);

            return msg->m_len;
        }
        case HW_MESSAGE_TYPE_NEW_OBJECT: {
            client->m_error = true;
            Debug::log(ERR, "client at fd {} core protocol error: invalid message recvd (HW_MESSAGE_TYPE_NEW_OBJECT)", client->m_fd.get());
            return 0;
        }
        case HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE: {
            auto msg = makeShared<CGenericProtocolMessage>(data, off);
            if (!msg->m_len) {
                Debug::log(ERR, "server at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE)", client->m_fd.get());
                return 0;
            }

            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

            client->onGeneric(msg);

            return msg->m_len;
        }
    }

    Debug::log(ERR, "client at fd {} core protocol error: malformed message recvd (invalid type code)", client->m_fd.get());
    client->m_error = true;

    return 0;
}

size_t CMessageParser::parseSingleMessage(const std::vector<uint8_t>& data, size_t off, SP<CClientSocket> client) {
    try {
        switch (sc<eMessageType>(data.at(off))) {
            case HW_MESSAGE_TYPE_SUP: {
                client->m_error = true;
                Debug::log(ERR, "server at fd {} core protocol error: invalid message recvd (HW_MESSAGE_TYPE_SUP)", client->m_fd.get());
                return 0;
            }
            case HW_MESSAGE_TYPE_HANDSHAKE_BEGIN: {
                auto msg = makeShared<CHandshakeBeginMessage>(data, off);
                if (!msg->m_len) {
                    Debug::log(ERR, "server at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_HANDSHAKE_BEGIN)", client->m_fd.get());
                    return 0;
                }

                if (!std::ranges::contains(msg->m_versionsSupported, HYPRWIRE_PROTOCOL_VER)) {
                    Debug::log(ERR, "server at fd {} core protocol error: version negotiation failed", client->m_fd.get());
                    return 0;
                }

                TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

                // version supported: let's select it
                client->sendMessage(makeShared<CHandshakeAckMessage>(HYPRWIRE_PROTOCOL_VER));

                return msg->m_len;
            }
            case HW_MESSAGE_TYPE_HANDSHAKE_ACK: {
                client->m_error = true;
                Debug::log(ERR, "server at fd {} core protocol error: invalid message recvd (HW_MESSAGE_TYPE_HANDSHAKE_ACK)", client->m_fd.get());
                return 0;
            }
            case HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS: {
                auto msg = makeShared<CHandshakeProtocolsMessage>(data, off);
                if (!msg->m_len) {
                    Debug::log(ERR, "server at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS)", client->m_fd.get());
                    return 0;
                }

                TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

                client->serverSpecs(msg->m_protocols);

                return msg->m_len;
            }
            case HW_MESSAGE_TYPE_BIND_PROTOCOL: {
                client->m_error = true;
                Debug::log(ERR, "server at fd {} core protocol error: invalid message recvd (HW_MESSAGE_TYPE_BIND_PROTOCOL)", client->m_fd.get());
                return 0;
            }
            case HW_MESSAGE_TYPE_NEW_OBJECT: {
                auto msg = makeShared<CNewObjectMessage>(data, off);
                if (!msg->m_len) {
                    Debug::log(ERR, "server at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_NEW_OBJECT)", client->m_fd.get());
                    return 0;
                }

                TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

                client->onSeq(msg->m_seq, msg->m_id);

                return msg->m_len;
            }
            case HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE: {
                auto msg = makeShared<CGenericProtocolMessage>(data, off);
                if (!msg->m_len) {
                    Debug::log(ERR, "server at fd {} core protocol error: malformed message recvd (HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE)", client->m_fd.get());
                    return 0;
                }

                TRACE(Debug::log(TRACE, "[{} @ {:.3f}] <- {}", client->m_fd.get(), steadyMillis(), msg->parseData()));

                client->onGeneric(msg);

                return msg->m_len;
            }
        }

    } catch (std::out_of_range& e) { ; }

    Debug::log(ERR, "server at fd {} core protocol error: invalid message recvd (invalid type code)", client->m_fd.get());

    return 0;
}

std::pair<size_t, size_t> CMessageParser::parseVarInt(const std::vector<uint8_t>& data, size_t offset) {
    return parseVarInt(std::span<const uint8_t>{&data[offset], data.size() - offset});
}

std::pair<size_t, size_t> CMessageParser::parseVarInt(const std::span<const uint8_t>& data) {
    size_t     rolling = 0;
    size_t     i       = 0;
    const auto LEN     = data.size();
    do {
        rolling += ((sc<uint8_t>(data[i] << 1) >> 1) << (i++ * 7));
    } while (i < LEN && data[i] & 0x80);

    return {rolling, i};
}

std::vector<uint8_t> CMessageParser::encodeVarInt(size_t num) {
    std::vector<uint8_t> data;
    data.resize(4);
    data[0] = (uint8_t)((num << 25) >> 25) | 0x80;
    data[1] = (uint8_t)((num << 18) >> 25) | 0x80;
    data[2] = (uint8_t)((num << 11) >> 25) | 0x80;
    data[3] = (uint8_t)((num << 4) >> 25) | 0x80;
    while (data.back() == 0x80 && data.size() > 1) {
        data.pop_back();
    }

    data.back() &= ~0x80;

    return data;
}
