#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CHandshakeAckMessage : public IMessage {
      public:
        CHandshakeAckMessage(const std::vector<uint8_t>& data, size_t offset);
        CHandshakeAckMessage(uint32_t version);

        virtual ~CHandshakeAckMessage() = default;

        uint32_t m_version = 0;
    };
};