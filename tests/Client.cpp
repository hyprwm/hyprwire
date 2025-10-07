#include <hyprwire/hyprwire.hpp>
#include "Spec.hpp"
#include <print>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

static SP<CTestProtocolSpec>       spec = makeShared<CTestProtocolSpec>();
static SP<Hyprwire::IObject>       obj;
static SP<Hyprwire::IClientSocket> sock;

//
static void onObjectS2CMessage(Hyprwire::IObject* obj, const char* data) {
    std::println("Received: {}", data);
    if (data == std::string{"You bound!"}) {
        obj->call(0, "Sure did! Over VaxWire™");
        obj->call(0, "I am now binding a new object!");
        auto id  = obj->call(1);
        auto obj = sock->objectForId(id);

        obj->call(0, std::format("Hey, seems like I got id {}!", id).c_str());
    }
}

class CTestProtocolImpl : public Hyprwire::IProtocolClientImplementation {
  public:
    virtual ~CTestProtocolImpl() = default;

    virtual SP<Hyprwire::IProtocolSpec> protocol() {
        return spec;
    }

    virtual std::vector<SP<Hyprwire::SClientObjectImplementation>> implementation() {
        return {
            makeShared<Hyprwire::SClientObjectImplementation>(Hyprwire::SClientObjectImplementation{
                .objectName = "my_manager",
                .version    = 1,
            }),
            makeShared<Hyprwire::SClientObjectImplementation>(Hyprwire::SClientObjectImplementation{
                .objectName = "my_object",
                .version    = 1,
            }),
        };
    }
};

int main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    sock                       = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

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