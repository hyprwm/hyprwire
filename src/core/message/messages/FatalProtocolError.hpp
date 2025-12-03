#pragma once

#include <vector>
#include <cstdint>

#include "IMessage.hpp"
#include "../../../helpers/Memory.hpp"

namespace Hyprwire {
    class IWireObject;

    class CFatalErrorMessage : public IMessage {
      public:
        CFatalErrorMessage(const std::vector<uint8_t>& data, size_t offset);
        CFatalErrorMessage(SP<IWireObject> obj, uint32_t errorId, const std::string_view& msg);
        ~CFatalErrorMessage() = default;

        uint32_t    m_objectId = 0;
        uint32_t    m_errorId  = 0;
        std::string m_errorMsg;
    };
};