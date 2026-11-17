#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

#include "core/server/ServerClient.hpp"

#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprwire;
using namespace Hyprutils::OS;

TEST(PeerCredentials, CapturedBeforeHandshakeAndNewClientHandler) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    CFileDescriptor serverFD{fds[0]}, peerFD{fds[1]};

    auto            server = IServerSocket::open();
    ASSERT_NE(server, nullptr);

    bool notified = false;
    server->setNewClientHandler([&](auto client) {
        const auto CREDENTIALS = client->getPeerCredentials();
        ASSERT_TRUE(CREDENTIALS.has_value());
        EXPECT_EQ(CREDENTIALS->pid, getpid());
        EXPECT_EQ(CREDENTIALS->uid, geteuid());
        EXPECT_EQ(CREDENTIALS->gid, getegid());
        notified = true;
    });

    auto client = server->addClient(serverFD.take());
    ASSERT_NE(client, nullptr);
    const auto CREDENTIALS = client->getPeerCredentials();
    ASSERT_TRUE(CREDENTIALS.has_value());
    EXPECT_EQ(CREDENTIALS->pid, getpid());
    EXPECT_EQ(CREDENTIALS->uid, geteuid());
    EXPECT_EQ(CREDENTIALS->gid, getegid());
    EXPECT_EQ(client->getPID(), CREDENTIALS->pid);
    EXPECT_FALSE(notified);

    auto peer = IClientSocket::open(peerFD.take());
    ASSERT_NE(peer, nullptr);
    ASSERT_TRUE(server->dispatchEvents(false));
    EXPECT_TRUE(notified);
}

TEST(PeerCredentials, SnapshotSurvivesDescriptorClosure) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    CFileDescriptor peerFD{fds[1]};
    CServerClient   client{fds[0]};

    client.m_fd.reset();

    const auto CREDENTIALS = client.getPeerCredentials();
    ASSERT_TRUE(CREDENTIALS.has_value());
    EXPECT_EQ(CREDENTIALS->pid, getpid());
    EXPECT_EQ(CREDENTIALS->uid, geteuid());
    EXPECT_EQ(CREDENTIALS->gid, getegid());
    EXPECT_EQ(client.getPID(), CREDENTIALS->pid);
}

TEST(PeerCredentials, MissingCredentialsRemainUnknown) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(pipe(fds), 0);
    CFileDescriptor writer{fds[1]};
    CServerClient   client{fds[0]};

    EXPECT_FALSE(client.getPeerCredentials().has_value());
    EXPECT_EQ(client.getPID(), -1);
}
