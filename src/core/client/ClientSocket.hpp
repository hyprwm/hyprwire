#pragma once

#include <hyprwire/core/ClientSocket.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../../helpers/Memory.hpp"

#include <vector>
#include <sys/poll.h>

namespace Hyprwire {
    class IMessage;
    class CClientObject;
    class CGenericProtocolMessage;

    class CClientSocket : public IClientSocket {
      public:
        CClientSocket()          = default;
        virtual ~CClientSocket() = default;

        bool                                           attempt(const std::string& path);

        virtual void                                   addImplementation(SP<IProtocolClientImplementation>&&);
        virtual bool                                   dispatchEvents(bool block);
        virtual int                                    extractLoopFD();
        virtual bool                                   waitForHandshake();
        virtual SP<IProtocolSpec>                      getSpec(const std::string& name);
        virtual SP<IObject>                            bindProtocol(const SP<IProtocolSpec>& spec, uint32_t version);
        virtual SP<IObject>                            objectForId(uint32_t id);

        void                                           sendMessage(const SP<IMessage>& message);
        void                                           serverSpecs(const std::vector<std::string>& s);
        void                                           recheckPollFds();
        void                                           onSeq(uint32_t seq, uint32_t id);
        void                                           onGeneric(SP<CGenericProtocolMessage> msg);
        SP<CClientObject>                              makeObject(const std::string& protocolName, const std::string& objectName, uint32_t seq);
        void                                           waitForObject(SP<CClientObject>);

        Hyprutils::OS::CFileDescriptor                 m_fd;
        std::vector<SP<IProtocolClientImplementation>> m_impls;
        std::vector<SP<IProtocolSpec>>                 m_serverSpecs;
        std::vector<pollfd>                            m_pollfds;
        std::vector<SP<CClientObject>>                 m_objects;

        bool                                           m_error         = false;
        bool                                           m_handshakeDone = false;

        std::chrono::steady_clock::time_point          m_handshakeBegin;

        WP<CClientSocket>                              m_self;
        uint32_t                                       m_seq = 0;
    };
};