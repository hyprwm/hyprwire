#include "RoundtripRequest.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../wireObject/IWireObject.hpp"
#include "../../../helpers/Env.hpp"

#include <cstring>
#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CRoundtripRequestMessage::CRoundtripRequestMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        if ((data.size() - offset - 2) < sizeof(m_seq))
            return;

        std::memcpy(&m_seq, &data.at(offset + 2), sizeof(m_seq));

        if (data.at(offset + 6) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = 7;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CRoundtripRequestMessage::CRoundtripRequestMessage(uint32_t seq) : m_seq(seq) {
    m_type = HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST;

    m_data = {HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_END};

    std::memcpy(&m_data[2], &seq, sizeof(seq));
}
