#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CBindProtocolMessage : public IMessage {
      public:
        CBindProtocolMessage(const std::vector<uint8_t>& data, size_t offset);
        CBindProtocolMessage(const std::string& protocol, uint32_t seq, uint32_t version);
        ~CBindProtocolMessage() = default;

        std::string m_protocol;
        uint32_t    m_seq = 0, m_version = 0;
    };
};