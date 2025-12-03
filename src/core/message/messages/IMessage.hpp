#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <span>

#include "../MessageType.hpp"

namespace Hyprwire {
    class IMessage {
      public:
        IMessage()  = default;
        ~IMessage() = default;

        std::vector<uint8_t> m_data;
        eMessageType         m_type = HW_MESSAGE_TYPE_INVALID;
        size_t               m_len  = 0;

        std::string          parseData() const;
    };
};