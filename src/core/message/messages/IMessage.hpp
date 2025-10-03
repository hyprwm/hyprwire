#pragma once

#include <vector>
#include <cstdint>

#include "../MessageType.hpp"

namespace Hyprwire {
    class IMessage {
      public:
        IMessage()  = default;
        ~IMessage() = default;

        std::vector<uint8_t> m_data;
        eMessageType         m_type = HW_MESSAGE_TYPE_INVALID;
    };
};