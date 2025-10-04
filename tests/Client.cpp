#include <hyprwire/hyprwire.hpp>
#include "Spec.hpp"
#include <print>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

static SP<CTestProtocolSpec> spec = makeShared<CTestProtocolSpec>();
static SP<Hyprwire::IObject> obj;

//
static void onObjectS2CMessage(const char* data) {
    std::println("Received: {}", data);
    if (data == std::string{"You bound!"})
        obj->call(0, "Sure did! Over VaxWire™");
}

class CTestProtocolImpl : public Hyprwire::IProtocolClientImplementation {
  public:
    virtual ~CTestProtocolImpl() = default;

    virtual SP<Hyprwire::IProtocolSpec> protocol() {
        return spec;
    }

    virtual std::vector<Hyprwire::SClientObjectImplementation> implementation() {
        return {
            Hyprwire::SClientObjectImplementation{
                .objectName = "my_object",
                .version    = 1,
            },
        };
    }
};

int main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    auto       sock            = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    sock->addImplementation(makeShared<CTestProtocolImpl>());

    if (!sock->waitForHandshake()) {
        std::println("err: handshake failed");
        return 1;
    }

    const auto SPEC = sock->getSpec("my_protocol");

    if (!SPEC) {
        std::println("err: test protocol unsupported");
        return 1;
    }

    std::println("test protocol supported at version {}. Binding.", SPEC->specVer());

    obj = sock->bindProtocol(spec, 1);

    std::println("Bound!");

    obj->call(0, "Hello!");
    obj->listen(0, rc<void*>(::onObjectS2CMessage));

    std::println("Sent hello!");

    while (sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}