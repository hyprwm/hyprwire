#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>
#include <span>
#include <vector>
#include <cstdint>

#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class IMessage;

    class IWireObject : public IObject {
      public:
        virtual ~IWireObject();

        virtual uint32_t                    call(uint32_t id, ...);
        virtual void                        listen(uint32_t id, void* fn);
        virtual void                        called(uint32_t id, const std::span<const uint8_t>& data, const std::vector<int>& fds);
        virtual const std::vector<SMethod>& methodsOut()                 = 0;
        virtual const std::vector<SMethod>& methodsIn()                  = 0;
        virtual void                        errd()                       = 0;
        virtual void                        sendMessage(const IMessage&) = 0;
        virtual bool                        server()                     = 0;

        std::vector<void*>                  m_listeners;
        uint32_t                            m_id = 0, m_version = 0, m_seq = 1;
        std::string                         m_protocolName;

        SP<IProtocolObjectSpec>             m_spec;
        WP<IWireObject>                     m_self;

      protected:
        IWireObject() = default;
    };
};