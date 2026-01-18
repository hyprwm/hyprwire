#include "HandshakeBegin.hpp"
#include "../MessageType.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Env.hpp"

#include <cstring>
#include <stdexcept>
#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

CHandshakeBeginMessage::CHandshakeBeginMessage(const std::vector<uint8_t>& data, size_t offset) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_BEGIN;

    try {
        if (data.at(offset + 0) != HW_MESSAGE_TYPE_HANDSHAKE_BEGIN)
            return;

        if (data.at(offset + 1) != HW_MESSAGE_MAGIC_TYPE_ARRAY)
            return;

        if (data.at(offset + 2) != HW_MESSAGE_MAGIC_TYPE_UINT)
            return;

        size_t needle = 3;

        auto [nVers, varIntLen] = g_messageParser->parseVarInt(data, offset + needle);

        needle += varIntLen;

        m_versionsSupported.resize(nVers);

        for (size_t i = 0; i < nVers; ++i) {
            std::memcpy(&m_versionsSupported[i], &data.at(offset + needle + (i * sizeof(uint32_t))), sizeof(m_versionsSupported[0]));
        }

        needle += (nVers * sizeof(uint32_t));

        if (data.at(offset + needle) != HW_MESSAGE_MAGIC_END)
            return;

        m_len = needle + 1;

        if (Env::isTrace())
            m_data = std::vector<uint8_t>{data.begin() + offset, data.begin() + offset + m_len - 1};

    } catch (std::out_of_range& e) { m_len = 0; }
}

CHandshakeBeginMessage::CHandshakeBeginMessage(const std::vector<uint32_t>& versions) {
    m_type = HW_MESSAGE_TYPE_HANDSHAKE_BEGIN;

    m_data.reserve(8 + (versions.size() * 4));

    m_data = {
        HW_MESSAGE_TYPE_HANDSHAKE_BEGIN,
        HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_UINT,
    };

    m_data.append_range(g_messageParser->encodeVarInt(versions.size()));

    const size_t HEAD_SIZE = m_data.size();

    m_data.resize(HEAD_SIZE + (versions.size() * 4));

    for (size_t i = 0; i < versions.size(); ++i) {
        *rc<uint32_t*>(&m_data.at(HEAD_SIZE + (i * 4))) = versions.at(i);
    }

    m_data.emplace_back(HW_MESSAGE_MAGIC_END);
}
