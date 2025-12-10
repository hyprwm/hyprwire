#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CNewObjectMessage : public IMessage {
      public:
        CNewObjectMessage(const std::vector<uint8_t>& data, size_t offset);
        CNewObjectMessage(uint32_t seq, uint32_t id);

        virtual ~CNewObjectMessage() = default;

        uint32_t m_seq = 0, m_id = 0;
    };
};