#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"
#include "../../../helpers/Memory.hpp"

namespace Hyprwire {
    class IWireObject;

    class CRoundtripDoneMessage : public IMessage {
      public:
        CRoundtripDoneMessage(const std::vector<uint8_t>& data, size_t offset);
        CRoundtripDoneMessage(uint32_t seq);

        virtual ~CRoundtripDoneMessage() = default;

        uint32_t m_seq = 0;
    };
};