
#include <hyprutils/os/FileDescriptor.hpp>
#include <cstdint>
#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class IMessage;
    class CServerSocket;

    class CServerClient {
      public:
        CServerClient(int fd);
        ~CServerClient() = default;

        void                           sendMessage(const SP<IMessage>& message);

        Hyprutils::OS::CFileDescriptor m_fd;

        uint32_t                       m_version = 0;
        bool                           m_error   = false;

        WP<CServerSocket>              m_server;
    };
};