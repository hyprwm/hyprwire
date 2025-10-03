#pragma once

#include <hyprwire/core/ClientSocket.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../../helpers/Memory.hpp"

#include <vector>
#include <sys/poll.h>

namespace Hyprwire {
    class IMessage;

    class CClientSocket : public IClientSocket {
      public:
        CClientSocket()          = default;
        virtual ~CClientSocket() = default;

        bool                                           attempt(const std::string& path);

        virtual void                                   addImplementation(SP<IProtocolClientImplementation>&&);
        virtual bool                                   dispatchEvents(bool block);
        virtual int                                    extractLoopFD();

        void                                           sendMessage(const SP<IMessage>& message);

        void                                           recheckPollFds();

        Hyprutils::OS::CFileDescriptor                 m_fd;
        std::vector<SP<IProtocolClientImplementation>> m_impls;
        std::vector<pollfd>                            m_pollfds;

        bool                                           m_error = false;

        WP<CClientSocket>                              m_self;
    };
};