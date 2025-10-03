#include "Hello.hpp"
#include "../MessageType.hpp"
#include "../MessageMagic.hpp"
#include "../MessageParser.hpp"
#include <stdexcept>

using namespace Hyprwire;

CHelloMessage::CHelloMessage() {
    m_type = HW_MESSAGE_TYPE_HELLO;

    m_data.reserve(2);

    m_data = {
        HW_MESSAGE_TYPE_HELLO,
        HW_MESSAGE_MAGIC_END,
    };
}
