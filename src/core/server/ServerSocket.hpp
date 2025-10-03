#pragma once

#include <hyprwire/core/ServerSocket.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../../helpers/Memory.hpp"

#include <vector>
#include <sys/poll.h>

namespace Hyprwire {
    class CServerClient;

    class CServerSocket : public IServerSocket {
      public:
        CServerSocket() = default;
        virtual ~CServerSocket();

        bool                                     attempt(const std::string& path);

        virtual void                             addImplementation(SP<IProtocolImplementation>&&);
        virtual bool                             dispatchEvents(bool block);
        virtual int                              extractLoopFD();

        void                                     recheckPollFds();
        void                                     dispatchNewConnections();
        void                                     dispatchExistingConnections();
        void                                     dispatchClient(SP<CServerClient> client);

        std::vector<SP<IProtocolImplementation>> m_impls;

        Hyprutils::OS::CFileDescriptor           m_fd;

        std::vector<pollfd>                      m_pollfds;
        std::vector<SP<CServerClient>>           m_clients;

        WP<CServerSocket>                        m_self;

        bool                                     m_success = false;
        std::string                              m_path;
    };
};