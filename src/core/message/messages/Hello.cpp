#include "Hello.hpp"
#include "../MessageType.hpp"
#include "../MessageMagic.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"
#include <stdexcept>

using namespace Hyprwire;

CHelloMessage::CHelloMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_HELLO;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_HELLO)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = 2;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.at(offset), data.at(offset + m_len - 1)};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CHelloMessage::CHelloMessage() {
    m_type = HW_MESSAGE_TYPE_HELLO;

    m_data.reserve(2);

    m_data = {
        HW_MESSAGE_TYPE_HELLO,
        HW_MESSAGE_MAGIC_END,
    };
}
