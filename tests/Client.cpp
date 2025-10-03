#include <hyprwire/hyprwire.hpp>

int main (int argc, char** argv, char** envp) {
    const auto XDG_RUNTIME_DIR = getenv("XDG_RUNTIME_DIR");
    auto sock = Hyprwire::IClientSocket::open(XDG_RUNTIME_DIR + std::string{"/test-hw.sock"});

    while (sock->dispatchEvents(true)) {
        ;
    }

    return 0;
}