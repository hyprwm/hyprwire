#include <hyprwire/hyprwire.hpp>
#include "Spec.hpp"
#include <print>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

static SP<CTestProtocolSpec> spec = makeShared<CTestProtocolSpec>();

//
static void onObjectC2SMessage(const char* data) {
    std::println("Received: {}", data);
}

class CTestProtocolImpl : public Hyprwire::IProtocolServerImplementation {
  public:
    virtual ~CTestProtocolImpl() = default;

    virtual SP<Hyprwire::IProtocolSpec> protocol() {
        return spec;
    }

    virtual std::vector<Hyprwire::SServerObjectImplementation> implementation() {
        return {
            Hyprwire::SServerObjectImplementation{
                .objectName = "my_object",
                .version    = 1,
                .onBind =
                    [](SP<Hyprwire::IObject> obj) {
                        std::println("Hey, looks like an object was bound :)");
                        obj->call(0, "You bound!");
                        obj->listen(0, rc<void*>(::onObjectC2SMessage));
                    },
            },
        };
    }
};

int main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    auto       sock            = Hyprwire::IServerSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    sock->addImplementation(makeShared<CTestProtocolImpl>());

    while (sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}