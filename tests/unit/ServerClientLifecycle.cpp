#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

#include "core/server/ServerClient.hpp"
#include "core/server/ServerObject.hpp"
#include "core/server/ServerSocket.hpp"

#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprwire;
using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

class CServerClientLifecycleTest : public testing::Test {
  protected:
    void SetUp() override {
        int fds[2] = {-1, -1};
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
        CFileDescriptor serverFD{fds[0]};
        m_peer     = CFileDescriptor{fds[1]};
        m_serverFD = serverFD.get();

        m_server = IServerSocket::open();
        ASSERT_NE(m_server, nullptr);
        m_client = m_server->addClient(serverFD.take());
        ASSERT_NE(m_client, nullptr);
        m_client->setDisconnectHandler([this] {
            ++m_disconnections;
            EXPECT_FALSE(reinterpretPointerCast<CServerClient>(m_client)->m_fd.isValid());
            m_events.emplace_back("client");
        });
    }

    void TearDown() override {
        m_server.reset();
        m_client.reset();
    }

    void pollFailure(short events) {
        auto server                                         = reinterpretPointerCast<CServerSocket>(m_server);
        server->m_pollfds.at(server->internalFds()).revents = events;
        EXPECT_TRUE(server->dispatchExistingConnections());
        EXPECT_EQ(m_disconnections, 1);
        EXPECT_TRUE(server->m_clients.empty());
    }

    int                           m_serverFD       = -1;
    int                           m_disconnections = 0;
    std::vector<std::string>      m_events;
    CFileDescriptor               m_peer;
    CSharedPointer<IServerSocket> m_server;
    CSharedPointer<IServerClient> m_client;
};

TEST_F(CServerClientLifecycleTest, ExplicitRemovalClosesRetainedClientExactlyOnce) {
    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_disconnections, 1);
    EXPECT_EQ(m_client->getPID(), getpid());

    char byte = 0;
    EXPECT_EQ(recv(m_peer.get(), &byte, 1, MSG_DONTWAIT), 0);
    EXPECT_FALSE(m_server->removeClient(m_serverFD));
    EXPECT_TRUE(m_server->dispatchEvents(false));
    m_client.reset();
    m_server.reset();
    EXPECT_EQ(m_disconnections, 1);
}

TEST_F(CServerClientLifecycleTest, PeerClosureNotifiesWithoutDroppingClientReference) {
    m_peer.reset();
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
}

TEST_F(CServerClientLifecycleTest, PeerWriteShutdownNotifiesOnEof) {
    ASSERT_EQ(shutdown(m_peer.get(), SHUT_WR), 0);
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
}

TEST_F(CServerClientLifecycleTest, MalformedMessageNotifiesExactlyOnce) {
    const uint8_t bytes[] = {0xFF, 0};
    ASSERT_EQ(write(m_peer.get(), bytes, sizeof(bytes)), sc<ssize_t>(sizeof(bytes)));
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
}

TEST_F(CServerClientLifecycleTest, HangupWithoutReadableDataNotifies) {
    pollFailure(POLLHUP);
}

TEST_F(CServerClientLifecycleTest, PollErrorNotifies) {
    pollFailure(POLLERR);
}

TEST_F(CServerClientLifecycleTest, InvalidDescriptorNotifies) {
    pollFailure(POLLNVAL);
}

TEST_F(CServerClientLifecycleTest, ServerDestructionNotifiesRetainedClient) {
    m_server.reset();
    EXPECT_EQ(m_disconnections, 1);
    m_client.reset();
    EXPECT_EQ(m_disconnections, 1);
}

TEST_F(CServerClientLifecycleTest, WireObjectsAreTornDownBeforeNotification) {
    auto client    = reinterpretPointerCast<CServerClient>(m_client);
    auto object    = makeShared<CServerObject>(client);
    object->m_self = object;
    client->m_objects.emplace_back(object);
    object->setOnDestroy([this] {
        m_events.emplace_back("object");
        EXPECT_FALSE(m_server->removeClient(m_serverFD));
    });

    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_events, (std::vector<std::string>{"object", "client"}));
    EXPECT_TRUE(object->m_destroyed);
    object.reset();
    EXPECT_EQ(m_events.size(), 2u);
}

TEST_F(CServerClientLifecycleTest, DisconnectHandlerCanRemoveAnotherClient) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    CFileDescriptor serverFD{fds[0]}, peer{fds[1]};
    auto            other = m_server->addClient(serverFD.take());
    ASSERT_NE(other, nullptr);
    other->setDisconnectHandler([this] { m_events.emplace_back("other"); });
    m_client->setDisconnectHandler([this, fd = fds[0]] {
        m_events.emplace_back("first");
        EXPECT_TRUE(m_server->removeClient(fd));
        EXPECT_FALSE(m_server->removeClient(m_serverFD));
    });

    m_peer.reset();
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_events, (std::vector<std::string>{"first", "other"}));
}

TEST_F(CServerClientLifecycleTest, NewClientHandlerCanRejectTheClient) {
    m_server->setNewClientHandler([this](auto) { EXPECT_TRUE(m_server->removeClient(m_serverFD)); });
    auto peer = IClientSocket::open(m_peer.take());
    ASSERT_NE(peer, nullptr);

    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_disconnections, 1);
    EXPECT_FALSE(peer->dispatchEvents(false));
}

TEST_F(CServerClientLifecycleTest, LateHandlerIsInvokedImmediately) {
    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_disconnections, 1);
    m_client->setDisconnectHandler([this] { m_events.emplace_back("late"); });
    EXPECT_EQ(m_events, (std::vector<std::string>{"client", "late"}));
    m_client.reset();
    EXPECT_EQ(m_events.size(), 2u);
}

TEST_F(CServerClientLifecycleTest, HandlerRegisteredDuringTeardownWaitsForObjects) {
    auto client       = reinterpretPointerCast<CServerClient>(m_client);
    auto first        = makeShared<CServerObject>(client);
    auto second       = makeShared<CServerObject>(client);
    client->m_objects = {first, second};
    first->setOnDestroy([&] {
        client->disconnect();
        client->setDisconnectHandler([this] { m_events.emplace_back("replacement"); });
        EXPECT_TRUE(m_events.empty());
    });
    second->setOnDestroy([this] { m_events.emplace_back("object"); });

    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_events, (std::vector<std::string>{"object", "replacement"}));
}

TEST_F(CServerClientLifecycleTest, HandlerCanBeCleared) {
    m_client->setDisconnectHandler({});
    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_disconnections, 0);
}

TEST_F(CServerClientLifecycleTest, RemovalHandlerCanReleaseServer) {
    m_client->setDisconnectHandler([this] { m_server.reset(); });
    ASSERT_TRUE(m_server->removeClient(m_serverFD));
    EXPECT_EQ(m_server, nullptr);
}

TEST_F(CServerClientLifecycleTest, DispatchHandlerCanReleaseServer) {
    m_client->setDisconnectHandler([this] { m_server.reset(); });
    m_peer.reset();
    ASSERT_TRUE(m_server->dispatchEvents(false));
    EXPECT_EQ(m_server, nullptr);
}

TEST_F(CServerClientLifecycleTest, ServerDestructionRejectsAndClosesReentrantAddition) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    CFileDescriptor               serverFD{fds[0]}, peer{fds[1]};
    CSharedPointer<IServerClient> added;
    m_client->setDisconnectHandler([&, server = m_server.get()] { added = server->addClient(serverFD.take()); });

    m_server.reset();
    EXPECT_EQ(added, nullptr);
    char byte = 0;
    EXPECT_EQ(recv(peer.get(), &byte, 1, MSG_DONTWAIT), 0);
}
