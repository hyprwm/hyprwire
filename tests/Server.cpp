#include <hyprwire/hyprwire.hpp>
#include "Spec.hpp"
#include <print>
#include <sys/signal.h>

using namespace Hyprutils::Memory;

#define SP CSharedPointer

static SP<CTestProtocolSpec>       spec = makeShared<CTestProtocolSpec>();
static SP<Hyprwire::IServerSocket> sock;
static bool                        quitt = false;

//
static void onManagerC2SMessage(Hyprwire::IObject* obj, const char* data) {
    std::println("Received on manager: {}", data);
}

static void onObjectC2SMessage(Hyprwire::IObject* obj, const char* data) {
    std::println("Received on child: {}", data);
}

static void onManagerMakeObjectMessage(Hyprwire::IObject* obj, uint32_t seq) {
    std::println("Received on manager: seq {}", seq);
    auto x = obj->serverSock()->createObject(obj->client(), obj->self(), "my_object", seq);
    x->listen(0, rc<void*>(::onObjectC2SMessage));
    x->call(0, "Hi there new object!");
}

class CTestProtocolImpl : public Hyprwire::IProtocolServerImplementation {
  public:
    virtual ~CTestProtocolImpl() = default;

    virtual SP<Hyprwire::IProtocolSpec> protocol() {
        return spec;
    }

    virtual std::vector<Hyprutils::Memory::CSharedPointer<Hyprwire::SServerObjectImplementation>> implementation() {
        return {
            makeShared<Hyprwire::SServerObjectImplementation>(Hyprwire::SServerObjectImplementation{
                .objectName = "my_manager",
                .version    = 1,
                .onBind =
                    [](SP<Hyprwire::IObject> obj) {
                        std::println("Hey, looks like an object was bound :)");
                        obj->call(0, "You bound!");
                        obj->listen(0, rc<void*>(::onManagerC2SMessage));
                        obj->listen(1, rc<void*>(::onManagerMakeObjectMessage));
                    },
            }),
            makeShared<Hyprwire::SServerObjectImplementation>(Hyprwire::SServerObjectImplementation{
                .objectName = "my_object",
                .version    = 1,
            }),
        };
    }
};

static void sigHandler(int sig) {
    quitt = true;
}

int main(int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    sock                       = Hyprwire::IServerSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    sock->addImplementation(makeShared<CTestProtocolImpl>());

    signal(SIGINT, ::sigHandler);
    signal(SIGTERM, ::sigHandler);

    while (!quitt && sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}