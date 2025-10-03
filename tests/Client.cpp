#include <hyprwire/hyprwire.hpp>
#include "Spec.hpp"
#include <print>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

static SP<CTestProtocolSpec> spec = makeShared<CTestProtocolSpec>();

//
static void onObjectS2CMessage(const char* data) {
    std::println("Client says hello! We got data: {}", data);
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
                .s2c =
                    {
                        rc<void*>(::onObjectS2CMessage),
                    },
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

    std::println("test protocol supported at version {}", SPEC->specVer());

    while (sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}