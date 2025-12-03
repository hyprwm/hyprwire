#pragma once

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprwire/core/ServerSocket.hpp>
#include <cstdint>
#include <vector>
#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class IMessage;
    class CServerSocket;
    class CServerObject;
    class CGenericProtocolMessage;

    class CServerClient : public IServerClient {
      public:
        CServerClient(int fd);
        virtual ~CServerClient();

        void                           sendMessage(const IMessage& message);
        SP<CServerObject>              createObject(const std::string& protocol, const std::string& object, uint32_t version, uint32_t seq);
        void                           onBind(SP<CServerObject> obj);
        void                           onGeneric(const CGenericProtocolMessage& msg);

        Hyprutils::OS::CFileDescriptor m_fd;

        uint32_t                       m_version = 0, m_maxId = 1;
        bool                           m_error = false;

        std::vector<SP<CServerObject>> m_objects;

        WP<CServerSocket>              m_server;
        WP<CServerClient>              m_self;
    };
};