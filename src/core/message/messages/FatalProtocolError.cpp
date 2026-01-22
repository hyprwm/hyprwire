#include "FatalProtocolError.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../wireObject/IWireObject.hpp"
#include "../../../helpers/Env.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CFatalErrorMessage::CFatalErrorMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_FATAL_PROTOCOL_ERROR;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_FATAL_PROTOCOL_ERROR)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        std::memcpy(&m_objectId, &data.at(offset + 2), sizeof(m_objectId));

        if (data.at(offset + 6) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        std::memcpy(&m_errorId, &data.at(offset + 7), sizeof(m_errorId));

        if (data.at(offset + 11) != HW_MESSAGE_MAGIC_TYPE_VARCHAR)
            return;

        size_t needle = 12;

        auto [strLen, strLenLen] = g_messageParser->parseVarInt(data, offset + needle);

        needle += strLenLen;

        m_errorMsg = std::string{rc<const char*>(data.data() + offset + needle), strLen};

        needle += strLen;

        if (data.at(offset + needle) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = needle + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CFatalErrorMessage::CFatalErrorMessage(SP<IWireObject> obj, uint32_t errorId, const std::string_view& msg) {
    m_type = HW_MESSAGE_TYPE_FATAL_PROTOCOL_ERROR;

    m_data = {HW_MESSAGE_TYPE_FATAL_PROTOCOL_ERROR, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_TYPE_UINT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_TYPE_VARCHAR};

    if (obj)
        std::memcpy(&m_data[2], &obj->m_id, sizeof(obj->m_id));
    std::memcpy(&m_data[7], &errorId, sizeof(errorId));

    m_data.append_range(g_messageParser->encodeVarInt(msg.size()));
    m_data.append_range(msg);
    m_data.emplace_back(HW_MESSAGE_MAGIC_END);
}
