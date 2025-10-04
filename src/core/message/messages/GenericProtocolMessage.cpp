#include "GenericProtocolMessage.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CGenericProtocolMessage::CGenericProtocolMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_OBJECT)
            return;

        m_object = *rc<const uint32_t*>(&data.at(offset + 2));

        if (data.at(offset + 6) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        m_method = *rc<const uint32_t*>(&data.at(offset + 7));

        m_dataSpan = std::span<const uint8_t>{data.begin() + 11, data.size() - 11};

        size_t i = 11;
        while (data.at(offset + i) != HW_MESSAGE_MAGIC_END) {
            switch (sc<eMessageMagic>(data.at(offset + i))) {
                case HW_MESSAGE_MAGIC_TYPE_UINT:
                case HW_MESSAGE_MAGIC_TYPE_F32:
                case HW_MESSAGE_MAGIC_TYPE_INT:
                case HW_MESSAGE_MAGIC_TYPE_OBJECT:
                case HW_MESSAGE_MAGIC_TYPE_SEQ: i += 5; break;
                case HW_MESSAGE_MAGIC_TYPE_VARCHAR:
                    auto [a, b] = g_messageParser->parseVarInt(data, offset + i + 1);
                    i += a + b + 1;
                    break;
                    // FIXME: arr
            }
        }

        m_len = i + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CGenericProtocolMessage::CGenericProtocolMessage(std::vector<uint8_t>&& data) {
    m_data = std::move(data);
    m_type = HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE;
}
