#pragma once

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>

#include "ClientSocket.hpp"
#include "../../helpers/Memory.hpp"
#include "../wireObject/IWireObject.hpp"

namespace Hyprwire {
    class CClientSocket;

    class CClientObject : public IWireObject {
      public:
        CClientObject(SP<CClientSocket> client);
        virtual ~CClientObject() = default;

        virtual const std::vector<SMethod>& methodsOut();
        virtual const std::vector<SMethod>& methodsIn();
        virtual void                        errd();
        virtual void                        sendMessage(SP<CGenericProtocolMessage>);

        WP<CClientSocket>                   m_client;
        uint32_t                            m_seq = 0;
    };
};
