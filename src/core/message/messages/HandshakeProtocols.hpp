#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CHandshakeProtocolsMessage : public IMessage {
      public:
        CHandshakeProtocolsMessage(const std::vector<uint8_t>& data, size_t offset);
        CHandshakeProtocolsMessage(const std::vector<std::string>& protocols);
        ~CHandshakeProtocolsMessage() = default;

        std::vector<std::string> m_protocols;
    };
};