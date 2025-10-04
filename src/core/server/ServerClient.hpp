#pragma once

#include <hyprutils/os/FileDescriptor.hpp>
#include <cstdint>
#include <vector>
#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class IMessage;
    class CServerSocket;
    class CServerObject;
    class CGenericProtocolMessage;

    class CServerClient {
      public:
        CServerClient(int fd);
        ~CServerClient() = default;

        void                           sendMessage(const SP<IMessage>& message);
        SP<CServerObject>              createObject(const std::string& protocol, const std::string& object, uint32_t version);
        void                           onBind(SP<CServerObject> obj);
        void                           onGeneric(SP<CGenericProtocolMessage> msg);

        Hyprutils::OS::CFileDescriptor m_fd;

        uint32_t                       m_version = 0, m_maxId = 1;
        bool                           m_error = false;

        std::vector<SP<CServerObject>> m_objects;

        WP<CServerSocket>              m_server;
        WP<CServerClient>              m_self;
    };
};