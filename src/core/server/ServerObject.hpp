#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>
#include <vector>

#include "ServerClient.hpp"
#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class CServerClient;

    class CServerObject : public IObject {
      public:
        CServerObject(SP<CServerClient> client);
        virtual ~CServerObject() = default;

        virtual uint32_t        call(uint32_t id, ...);
        virtual void            listen(uint32_t id, void* fn);

        std::vector<void*>      m_listeners;

        WP<CServerClient>       m_client;
        uint32_t                m_id = 0;
        std::string             m_protocolName;

        SP<IProtocolObjectSpec> m_spec;
    };
};
