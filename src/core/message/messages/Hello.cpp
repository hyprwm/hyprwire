#include "Hello.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <cstring>
#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CHelloMessage::CHelloMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_SUP;

    try {
        constexpr const std::array<uint8_t, 7> expected = {
            HW_MESSAGE_TYPE_SUP, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x03, 'V', 'A', 'X', HW_MESSAGE_MAGIC_END,
        };

        if (std::memcmp(expected.data(), &data.at(offset), expected.size()) != 0)
            return;

        m_len = expected.size();

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CHelloMessage::CHelloMessage() {
    m_type = HW_MESSAGE_TYPE_SUP;

    m_data = {
        HW_MESSAGE_TYPE_SUP, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x03, 'V', 'A', 'X', HW_MESSAGE_MAGIC_END,
    };
}
