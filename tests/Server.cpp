#include <hyprwire/hyprwire.hpp>
#include <print>
#include <sys/signal.h>

#include "generated/test_protocol_v1-server.hpp"

using namespace Hyprutils::Memory;

#define SP CSharedPointer
#define WP CWeakPointer

static SP<CMyManagerV1Object>      manager;
static SP<CMyObjectV1Object>       object;
static SP<Hyprwire::IServerSocket> sock;
static SP<CTestProtocolV1Impl>     spec  = makeShared<CTestProtocolV1Impl>(1, [](SP<Hyprwire::IObject> obj) {
    std::println("Object bound XD");
    manager = makeShared<CMyManagerV1Object>(std::move(obj));

    manager->sendSendMessage("Hello object");
    manager->setSendMessage([](const char* msg) { std::println("Recvd message: {}", msg); });
    manager->setSendMessageArray([](std::vector<const char*> data) {
        std::string conct = "";
        for (const auto& d : data) {
            conct += d + std::string{", "};
        }
        conct.pop_back();
        conct.pop_back();
        std::println("Got array message: \"{}\"", conct);
    });
    manager->setSendMessageArrayUint([](std::vector<uint32_t> data) {
        std::string conct = "";
        for (const auto& d : data) {
            conct += std::format("{}, ", d);
        }
        conct.pop_back();
        conct.pop_back();
        std::println("Got uint array message: \"{}\"", conct);
    });
    manager->setMakeObject([](uint32_t seq) {
        object = makeShared<CMyObjectV1Object>(sock->createObject(manager->getObject()->client(), manager->getObject(), "my_object_v1", seq));
        object->sendSendMessage("Hello object");
        object->setSendMessage([](const char* msg) { std::println("Object says hello"); });
    });
    manager->setOnDestroy([w = WP<CMyManagerV1Object>{manager}]() { //
        std::println("object {:x} destroyed", (uintptr_t)manager.get());
    });
});
static bool                        quitt = false;

static void                        sigHandler(int sig) {
    quitt = true;
}

int main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    sock                       = Hyprwire::IServerSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    sock->addImplementation(spec);

    signal(SIGINT, ::sigHandler);
    signal(SIGTERM, ::sigHandler);

    while (!quitt && sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}