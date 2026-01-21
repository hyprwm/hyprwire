#include <hyprwire/hyprwire.hpp>
#include <print>
#include "generated/test_protocol_v1-client.hpp"
#include <unistd.h>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

constexpr const uint32_t           TEST_PROTOCOL_VERSION = 1;

static SP<CCTestProtocolV1Impl>    impl = makeShared<CCTestProtocolV1Impl>(TEST_PROTOCOL_VERSION);
static SP<CCMyManagerV1Object>     manager;
static SP<CCMyObjectV1Object>      object;
static SP<Hyprwire::IClientSocket> sock;

int                                main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    sock                       = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    if (!sock) {
        std::println("err: failed to open client socket");
        return 1;
    }

    sock->addImplementation(impl);

    if (!sock->waitForHandshake()) {
        std::println("err: handshake failed");
        return 1;
    }

    const auto SPEC = sock->getSpec(impl->protocol()->specName());

    if (!SPEC) {
        std::println("err: test protocol unsupported");
        return 1;
    }

    std::println("test protocol supported at version {}. Binding.", SPEC->specVer());

    manager = makeShared<CCMyManagerV1Object>(sock->bindProtocol(impl->protocol(), TEST_PROTOCOL_VERSION));

    std::println("Bound!");

    int pips[2];
    sc<void>(pipe(pips));
    sc<void>(write(pips[1], "pipe!", 5));

    std::println("Will send fd {}", pips[0]);

    manager->sendSendMessage("Hello!");
    manager->sendSendMessageFd(pips[0]);
    manager->sendSendMessageArray(std::vector<const char*>{"Hello", "via", "array!"});
    manager->sendSendMessageArray(std::vector<const char*>{});
    manager->sendSendMessageArrayUint(std::vector<uint32_t>{69, 420, 2137});
    manager->setSendMessage([](const char* msg) { std::println("Server says {}", msg); });

    // test roundtrip
    sock->roundtrip();

    object = makeShared<CCMyObjectV1Object>(manager->sendMakeObject());
    object->setSendMessage([](const char* msg) { std::println("Server says on object {}", msg); });
    object->sendSendMessage("Hello on object");
    object->sendSendEnum(TEST_PROTOCOL_V1_MY_ENUM_WORLD);

    std::println("Sent hello!");

    while (sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}
