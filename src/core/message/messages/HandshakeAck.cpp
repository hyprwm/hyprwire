#include "HandshakeAck.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CHandshakeAckMessage::CHandshakeAckMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_ACK;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_HANDSHAKE_ACK)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        size_t needle = 2;

        if (data.at(offset + needle + 4) != HW_MESSAGE_MAGIC_END)
            return;

        m_version = *rc<const uint32_t*>(&data.at(offset + needle));

        needle += 4;

        m_len = needle + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};
    } catch (std::out_of_range& e) { m_len = 0; }
}

CHandshakeAckMessage::CHandshakeAckMessage(uint32_t version) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_ACK;

    m_data.reserve(7);

    m_data = {
        HW_MESSAGE_TYPE_HANDSHAKE_ACK,
        HW_MESSAGE_MAGIC_TYPE_UINT,
    };

    m_data.resize(6);

    *rc<uint32_t*>(&m_data[2]) = version;

    m_data.emplace_back(HW_MESSAGE_MAGIC_END);
}
