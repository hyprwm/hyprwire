#include "NewObject.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CNewObjectMessage::CNewObjectMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_NEW_OBJECT;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_NEW_OBJECT)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        m_id = *rc<const uint32_t*>(&data.at(offset + 2));

        if (data.at(offset + 6) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        m_seq = *rc<const uint32_t*>(&data.at(offset + 7));

        if (data.at(offset + 11) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = 12;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CNewObjectMessage::CNewObjectMessage(uint32_t seq, uint32_t id) {
    m_type = HW_MESSAGE_TYPE_NEW_OBJECT;

    m_data = {
        HW_MESSAGE_TYPE_NEW_OBJECT, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_END,
    };

    *rc<uint32_t*>(&m_data[2]) = seq;
    *rc<uint32_t*>(&m_data[7]) = seq;
}
