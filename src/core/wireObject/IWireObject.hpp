#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>
#include <span>
#include <vector>
#include <cstdint>

#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class CGenericProtocolMessage;

    class IWireObject : public IObject {
      public:
        virtual ~IWireObject() = default;

        virtual uint32_t                    call(uint32_t id, ...);
        virtual void                        listen(uint32_t id, void* fn);
        virtual void                        called(uint32_t id, const std::span<const uint8_t>& data);
        virtual const std::vector<SMethod>& methodsOut()                             = 0;
        virtual const std::vector<SMethod>& methodsIn()                              = 0;
        virtual void                        errd()                                   = 0;
        virtual void                        sendMessage(SP<CGenericProtocolMessage>) = 0;

        std::vector<void*>                  m_listeners;
        uint32_t                            m_id = 0;
        std::string                         m_protocolName;

        SP<IProtocolObjectSpec>             m_spec;

      protected:
        IWireObject() = default;
    };
};