#pragma once

#include <sys/poll.h>
#include <sys/socket.h>

namespace Hyprwire::Syscalls {
    using PFN_poll    = int (*)(pollfd* fds, nfds_t nfds, int timeout);
    using PFN_sendmsg = ssize_t (*)(int sockfd, const msghdr* msg, int flags);
    using PFN_recvmsg = ssize_t (*)(int sockfd, msghdr* msg, int flags);

    struct SHooks {
        PFN_poll    poll    = nullptr;
        PFN_sendmsg sendmsg = nullptr;
        PFN_recvmsg recvmsg = nullptr;
    };

    int     poll(pollfd* fds, nfds_t nfds, int timeout);
    ssize_t sendmsg(int sockfd, const msghdr* msg, int flags);
    ssize_t recvmsg(int sockfd, msghdr* msg, int flags);

    void    setHooks(const SHooks& hooks);
    void    resetHooks();
};
