#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>
#include <vector>
#include <span>

#include "ClientSocket.hpp"
#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class CClientSocket;

    class CClientObject : public IObject {
      public:
        CClientObject(SP<CClientSocket> client);
        virtual ~CClientObject() = default;

        virtual uint32_t        call(uint32_t id, ...);
        virtual void            listen(uint32_t id, void* fn);
        virtual void            called(uint32_t id, const std::span<const uint8_t>& data);

        std::vector<void*>      m_listeners;

        WP<CClientSocket>       m_client;
        uint32_t                m_id = 0, m_seq = 0;

        SP<IProtocolObjectSpec> m_spec;
    };
};
