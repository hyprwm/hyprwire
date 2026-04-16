#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>
#include <hyprwire/core/types/MessageMagic.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

using namespace Hyprwire;
using namespace Hyprutils::Memory;

namespace {

    template <typename T>
    using SP = CSharedPointer<T>;

    template <typename Fn>
    void* fnToVoid(Fn fn) {
        static_assert(std::is_pointer_v<Fn>);
        static_assert(sizeof(Fn) == sizeof(void*));

        union {
            Fn    f;
            void* p;
        } caster = {
            .f = fn,
        };

        return caster.p;
    }

    class CManagerSpec final : public IProtocolObjectSpec {
      public:
        std::string objectName() override {
            return "manager";
        }

        const std::vector<SMethod>& c2s() override {
            static const std::vector<SMethod> methods = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_VARCHAR}, .returnsType = "", .since = 0},
                SMethod{.idx = 1, .params = {}, .returnsType = "child", .since = 0},
                SMethod{.idx = 2, .params = {HW_MESSAGE_MAGIC_TYPE_FD}, .returnsType = "", .since = 0},
                SMethod{.idx = 3, .params = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
            };

            return methods;
        }

        const std::vector<SMethod>& s2c() override {
            static const std::vector<SMethod> methods = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_VARCHAR}, .returnsType = "", .since = 0},
            };

            return methods;
        }
    };

    class CChildSpec final : public IProtocolObjectSpec {
      public:
        std::string objectName() override {
            return "child";
        }

        const std::vector<SMethod>& c2s() override {
            static const std::vector<SMethod> methods = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
            };

            return methods;
        }

        const std::vector<SMethod>& s2c() override {
            static const std::vector<SMethod> methods = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
            };

            return methods;
        }
    };

    class CIntegrationProtocolSpec final : public IProtocolSpec {
      public:
        std::string specName() override {
            return "integration_protocol";
        }

        uint32_t specVer() override {
            return 1;
        }

        std::vector<SP<IProtocolObjectSpec>> objects() override {
            return {
                m_managerSpec,
                m_childSpec,
            };
        }

      private:
        SP<CManagerSpec> m_managerSpec = makeShared<CManagerSpec>();
        SP<CChildSpec>   m_childSpec   = makeShared<CChildSpec>();
    };

    class CIntegrationClientImpl final : public IProtocolClientImplementation {
      public:
        explicit CIntegrationClientImpl(SP<IProtocolSpec> protocolSpec) : m_protocolSpec(std::move(protocolSpec)) {
            ;
        }

        SP<IProtocolSpec> protocol() override {
            return m_protocolSpec;
        }

        std::vector<SP<SClientObjectImplementation>> implementation() override {
            return {
                makeShared<SClientObjectImplementation>(SClientObjectImplementation{.objectName = "manager", .version = 1}),
                makeShared<SClientObjectImplementation>(SClientObjectImplementation{.objectName = "child", .version = 1}),
            };
        }

      private:
        SP<IProtocolSpec> m_protocolSpec;
    };

    class CIntegrationServerImpl final : public IProtocolServerImplementation {
      public:
        CIntegrationServerImpl(SP<IProtocolSpec> protocolSpec, std::function<void(SP<IObject>)>&& bindFn) : m_protocolSpec(std::move(protocolSpec)), m_bindFn(std::move(bindFn)) {
            ;
        }

        SP<IProtocolSpec> protocol() override {
            return m_protocolSpec;
        }

        std::vector<SP<SServerObjectImplementation>> implementation() override {
            return {
                makeShared<SServerObjectImplementation>(SServerObjectImplementation{.objectName = "manager", .version = 1, .onBind = m_bindFn}),
                makeShared<SServerObjectImplementation>(SServerObjectImplementation{.objectName = "child", .version = 1}),
            };
        }

      private:
        SP<IProtocolSpec>                m_protocolSpec;
        std::function<void(SP<IObject>)> m_bindFn;
    };

    class CIntegrationHarness {
      public:
        CIntegrationHarness() {
            int fds[2] = {-1, -1};
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
                throw std::runtime_error("socketpair failed");

            m_server = IServerSocket::open();
            if (!m_server)
                throw std::runtime_error("server open failed");

            m_protocolSpec = makeShared<CIntegrationProtocolSpec>();
            m_serverImpl   = makeShared<CIntegrationServerImpl>(m_protocolSpec, [this](SP<IObject> obj) { onManagerBind(std::move(obj)); });
            m_clientImpl   = makeShared<CIntegrationClientImpl>(m_protocolSpec);

            m_server->addImplementation(std::move(m_serverImpl));
            m_serverClient = m_server->addClient(fds[0]);
            if (!m_serverClient)
                throw std::runtime_error("server addClient failed");

            m_client = IClientSocket::open(fds[1]);
            if (!m_client)
                throw std::runtime_error("client open failed");

            m_client->addImplementation(std::move(m_clientImpl));

            m_pumpThread = std::thread([this] {
                while (!m_stopPump) {
                    if (m_server)
                        m_server->dispatchEvents(false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }

        ~CIntegrationHarness() {
            m_stopPump = true;
            if (m_pumpThread.joinable())
                m_pumpThread.join();
        }

        bool waitForHandshake() {
            return m_client->waitForHandshake();
        }

        SP<IObject> bindManager() {
            auto manager = m_client->bindProtocol(m_protocolSpec, 1);
            if (!manager)
                return nullptr;

            manager->setData(this);
            manager->listen(0, fnToVoid(&CIntegrationHarness::onManagerNotify));

            {
                std::scoped_lock lock(m_stateMutex);
                m_managerClientObject = manager;
            }

            return manager;
        }

        bool pumpClientUntil(const std::function<bool()>& pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(1500)) {
            const auto start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start < timeout) {
                if (!m_client->dispatchEvents(false) && pred())
                    return true;

                if (pred())
                    return true;

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return pred();
        }

        SP<IObject> childClientObjectForSeq(uint32_t seq) {
            return m_client->objectForSeq(seq);
        }

        void attachChildClientListener(const SP<IObject>& child) {
            child->setData(this);
            child->listen(0, fnToVoid(&CIntegrationHarness::onChildNotify));
        }

        void roundtrip() {
            m_client->roundtrip();
        }

        bool dispatchClient() {
            return m_client->dispatchEvents(false);
        }

        int clientFD() {
            return m_client->extractLoopFD();
        }

        SP<IClientSocket> client() {
            return m_client;
        }

        SP<IProtocolSpec> protocolSpec() {
            return m_protocolSpec;
        }

        std::vector<std::string> serverPings() {
            std::scoped_lock lock(m_stateMutex);
            return m_serverPings;
        }

        std::vector<std::string> clientNotifications() {
            std::scoped_lock lock(m_stateMutex);
            return m_clientNotifications;
        }

        std::vector<uint32_t> serverArrayPayload() {
            std::scoped_lock lock(m_stateMutex);
            return m_serverArrayPayload;
        }

        std::string serverFdPayload() {
            std::scoped_lock lock(m_stateMutex);
            return m_serverFdPayload;
        }

        uint32_t childPingValue() {
            std::scoped_lock lock(m_stateMutex);
            return m_childPingValue;
        }

        uint32_t childNotifyValue() {
            std::scoped_lock lock(m_stateMutex);
            return m_childNotifyValue;
        }

      private:
        static void onManagerPing(IObject* object, const char* msg) {
            auto* self = static_cast<CIntegrationHarness*>(object->getData());
            {
                std::scoped_lock lock(self->m_stateMutex);
                self->m_serverPings.emplace_back(msg ? msg : "");
            }

            object->call(0, "pong");
        }

        static void onManagerMakeChild(IObject* object, uint32_t seq) {
            auto* self = static_cast<CIntegrationHarness*>(object->getData());

            auto  child = object->serverSock()->createObject(object->client(), object->self(), "child", seq);
            if (!child)
                return;

            child->setData(self);
            child->listen(0, fnToVoid(&CIntegrationHarness::onChildPing));
        }

        static void onManagerSendFd(IObject* object, int32_t fd) {
            auto*            self = static_cast<CIntegrationHarness*>(object->getData());

            char             buf[32] = {0};
            int              n       = static_cast<int>(read(fd, buf, sizeof(buf) - 1));

            std::scoped_lock lock(self->m_stateMutex);
            self->m_serverFdPayload = n > 0 ? std::string{buf, static_cast<size_t>(n)} : "";
        }

        static void onManagerSendArray(IObject* object, uint32_t* data, uint32_t len) {
            auto*                 self = static_cast<CIntegrationHarness*>(object->getData());

            std::vector<uint32_t> payload;
            payload.reserve(len);
            for (uint32_t i = 0; i < len; ++i) {
                payload.emplace_back(data[i]);
            }

            std::scoped_lock lock(self->m_stateMutex);
            self->m_serverArrayPayload = std::move(payload);
        }

        static void onChildPing(IObject* object, uint32_t value) {
            auto* self = static_cast<CIntegrationHarness*>(object->getData());

            {
                std::scoped_lock lock(self->m_stateMutex);
                self->m_childPingValue = value;
            }

            object->call(0, value + 1);
        }

        static void onManagerNotify(IObject* object, const char* msg) {
            auto*            self = static_cast<CIntegrationHarness*>(object->getData());
            std::scoped_lock lock(self->m_stateMutex);
            self->m_clientNotifications.emplace_back(msg ? msg : "");
        }

        static void onChildNotify(IObject* object, uint32_t value) {
            auto*            self = static_cast<CIntegrationHarness*>(object->getData());
            std::scoped_lock lock(self->m_stateMutex);
            self->m_childNotifyValue = value;
        }

        void onManagerBind(SP<IObject> obj) {
            obj->setData(this);
            obj->listen(0, fnToVoid(&CIntegrationHarness::onManagerPing));
            obj->listen(1, fnToVoid(&CIntegrationHarness::onManagerMakeChild));
            obj->listen(2, fnToVoid(&CIntegrationHarness::onManagerSendFd));
            obj->listen(3, fnToVoid(&CIntegrationHarness::onManagerSendArray));
        }

      private:
        std::atomic<bool>                 m_stopPump = false;
        std::thread                       m_pumpThread;

        SP<IProtocolSpec>                 m_protocolSpec;
        SP<IProtocolServerImplementation> m_serverImpl;
        SP<IProtocolClientImplementation> m_clientImpl;

        SP<IServerSocket>                 m_server;
        SP<IServerClient>                 m_serverClient;
        SP<IClientSocket>                 m_client;

        SP<IObject>                       m_managerClientObject;

        std::mutex                        m_stateMutex;
        std::vector<std::string>          m_serverPings;
        std::vector<std::string>          m_clientNotifications;
        std::vector<uint32_t>             m_serverArrayPayload;
        std::string                       m_serverFdPayload;
        uint32_t                          m_childPingValue   = 0;
        uint32_t                          m_childNotifyValue = 0;
    };

} // namespace

TEST(IntegrationCore, AnonymousServerCanAddAndRemoveClients) {
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

    auto server = IServerSocket::open();
    ASSERT_NE(server, nullptr);

    auto addedClient = server->addClient(fds[0]);
    ASSERT_NE(addedClient, nullptr);

    EXPECT_TRUE(server->removeClient(fds[0]));
    EXPECT_FALSE(server->removeClient(fds[0]));

    close(fds[1]);
}

TEST(IntegrationCore, HandshakeAndSpecDiscoveryWorks) {
    CIntegrationHarness harness;

    ASSERT_TRUE(harness.waitForHandshake());
    ASSERT_TRUE(harness.client()->isHandshakeDone());

    auto spec = harness.client()->getSpec(harness.protocolSpec()->specName());
    ASSERT_NE(spec, nullptr);
    EXPECT_EQ(spec->specName(), "integration_protocol");
    EXPECT_EQ(spec->specVer(), 1u);

    EXPECT_EQ(harness.client()->getSpec("does_not_exist"), nullptr);
}

TEST(IntegrationCore, BindPingAndRoundtripFlowWorks) {
    CIntegrationHarness harness;

    ASSERT_TRUE(harness.waitForHandshake());

    auto manager = harness.bindManager();
    ASSERT_NE(manager, nullptr);

    manager->call(0, "hello");
    harness.roundtrip();

    ASSERT_TRUE(harness.pumpClientUntil([&harness] { return !harness.clientNotifications().empty(); }));

    const auto pings = harness.serverPings();
    ASSERT_EQ(pings.size(), 1u);
    EXPECT_EQ(pings[0], "hello");

    const auto notifications = harness.clientNotifications();
    ASSERT_FALSE(notifications.empty());
    EXPECT_EQ(notifications.back(), "pong");
}

TEST(IntegrationCore, FdArrayAndReturnedObjectFlowWorks) {
    CIntegrationHarness harness;

    ASSERT_TRUE(harness.waitForHandshake());

    auto manager = harness.bindManager();
    ASSERT_NE(manager, nullptr);

    int pipefd[2] = {-1, -1};
    ASSERT_EQ(pipe(pipefd), 0);
    ASSERT_EQ(write(pipefd[1], "pipe!", 5), 5);

    manager->call(2, pipefd[0]);

    uint32_t numbers[3] = {69, 420, 2137};
    manager->call(3, numbers, static_cast<uint32_t>(3));

    const uint32_t childSeq = manager->call(1);
    ASSERT_NE(childSeq, 0u);

    auto child = harness.childClientObjectForSeq(childSeq);
    ASSERT_NE(child, nullptr);
    harness.attachChildClientListener(child);

    child->call(0, 41u);

    harness.roundtrip();

    ASSERT_TRUE(harness.pumpClientUntil([&harness] { return harness.childNotifyValue() != 0; }));

    EXPECT_EQ(harness.serverFdPayload(), "pipe!");
    EXPECT_EQ(harness.serverArrayPayload(), (std::vector<uint32_t>{69, 420, 2137}));
    EXPECT_EQ(harness.childPingValue(), 41u);
    EXPECT_EQ(harness.childNotifyValue(), 42u);

    close(pipefd[0]);
    close(pipefd[1]);
}

TEST(IntegrationCore, MalformedMessageDisconnectsClient) {
    CIntegrationHarness harness;

    ASSERT_TRUE(harness.waitForHandshake());

    const std::array<uint8_t, 2> badMessage = {0xFF, HW_MESSAGE_MAGIC_END};
    ASSERT_EQ(write(harness.clientFD(), badMessage.data(), badMessage.size()), static_cast<ssize_t>(badMessage.size()));

    bool       disconnected = false;
    const auto start        = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1500)) {
        if (!harness.dispatchClient()) {
            disconnected = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(disconnected);
}
