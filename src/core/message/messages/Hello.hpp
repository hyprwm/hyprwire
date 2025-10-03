#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CHelloMessage : public IMessage {
      public:
        CHelloMessage(const std::vector<uint8_t>& data, size_t offset);
        CHelloMessage();
        ~CHelloMessage() = default;
    };
};