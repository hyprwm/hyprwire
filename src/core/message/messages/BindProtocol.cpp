#include "BindProtocol.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CBindProtocolMessage::CBindProtocolMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_BIND_PROTOCOL;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_BIND_PROTOCOL)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        std::memcpy(&m_seq, &data.at(offset + 2), sizeof(m_seq));

        if (data.at(offset + 6) != HW_MESSAGE_MAGIC_TYPE_VARCHAR)
            return;

        size_t needle = 7;

        auto [strLen, varIntLen] = g_messageParser->parseVarInt(data, offset + needle);

        needle += varIntLen;

        m_protocol = std::string_view{rc<const char*>(&data.at(offset + needle)), strLen};

        needle += strLen;

        if (data.at(offset + needle) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        std::memcpy(&m_version, &data.at(offset + needle + 1), sizeof(m_version));

        if (!m_version)
            return;

        needle += 5;

        if (data.at(offset + needle) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = needle + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CBindProtocolMessage::CBindProtocolMessage(const std::string& protocol, uint32_t seq, uint32_t version) {
    m_type = HW_MESSAGE_TYPE_BIND_PROTOCOL;

    m_data = {
        HW_MESSAGE_TYPE_BIND_PROTOCOL, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0,
    };

    std::memcpy(&m_data[2], &seq, sizeof(seq));

    m_data.emplace_back(HW_MESSAGE_MAGIC_TYPE_VARCHAR);

    m_data.append_range(g_messageParser->encodeVarInt(protocol.length()));
    m_data.append_range(protocol);

    m_data.append_range(std::vector<uint8_t>{HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0});

    std::memcpy(&m_data[m_data.size() - 4], &version, sizeof(version));

    m_data.emplace_back(HW_MESSAGE_MAGIC_END);
}
