#include <gtest/gtest.h>

#include "core/client/ClientSocket.hpp"
#include "core/message/MessageType.hpp"
#include "core/socket/SocketHelpers.hpp"
#include "helpers/Syscalls.hpp"

#include <hyprwire/hyprwire.hpp>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>

#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprwire;

namespace {
    class CScopedSyscallHooks {
      public:
        explicit CScopedSyscallHooks(const Syscalls::SHooks& hooks) {
            Syscalls::setHooks(hooks);
        }

        ~CScopedSyscallHooks() {
            Syscalls::resetHooks();
        }
    };

    class CScopedHandshakeTimeout {
      public:
        explicit CScopedHandshakeTimeout(std::chrono::milliseconds timeout) {
            CClientSocket::setHandshakeTimeoutForTests(timeout);
        }

        ~CScopedHandshakeTimeout() {
            CClientSocket::resetHandshakeTimeoutForTests();
        }
    };

    int     g_sendmsgCalls = 0;
    int     g_pollCalls    = 0;
    ssize_t hookSendmsgRetryEagain(int, const msghdr* msg, int) {
        ++g_sendmsgCalls;
        if (g_sendmsgCalls == 1) {
            errno = EAGAIN;
            return -1;
        }

        return static_cast<ssize_t>(msg->msg_iov[0].iov_len);
    }

    int hookPollAwake(pollfd*, nfds_t, int) {
        ++g_pollCalls;
        return 1;
    }

    int hookPollTimeout(pollfd*, nfds_t, int) {
        return 0;
    }

    ssize_t hookRecvmsgError(int, msghdr*, int) {
        errno = EIO;
        return -1;
    }

    ssize_t hookRecvmsgInvalidControl(int, msghdr* msg, int) {
        auto* io = msg->msg_iov;
        if (io && io->iov_len > 0) {
            auto* bytes = static_cast<uint8_t*>(io->iov_base);
            bytes[0]    = HW_MESSAGE_TYPE_SUP;
        }

        auto* cmsg = CMSG_FIRSTHDR(msg);
        if (!cmsg)
            return -1;

        cmsg->cmsg_level                         = SOL_SOCKET;
        cmsg->cmsg_type                          = 0x7FFF;
        cmsg->cmsg_len                           = CMSG_LEN(sizeof(int));
        *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = 123;

        msg->msg_controllen = cmsg->cmsg_len;

        return 1;
    }
}

TEST(SyscallSeams, ClientSendRetriesOnEagain) {
    g_sendmsgCalls = 0;
    g_pollCalls    = 0;

    CScopedSyscallHooks hooks(Syscalls::SHooks{
        .poll    = hookPollAwake,
        .sendmsg = hookSendmsgRetryEagain,
        .recvmsg = nullptr,
    });

    int                 fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    auto client = IClientSocket::open(fds[0]);
    ASSERT_NE(client, nullptr);

    close(fds[1]);

    EXPECT_GE(g_sendmsgCalls, 2);
    EXPECT_GE(g_pollCalls, 1);
}

TEST(SyscallSeams, HandshakeTimeoutCanBeOverridden) {
    CScopedSyscallHooks hooks(Syscalls::SHooks{
        .poll    = hookPollTimeout,
        .sendmsg = nullptr,
        .recvmsg = nullptr,
    });

    int                 fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    CScopedHandshakeTimeout timeout{std::chrono::milliseconds(1)};

    auto                    client = IClientSocket::open(fds[0]);
    ASSERT_NE(client, nullptr);

    EXPECT_FALSE(client->waitForHandshake());
    EXPECT_FALSE(client->isHandshakeDone());

    close(fds[1]);
}

TEST(SyscallSeams, ParseFromFdReturnsBadWhenRecvmsgFails) {
    CScopedSyscallHooks hooks(Syscalls::SHooks{
        .poll    = nullptr,
        .sendmsg = nullptr,
        .recvmsg = hookRecvmsgError,
    });

    int                 fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    Hyprutils::OS::CFileDescriptor fd{fds[0]};
    close(fds[1]);

    const auto parsed = parseFromFd(fd);
    EXPECT_TRUE(parsed.bad);
}

TEST(SyscallSeams, ParseFromFdReturnsBadForInvalidControlMessage) {
    CScopedSyscallHooks hooks(Syscalls::SHooks{
        .poll    = nullptr,
        .sendmsg = nullptr,
        .recvmsg = hookRecvmsgInvalidControl,
    });

    int                 fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    Hyprutils::OS::CFileDescriptor fd{fds[0]};
    close(fds[1]);

    const auto parsed = parseFromFd(fd);
    EXPECT_TRUE(parsed.bad);
}
