#include <hyprwire/hyprwire.hpp>
#include <print>
#include <sys/poll.h>
#include <sys/signal.h>
#include <sys/socket.h>

#include "generated/test_protocol_v1-server.hpp"
#include "generated/test_protocol_v1-client.hpp"

using namespace Hyprutils::Memory;

#define SP CSharedPointer
#define WP CWeakPointer

constexpr const uint32_t TEST_PROTOCOL_VERSION = 1;

static bool              quitt = false;

static void              sigHandler(int sig) {
    quitt = true;
}

static SP<CMyManagerV1Object>      manager;
static SP<CMyObjectV1Object>       object;
static SP<Hyprwire::IServerSocket> serverSock;
static SP<CCTestProtocolV1Impl>    impl = makeShared<CCTestProtocolV1Impl>(TEST_PROTOCOL_VERSION);
static SP<CTestProtocolV1Impl>     spec = makeShared<CTestProtocolV1Impl>(TEST_PROTOCOL_VERSION, [](SP<Hyprwire::IObject> obj) {
    std::println("Object bound XD");
    manager = makeShared<CMyManagerV1Object>(std::move(obj));

    manager->sendSendMessage("Hello manager");
    manager->setSendMessage([](const char* msg) { std::println("Recvd message: {}", msg); });
    manager->setSendMessageFd([](int fd) {
        char msgbuf[6] = {0};
        sc<void>(read(fd, msgbuf, 5));
        std::println("Recvd fd {} with data: {}", fd, msgbuf);
    });
    manager->setSendMessageArray([](std::vector<const char*> data) {
        std::string conct = "";
        for (const auto& d : data) {
            conct += d + std::string{", "};
        }
        if (conct.size() > 1) {
            conct.pop_back();
            conct.pop_back();
        }
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
        object = makeShared<CMyObjectV1Object>(serverSock->createObject(manager->getObject()->client(), manager->getObject(), "my_object_v1", seq));
        object->sendSendMessage("Hello object");
        object->setSendMessage([](const char* msg) { std::println("Object says hello: {}", msg); });
        object->setSendEnum([](testProtocolV1MyEnum e) {
            std::println("Object sent enum: {}", sc<uint32_t>(e));

            std::println("Erroring out the client!");

            quitt = true;
            object->error(TEST_PROTOCOL_V1_MY_ERROR_ENUM_ERROR_IMPORTANT, "Important error occurred!");
        });
    });
    manager->setOnDestroy([w = WP<CMyManagerV1Object>{manager}]() { //
        std::println("object {:x} destroyed", (uintptr_t)manager.get());
    });
});

static void                        server(int clientFd) {
    serverSock = Hyprwire::IServerSocket::open();
    pollfd pfd = {.fd = clientFd, .events = POLLIN, .revents = 0};

    serverSock->addImplementation(spec);

    // This is not requried, but it is nice to have
    if (!(poll(&pfd, 1, 1000) > 0 && (pfd.revents & POLLIN))) {
        std::println("Failed to wait for client hello");
        exit(1);
    }

    if (serverSock->addClient(clientFd) == nullptr) {
        std::println("Failed to add clientFd to the server socket!");
        exit(1);
    }

    signal(SIGINT, ::sigHandler);
    signal(SIGTERM, ::sigHandler);

    pfd = {.fd = serverSock->extractLoopFD(), .events = POLLIN, .revents = 0};

    while (!quitt) {
        int events = poll(&pfd, 1, -1);
        if (events < 0) {
            if (errno == EAGAIN)
                continue;
            else
                break;
        } else if (events == 0)
            continue;

        if (pfd.revents & POLLHUP)
            break;

        if (!(pfd.revents & POLLIN))
            continue;

        serverSock->dispatchEvents(false);
    }
}

static void client(int serverFd) {
    auto sock = Hyprwire::IClientSocket::open(serverFd);

    if (!sock->waitForHandshake()) {
        std::println("err: handshake failed");
        return;
    }

    sock->addImplementation(impl);

    std::println("OK!");

    const auto SPEC = sock->getSpec(impl->protocol()->specName());

    if (!SPEC) {
        std::println("err: test protocol unsupported");
        return;
    }

    std::println("test protocol supported at version {}. Binding.", SPEC->specVer());

    auto cmanager = makeShared<CCMyManagerV1Object>(sock->bindProtocol(impl->protocol(), TEST_PROTOCOL_VERSION));

    std::println("Bound!");

    int pips[2];
    sc<void>(pipe(pips));
    sc<void>(write(pips[1], "pipe!", 5));

    std::println("Will send fd {}", pips[0]);

    cmanager->sendSendMessage("Hello!");
    cmanager->sendSendMessageFd(pips[0]);
    cmanager->sendSendMessageArray(std::vector<const char*>{"Hello", "via", "array!"});
    cmanager->sendSendMessageArray(std::vector<const char*>{});
    cmanager->sendSendMessageArrayUint(std::vector<uint32_t>{69, 420, 2137});
    cmanager->setSendMessage([](const char* msg) { std::println("Server says {}", msg); });

    // test roundtrip
    sock->roundtrip();

    auto cobject = makeShared<CCMyObjectV1Object>(cmanager->sendMakeObject());
    cobject->setSendMessage([&cobject](const char* msg) {
        std::println("Server says on object {}", msg);
        cobject->sendSendEnum(TEST_PROTOCOL_V1_MY_ENUM_WORLD);
        quitt = true;
    });

    cobject->sendSendMessage("Hello on object");

    while (!quitt)
        sock->dispatchEvents(true);

    sock->roundtrip();
}

int main(int argc, char** argv, char** envp) {
    int sockFds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockFds))
        return 1;

    int   s    = 0;
    int   c    = 1;
    pid_t chld = fork();
    if (chld < 0) {
        close(sockFds[s]);
        close(sockFds[c]);
        std::println(stderr, "Failed to fork");
    } else if (chld == 0) {
        // CHILD (Client)
        close(sockFds[s]);
        client(sockFds[c]);
    } else {
        // PARENT (Server)
        close(sockFds[c]);
        server(sockFds[s]);
    }

    return 0;
}
