#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CGenericProtocolMessage : public IMessage {
      public:
        CGenericProtocolMessage(const std::vector<uint8_t>& data, size_t offset);
        CGenericProtocolMessage(std::vector<uint8_t>&& data);
        ~CGenericProtocolMessage() = default;

        uint32_t                 m_object = 0;
        uint32_t                 m_method = 0;

        std::span<const uint8_t> m_dataSpan;
    };
};