#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>

#include "ServerClient.hpp"
#include "../../helpers/Memory.hpp"
#include "../wireObject/IWireObject.hpp"

namespace Hyprwire {
    class CServerClient;

    class CServerObject : public IWireObject {
      public:
        CServerObject(SP<CServerClient> client);
        virtual ~CServerObject() = default;

        virtual const std::vector<SMethod>& methodsOut();
        virtual const std::vector<SMethod>& methodsIn();
        virtual void                        errd();
        virtual void                        sendMessage(SP<CGenericProtocolMessage>);

        WP<CServerClient>                   m_client;
    };
};
