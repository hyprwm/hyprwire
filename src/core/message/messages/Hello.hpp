#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"

namespace Hyprwire {
    class CHelloMessage : public IMessage {
      public:
        CHelloMessage();
        ~CHelloMessage() = default;
    };
};