#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>
#include <hyprwire/core/types/MessageMagic.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
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
                SMethod{.idx = 0, .params = {}, .returnsType = "child", .since = 0},
            };
            return methods;
        }

        const std::vector<SMethod>& s2c() override {
            static const std::vector<SMethod> methods = {};
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
                SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 0, .isDestructor = true},
                SMethod{.idx = 1, .params = {HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
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

    class CSemanticsProtocolSpec final : public IProtocolSpec {
      public:
        std::string specName() override {
            return "semantics_protocol";
        }

        uint32_t specVer() override {
            return 1;
        }

        std::vector<SP<IProtocolObjectSpec>> objects() override {
            return {m_manager, m_child};
        }

      private:
        SP<CManagerSpec> m_manager = makeShared<CManagerSpec>();
        SP<CChildSpec>   m_child   = makeShared<CChildSpec>();
    };

    class CClientImpl final : public IProtocolClientImplementation {
      public:
        explicit CClientImpl(SP<IProtocolSpec> spec) : m_spec(std::move(spec)) {
            ;
        }

        SP<IProtocolSpec> protocol() override {
            return m_spec;
        }

        std::vector<SP<SClientObjectImplementation>> implementation() override {
            return {
                makeShared<SClientObjectImplementation>(SClientObjectImplementation{.objectName = "manager", .version = 1}),
                makeShared<SClientObjectImplementation>(SClientObjectImplementation{.objectName = "child", .version = 1}),
            };
        }

      private:
        SP<IProtocolSpec> m_spec;
    };

    class CServerImpl final : public IProtocolServerImplementation {
      public:
        CServerImpl(SP<IProtocolSpec> spec, std::function<void(SP<IObject>)>&& bindFn) : m_spec(std::move(spec)), m_bindFn(std::move(bindFn)) {
            ;
        }

        SP<IProtocolSpec> protocol() override {
            return m_spec;
        }

        std::vector<SP<SServerObjectImplementation>> implementation() override {
            return {
                makeShared<SServerObjectImplementation>(SServerObjectImplementation{.objectName = "manager", .version = 1, .onBind = m_bindFn}),
                makeShared<SServerObjectImplementation>(SServerObjectImplementation{.objectName = "child", .version = 1}),
            };
        }

      private:
        SP<IProtocolSpec>                m_spec;
        std::function<void(SP<IObject>)> m_bindFn;
    };

    class CSemanticsHarness {
      public:
        CSemanticsHarness() {
            int fds[2] = {-1, -1};
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
                throw std::runtime_error("socketpair failed");

            m_server = IServerSocket::open();
            if (!m_server)
                throw std::runtime_error("server open failed");

            m_spec       = makeShared<CSemanticsProtocolSpec>();
            m_serverImpl = makeShared<CServerImpl>(m_spec, [this](SP<IObject> obj) { onManagerBind(std::move(obj)); });
            m_clientImpl = makeShared<CClientImpl>(m_spec);

            m_server->addImplementation(std::move(m_serverImpl));
            auto serverClient = m_server->addClient(fds[0]);
            if (!serverClient)
                throw std::runtime_error("server addClient failed");

            m_client = IClientSocket::open(fds[1]);
            if (!m_client)
                throw std::runtime_error("client open failed");

            m_client->addImplementation(std::move(m_clientImpl));

            m_pumpThread = std::thread([this] {
                while (!m_stop) {
                    m_server->dispatchEvents(false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }

        ~CSemanticsHarness() {
            m_stop = true;
            if (m_pumpThread.joinable())
                m_pumpThread.join();
        }

        bool handshake() {
            return m_client->waitForHandshake();
        }

        SP<IObject> bindManager() {
            return m_client->bindProtocol(m_spec, 1);
        }

        SP<IObject> objectForSeq(uint32_t seq) {
            return m_client->objectForSeq(seq);
        }

        void attachChildListener(const SP<IObject>& child) {
            child->setData(this);
            child->listen(0, fnToVoid(&CSemanticsHarness::onChildPong));
        }

        void roundtrip() {
            m_client->roundtrip();
        }

        bool dispatchClient() {
            return m_client->dispatchEvents(false);
        }

        uint32_t childPong() const {
            return m_childPong.load();
        }

        uint32_t childPingCount() const {
            return m_childPingCount.load();
        }

        uint32_t childDestroyRequests() const {
            return m_childDestroyRequests.load();
        }

        uint32_t childDestroyCallbacks() const {
            return m_childDestroyCallbacks.load();
        }

      private:
        static void onManagerCreateChild(IObject* object, uint32_t seq) {
            auto* self = static_cast<CSemanticsHarness*>(object->getData());

            auto  child = object->serverSock()->createObject(object->client(), object->self(), "child", seq);
            if (!child)
                return;

            child->setData(self);
            child->listen(0, fnToVoid(&CSemanticsHarness::onChildDestroy));
            child->listen(1, fnToVoid(&CSemanticsHarness::onChildPing));
            child->setOnDestroy([self] { self->m_childDestroyCallbacks.fetch_add(1); });
        }

        static void onChildDestroy(IObject* object) {
            auto* self = static_cast<CSemanticsHarness*>(object->getData());
            self->m_childDestroyRequests.fetch_add(1);
        }

        static void onChildPing(IObject* object, uint32_t value) {
            auto* self = static_cast<CSemanticsHarness*>(object->getData());
            self->m_childPingCount.fetch_add(1);
            object->call(0, value + 1);
        }

        static void onChildPong(IObject* object, uint32_t value) {
            auto* self        = static_cast<CSemanticsHarness*>(object->getData());
            self->m_childPong = value;
        }

        void onManagerBind(SP<IObject> object) {
            object->setData(this);
            object->listen(0, fnToVoid(&CSemanticsHarness::onManagerCreateChild));
        }

      private:
        std::atomic<bool>                 m_stop = false;
        std::thread                       m_pumpThread;

        SP<IProtocolSpec>                 m_spec;
        SP<IProtocolServerImplementation> m_serverImpl;
        SP<IProtocolClientImplementation> m_clientImpl;
        SP<IServerSocket>                 m_server;
        SP<IClientSocket>                 m_client;

        std::atomic<uint32_t>             m_childPong             = 0;
        std::atomic<uint32_t>             m_childPingCount        = 0;
        std::atomic<uint32_t>             m_childDestroyRequests  = 0;
        std::atomic<uint32_t>             m_childDestroyCallbacks = 0;
    };

} // namespace

TEST(IntegrationSemantics, DestructorMethodsDestroyObjectsAndRejectFurtherCalls) {
    CSemanticsHarness harness;
    ASSERT_TRUE(harness.handshake());

    auto manager = harness.bindManager();
    ASSERT_NE(manager, nullptr);

    const uint32_t childSeq = manager->call(0);
    ASSERT_NE(childSeq, 0u);

    auto child = harness.objectForSeq(childSeq);
    ASSERT_NE(child, nullptr);
    harness.attachChildListener(child);

    child->call(1, 41u);
    harness.roundtrip();

    for (int i = 0; i < 500 && harness.childPong() != 42u; ++i) {
        harness.dispatchClient();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(harness.childPong(), 42u);
    EXPECT_EQ(harness.childPingCount(), 1u);

    child->call(0); // destructor method
    harness.roundtrip();

    for (int i = 0; i < 500 && harness.childDestroyCallbacks() < 1u; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(harness.childDestroyRequests(), 1u);
    EXPECT_EQ(harness.childDestroyCallbacks(), 1u);

    child->call(1, 55u); // stale object ID, should trigger fatal + disconnect

    bool disconnected = false;
    for (int i = 0; i < 500; ++i) {
        if (!harness.dispatchClient()) {
            disconnected = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(disconnected);
    EXPECT_EQ(harness.childPingCount(), 1u);
}

TEST(IntegrationSemantics, LostClientObjectAutoCallsProtocolDestructor) {
    CSemanticsHarness harness;
    ASSERT_TRUE(harness.handshake());

    auto manager = harness.bindManager();
    ASSERT_NE(manager, nullptr);

    const uint32_t childSeq = manager->call(0);
    ASSERT_NE(childSeq, 0u);

    // We intentionally never grab the returned object by seq.
    // This emulates the user "losing" the object immediately.

    for (int i = 0; i < 500 && harness.childDestroyRequests() == 0; ++i) {
        harness.dispatchClient();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int i = 0; i < 500 && harness.childDestroyCallbacks() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_EQ(harness.childDestroyRequests(), 1u);
    EXPECT_EQ(harness.childDestroyCallbacks(), 1u);
}
