#pragma once

#include <hyprwire/core/ServerSocket.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../../helpers/Memory.hpp"

#include <vector>
#include <thread>
#include <sys/poll.h>

namespace Hyprwire {
    class CServerClient;

    class CServerSocket : public IServerSocket {
      public:
        CServerSocket() = default;
        virtual ~CServerSocket();

        bool                                           attempt(const std::string& path);
        bool                                           attemptFromFd(const int fd);

        virtual void                                   addImplementation(SP<IProtocolServerImplementation>&&);
        virtual bool                                   dispatchEvents(bool block);
        virtual int                                    extractLoopFD();
        virtual SP<IObject>                            createObject(SP<IServerClient> client, SP<IObject> reference, const std::string& object, uint32_t seq);

        void                                           recheckPollFds();
        bool                                           dispatchNewConnections();
        bool                                           dispatchExistingConnections();
        bool                                           dispatchPending();
        void                                           dispatchClient(SP<CServerClient> client);
        void                                           clearEventFd();
        size_t                                         internalFds();

        std::vector<SP<IProtocolServerImplementation>> m_impls;

        Hyprutils::OS::CFileDescriptor                 m_fd;
        Hyprutils::OS::CFileDescriptor                 m_exportFd, m_exportWriteFd;
        Hyprutils::OS::CFileDescriptor                 m_exitFd, m_exitWriteFd;

        std::vector<pollfd>                            m_pollfds;
        std::vector<SP<CServerClient>>                 m_clients;

        WP<CServerSocket>                              m_self;

        bool                                           m_threadCanPoll = false;
        std::thread                                    m_pollThread;
        std::mutex                                     m_pollmtx;

        bool                                           m_success      = false;
        bool                                           m_isFdListener = false;
        std::string                                    m_path;
    };
};