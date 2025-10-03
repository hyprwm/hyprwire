#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CHandshakeBeginMessage : public IMessage {
      public:
        CHandshakeBeginMessage(const std::vector<uint8_t>& data, size_t offset);
        CHandshakeBeginMessage(const std::vector<uint32_t>& versions);
        ~CHandshakeBeginMessage() = default;

        size_t                m_len = 0;
        std::vector<uint32_t> m_versionsSupported;
    };
};