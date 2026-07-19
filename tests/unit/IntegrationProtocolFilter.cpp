#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>

#include "../../src/core/message/messages/BindProtocol.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprwire;
using namespace Hyprutils::Memory;

template <typename T>
using SP = CSharedPointer<T>;

static constexpr std::string_view ALLOWED_PROTOCOL = "allowed_protocol";
static constexpr std::string_view BLOCKED_PROTOCOL = "blocked_protocol";

class CFilterObjectSpec final : public IProtocolObjectSpec {
  public:
    std::string objectName() override {
        return "manager";
    }

    const std::vector<SMethod>& c2s() override {
        static const std::vector<SMethod> methods = {};
        return methods;
    }

    const std::vector<SMethod>& s2c() override {
        static const std::vector<SMethod> methods = {};
        return methods;
    }
};

class CFilterProtocolSpec final : public IProtocolSpec {
  public:
    explicit CFilterProtocolSpec(std::string name) : m_name(std::move(name)) {
        ;
    }

    std::string specName() override {
        return m_name;
    }

    uint32_t specVer() override {
        return 1;
    }

    std::vector<SP<IProtocolObjectSpec>> objects() override {
        return {
            m_objectSpec,
        };
    }

  private:
    std::string           m_name;
    SP<CFilterObjectSpec> m_objectSpec = makeShared<CFilterObjectSpec>();
};

class CFilterServerImpl final : public IProtocolServerImplementation {
  public:
    explicit CFilterServerImpl(SP<IProtocolSpec> spec) : m_spec(std::move(spec)) {
        ;
    }

    SP<IProtocolSpec> protocol() override {
        return m_spec;
    }

    std::vector<SP<SServerObjectImplementation>> implementation() override {
        return {
            makeShared<SServerObjectImplementation>(SServerObjectImplementation{.objectName = "manager", .version = 1}),
        };
    }

  private:
    SP<IProtocolSpec> m_spec;
};

class CProtocolFilterHarness {
  public:
    using TNewClientHandler = std::function<void(SP<IServerClient>)>;

    explicit CProtocolFilterHarness(TNewClientHandler handler = {}) {
        int fds[2] = {-1, -1};
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
            throw std::runtime_error("socketpair failed");

        m_clientFd = fds[1];
        m_server   = IServerSocket::open();
        if (!m_server)
            throw std::runtime_error("server open failed");

        addFilterImplementations(m_server);

        if (handler)
            m_server->setNewClientHandler(std::move(handler));

        auto serverClient = m_server->addClient(fds[0]);
        if (!serverClient)
            throw std::runtime_error("server addClient failed");

        m_client = IClientSocket::open(fds[1]);
        if (!m_client)
            throw std::runtime_error("client open failed");

        m_pumpThread = std::thread([this] {
            while (!m_stop) {
                m_server->dispatchEvents(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    ~CProtocolFilterHarness() {
        m_stop = true;
        if (m_pumpThread.joinable())
            m_pumpThread.join();
    }

    bool waitForHandshake() {
        return m_client->waitForHandshake();
    }

    SP<IClientSocket> client() {
        return m_client;
    }

    bool sendRawBind(std::string_view protocol) {
        const CBindProtocolMessage msg(std::string{protocol}, 100, 1);
        return write(m_clientFd, msg.m_data.data(), msg.m_data.size()) == static_cast<ssize_t>(msg.m_data.size());
    }

    bool waitForClientDisconnect() {
        for (int i = 0; i < 500; ++i) {
            if (!m_client->dispatchEvents(false))
                return true;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return false;
    }

    static void addFilterImplementations(const SP<IServerSocket>& server) {
        auto allowedSpec = makeShared<CFilterProtocolSpec>(std::string{ALLOWED_PROTOCOL});
        auto blockedSpec = makeShared<CFilterProtocolSpec>(std::string{BLOCKED_PROTOCOL});

        server->addImplementation(makeShared<CFilterServerImpl>(std::move(allowedSpec)));
        server->addImplementation(makeShared<CFilterServerImpl>(std::move(blockedSpec)));
    }

  private:
    std::atomic<bool> m_stop = false;
    std::thread       m_pumpThread;

    SP<IServerSocket> m_server;
    SP<IClientSocket> m_client;

    int               m_clientFd = -1;
};

class CPathProtocolFilterHarness {
  public:
    explicit CPathProtocolFilterHarness(CProtocolFilterHarness::TNewClientHandler handler) {
        m_path   = makeSocketPath();
        m_server = IServerSocket::open(m_path);
        if (!m_server)
            throw std::runtime_error("server open(path) failed");

        CProtocolFilterHarness::addFilterImplementations(m_server);
        m_server->setNewClientHandler(std::move(handler));

        m_pumpThread = std::thread([this] {
            while (!m_stop) {
                m_server->dispatchEvents(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    ~CPathProtocolFilterHarness() {
        m_stop = true;
        if (m_pumpThread.joinable())
            m_pumpThread.join();

        m_server.reset();

        std::error_code ec;
        std::filesystem::remove(m_path, ec);
        std::filesystem::remove(std::filesystem::path(m_path).parent_path(), ec);
    }

    SP<IClientSocket> openClient() {
        return IClientSocket::open(m_path);
    }

  private:
    static std::string makeSocketPath() {
        const auto dir = std::filesystem::temp_directory_path() / ("hyprwire-filter-" + std::to_string(getpid()));
        std::filesystem::create_directories(dir);
        return (dir / "wire.sock").string();
    }

    std::string       m_path;
    std::atomic<bool> m_stop = false;
    std::thread       m_pumpThread;
    SP<IServerSocket> m_server;
};

static bool contains(const std::vector<std::string>& values, std::string_view value) {
    return std::ranges::find(values, value) != values.end();
}

TEST(IntegrationProtocolFilter, DefaultExposesAllProtocols) {
    CProtocolFilterHarness harness;

    ASSERT_TRUE(harness.waitForHandshake());
    EXPECT_NE(harness.client()->getSpec(std::string{ALLOWED_PROTOCOL}), nullptr);
    EXPECT_NE(harness.client()->getSpec(std::string{BLOCKED_PROTOCOL}), nullptr);
}

TEST(IntegrationProtocolFilter, FilterLimitsHandshakeExposureAndRunsOnce) {
    std::atomic<uint32_t>    handlerCalls = 0;
    std::mutex               filterNamesMutex;
    std::vector<std::string> filterNames;

    CProtocolFilterHarness   harness([&](SP<IServerClient> client) {
        handlerCalls.fetch_add(1);
        client->setProtocolFilter([&](std::string_view name) {
            std::scoped_lock lock(filterNamesMutex);
            filterNames.emplace_back(name);
            return name == ALLOWED_PROTOCOL;
        });
    });

    ASSERT_TRUE(harness.waitForHandshake());

    EXPECT_EQ(handlerCalls.load(), 1u);
    EXPECT_NE(harness.client()->getSpec(std::string{ALLOWED_PROTOCOL}), nullptr);
    EXPECT_EQ(harness.client()->getSpec(std::string{BLOCKED_PROTOCOL}), nullptr);

    std::scoped_lock lock(filterNamesMutex);
    EXPECT_TRUE(contains(filterNames, ALLOWED_PROTOCOL));
    EXPECT_TRUE(contains(filterNames, BLOCKED_PROTOCOL));
}

TEST(IntegrationProtocolFilter, AllFilteredHandshakeStillSucceeds) {
    CProtocolFilterHarness harness([](SP<IServerClient> client) { client->setProtocolFilter([](std::string_view) { return false; }); });

    ASSERT_TRUE(harness.waitForHandshake());
    EXPECT_EQ(harness.client()->getSpec(std::string{ALLOWED_PROTOCOL}), nullptr);
    EXPECT_EQ(harness.client()->getSpec(std::string{BLOCKED_PROTOCOL}), nullptr);
}

TEST(IntegrationProtocolFilter, FilteredProtocolCannotBeBound) {
    CProtocolFilterHarness harness([](SP<IServerClient> client) { client->setProtocolFilter([](std::string_view name) { return name == ALLOWED_PROTOCOL; }); });

    ASSERT_TRUE(harness.waitForHandshake());
    ASSERT_EQ(harness.client()->getSpec(std::string{BLOCKED_PROTOCOL}), nullptr);

    ASSERT_TRUE(harness.sendRawBind(BLOCKED_PROTOCOL));
    EXPECT_TRUE(harness.waitForClientDisconnect());
}

TEST(IntegrationProtocolFilter, AcceptedPathClientsUseNewClientFilter) {
    std::atomic<uint32_t>      handlerCalls = 0;

    CPathProtocolFilterHarness harness([&](SP<IServerClient> client) {
        handlerCalls.fetch_add(1);
        client->setProtocolFilter([](std::string_view name) { return name == ALLOWED_PROTOCOL; });
    });

    auto                       client = harness.openClient();
    ASSERT_NE(client, nullptr);

    ASSERT_TRUE(client->waitForHandshake());
    EXPECT_EQ(handlerCalls.load(), 1u);
    EXPECT_NE(client->getSpec(std::string{ALLOWED_PROTOCOL}), nullptr);
    EXPECT_EQ(client->getSpec(std::string{BLOCKED_PROTOCOL}), nullptr);
}
