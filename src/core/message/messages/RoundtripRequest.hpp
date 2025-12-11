#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"
#include "../../../helpers/Memory.hpp"

namespace Hyprwire {
    class IWireObject;

    class CRoundtripRequestMessage : public IMessage {
      public:
        CRoundtripRequestMessage(const std::vector<uint8_t>& data, size_t offset);
        CRoundtripRequestMessage(uint32_t seq);

        virtual ~CRoundtripRequestMessage() = default;

        uint32_t m_seq = 0;
    };
};