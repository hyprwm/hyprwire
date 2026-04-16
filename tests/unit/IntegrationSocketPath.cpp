#include <gtest/gtest.h>

#include <hyprwire/hyprwire.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <thread>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace Hyprwire;

namespace {

    std::string makeSocketPath(const std::string& stem) {
        auto dir = std::filesystem::temp_directory_path() / std::format("hyprwire-tests-{}-{}", stem, getpid());
        std::filesystem::create_directories(dir);
        return (dir / "wire.sock").string();
    }

    class CPathHarness {
      public:
        explicit CPathHarness(std::string path) : m_path(std::move(path)) {
            m_server = IServerSocket::open(m_path);
            if (!m_server)
                throw std::runtime_error("server open(path) failed");

            m_pumpThread = std::thread([this] {
                while (!m_stop) {
                    m_server->dispatchEvents(false);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
        }

        ~CPathHarness() {
            m_stop = true;
            if (m_pumpThread.joinable())
                m_pumpThread.join();

            m_server.reset();
            std::error_code ec;
            std::filesystem::remove(m_path, ec);
            std::filesystem::remove(std::filesystem::path(m_path).parent_path(), ec);
        }

        int loopFD() {
            return m_server->extractLoopFD();
        }

        Hyprutils::Memory::CSharedPointer<IClientSocket> openClient() {
            return IClientSocket::open(m_path);
        }

      private:
        std::string                                      m_path;
        std::atomic<bool>                                m_stop = false;
        std::thread                                      m_pumpThread;
        Hyprutils::Memory::CSharedPointer<IServerSocket> m_server;
    };

} // namespace

TEST(IntegrationSocketPath, PathOpenHandshakeWorks) {
    CPathHarness harness{makeSocketPath("open")};

    auto         client = harness.openClient();
    ASSERT_NE(client, nullptr);

    EXPECT_TRUE(client->waitForHandshake());
    EXPECT_TRUE(client->isHandshakeDone());
}

TEST(IntegrationSocketPath, ExtractLoopFdSignalsPendingConnectionWork) {
    const auto socketPath = makeSocketPath("loopfd");

    auto       server = IServerSocket::open(socketPath);
    ASSERT_NE(server, nullptr);

    const int loopFD = server->extractLoopFD();
    ASSERT_GE(loopFD, 0);

    auto client = IClientSocket::open(socketPath);
    ASSERT_NE(client, nullptr);

    pollfd pfd = {
        .fd     = loopFD,
        .events = POLLIN,
    };

    const int pollRet = poll(&pfd, 1, 1000);
    ASSERT_GT(pollRet, 0);
    EXPECT_TRUE(pfd.revents & POLLIN);

    server->dispatchEvents(false);

    std::error_code ec;
    std::filesystem::remove(socketPath, ec);
    std::filesystem::remove(std::filesystem::path(socketPath).parent_path(), ec);
}

TEST(IntegrationSocketPath, RecoversFromStaleSocketFile) {
    const auto socketPath = makeSocketPath("stale");

    int        staleFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(staleFd, 0);

    sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    ASSERT_EQ(bind(staleFd, reinterpret_cast<sockaddr*>(&addr), SUN_LEN(&addr)), 0);
    close(staleFd);

    ASSERT_TRUE(std::filesystem::exists(socketPath));

    {
        auto server = IServerSocket::open(socketPath);
        ASSERT_NE(server, nullptr);
    }

    std::error_code ec;
    std::filesystem::remove(socketPath, ec);
    std::filesystem::remove(std::filesystem::path(socketPath).parent_path(), ec);
}

TEST(IntegrationSocketPath, RejectsPathLongerThanUnixSocketLimit) {
    const std::string longPath = "/tmp/" + std::string(200, 'x');

    EXPECT_EQ(IServerSocket::open(longPath), nullptr);
    EXPECT_EQ(IClientSocket::open(longPath), nullptr);
}
