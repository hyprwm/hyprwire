#include "HandshakeProtocols.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CHandshakeProtocolsMessage::CHandshakeProtocolsMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_ARRAY)
            return;

        if (data.at(offset + 2) != HW_MESSAGE_MAGIC_TYPE_VARCHAR)
            return;

        size_t needle = 3;

        auto [els, varIntLen] = g_messageParser->parseVarInt(data, offset + needle);

        needle += varIntLen;

        m_protocols.resize(els);

        for (size_t i = 0; i < els; ++i) {
            auto [strLen, strLenLen] = g_messageParser->parseVarInt(data, offset + needle);

            m_protocols.at(i) = std::string_view{rc<const char*>(&data.at(offset + needle + strLenLen)), strLen};
            needle += strLen + strLenLen;
        }

        if (data.at(offset + needle) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = needle + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CHandshakeProtocolsMessage::CHandshakeProtocolsMessage(const std::vector<std::string>& protocols) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS;

    m_data = {
        HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS,
        HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_VARCHAR,
    };

    m_data.append_range(g_messageParser->encodeVarInt(protocols.size()));

    for (const auto& p : protocols) {
        m_data.append_range(g_messageParser->encodeVarInt(p.size()));
        m_data.append_range(p);
    }

    m_data.emplace_back(HW_MESSAGE_MAGIC_END);
}
