#include "ServerSocket.hpp"
#include "ServerClient.hpp"
#include "ServerObject.hpp"
#include "../../helpers/Memory.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/FatalProtocolError.hpp"
#include "../message/messages/RoundtripDone.hpp"
#include "../socket/SocketHelpers.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <filesystem>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprwire;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

SP<IServerSocket> IServerSocket::open(const std::string& path) {
    SP<CServerSocket> sock = makeShared<CServerSocket>();
    sock->m_self           = sock;

    if (!sock->attempt(path))
        return nullptr;

    return sock;
}

SP<IServerSocket> IServerSocket::open(const int fd) {
    SP<CServerSocket> sock = makeShared<CServerSocket>();
    sock->m_self           = sock;

    if (!sock->attemptFromFd(fd))
        return nullptr;

    return sock;
}

CServerSocket::~CServerSocket() {
    if (!m_success)
        return;

    if (m_pollThread.joinable()) {
        write(m_exitWriteFd.get(), "x", 1);
        m_pollThread.join();
    }

    m_fd.reset();

    std::error_code ec;
    std::filesystem::remove(m_path, ec);
}

bool CServerSocket::attempt(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        if (ec)
            return false;

        // check if the socket is alive, if so, fuck off.
        m_fd                      = CFileDescriptor{socket(AF_UNIX, SOCK_STREAM, 0)};
        sockaddr_un serverAddress = {.sun_family = AF_UNIX};

        if (path.size() >= 108)
            return false;

        strcpy(serverAddress.sun_path, path.c_str());

        const bool FAILURE = connect(m_fd.get(), (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0;

        if (!FAILURE) {
            m_fd.reset();
            return false; // alive
        }

        if (errno != ECONNREFUSED)
            return false; // likely alive

        m_fd.reset();

        // remove file and continue
        std::filesystem::remove(path, ec);

        if (ec)
            return false; // no perms?
    }

    m_fd                      = CFileDescriptor{socket(AF_UNIX, SOCK_STREAM, 0)};
    sockaddr_un serverAddress = {.sun_family = AF_UNIX};

    if (path.size() >= 108)
        return false;

    strcpy(serverAddress.sun_path, path.c_str());

    if (bind(m_fd.get(), (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)))
        return false;

    listen(m_fd.get(), 100);

    m_fd.setFlags(O_NONBLOCK);

    m_success = true;
    m_path    = path;

    recheckPollFds();

    return true;
}

bool CServerSocket::attemptFromFd(const int fd) {

    auto x      = m_clients.emplace_back(makeShared<CServerClient>(fd));
    x->m_server = m_self;
    x->m_self   = x;

    m_success      = true;
    m_isFdListener = true;

    recheckPollFds();

    return true;
}

void CServerSocket::addImplementation(SP<IProtocolServerImplementation>&& x) {
    m_impls.emplace_back(std::move(x));
}

bool CServerSocket::dispatchPending() {
    poll(m_pollfds.data(), m_pollfds.size(), 0);
    if (dispatchNewConnections())
        return dispatchPending();

    return dispatchExistingConnections();
}

bool CServerSocket::dispatchEvents(bool block) {

    m_pollmtx.lock();

    while (dispatchPending()) {
        ;
    }

    // read from our event fd to avoid events
    clearEventFd();

    if (block) {
        poll(m_pollfds.data(), m_pollfds.size(), -1);
        while (dispatchPending()) {
            ;
        }
    }

    m_pollmtx.unlock();

    return true;
}

void CServerSocket::clearEventFd() {
    char   buf[128];
    pollfd pfd = {
        .fd     = m_exportFd.get(),
        .events = POLLIN,

    };

    while (m_exportFd.isValid()) {
        poll(&pfd, 1, 0);

        if (pfd.revents & POLLIN) {
            read(m_exportFd.get(), buf, 127);
            continue;
        }

        break;
    }
}

size_t CServerSocket::internalFds() {
    return m_isFdListener ? 1 : 2;
}

//
void CServerSocket::recheckPollFds() {
    m_pollfds.clear();

    if (!m_isFdListener) {
        m_pollfds.emplace_back(pollfd{
            .fd     = m_fd.get(),
            .events = POLLIN,
        });
    }

    m_pollfds.emplace_back(pollfd{
        .fd     = m_exitFd.get(),
        .events = POLLIN,
    });

    for (const auto& c : m_clients) {
        m_pollfds.emplace_back(pollfd{
            .fd     = c->m_fd.get(),
            .events = POLLIN,
        });
    }
}

bool CServerSocket::dispatchNewConnections() {
    if (m_isFdListener)
        return false;

    if (!(m_pollfds.at(0).revents & POLLIN))
        return false;

    sockaddr_in clientAddress = {};
    socklen_t   clientSize    = sizeof(clientAddress);

    auto        x = m_clients.emplace_back(makeShared<CServerClient>(accept(m_fd.get(), (sockaddr*)&clientAddress, &clientSize)));
    x->m_server   = m_self;
    x->m_self     = x;

    recheckPollFds();

    return true;
}

bool CServerSocket::dispatchExistingConnections() {
    bool hadAny           = false;
    bool needsPollRecheck = false;

    for (size_t i = internalFds(); i < m_pollfds.size(); ++i) {
        if (!(m_pollfds.at(i).revents & POLLIN))
            continue;

        dispatchClient(m_clients.at(i - internalFds()));

        hadAny = true;

        if (m_pollfds.at(i).revents & POLLHUP) {
            m_clients.at(i - internalFds())->m_error = true;
            needsPollRecheck                         = true;
            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] Dropping client (hangup)", m_clients.at(i - internalFds())->m_fd.get(), steadyMillis()));
            continue;
        }

        if (m_clients.at(i - internalFds())->m_error)
            TRACE(Debug::log(TRACE, "[{} @ {:.3f}] Dropping client (protocol error)", m_clients.at(i - internalFds())->m_fd.get(), steadyMillis()));
    }

    if (needsPollRecheck) {
        std::erase_if(m_clients, [](const auto& c) { return c->m_error; });
        recheckPollFds();
    }

    return hadAny;
}

void CServerSocket::dispatchClient(SP<CServerClient> client) {
    auto data = parseFromFd(client->m_fd);

    if (data.bad) {
        client->sendMessage(CFatalErrorMessage(nullptr, -1, "fatal: invalid message on wire"));
        client->m_error = true;
        return;
    }

    if (data.data.empty()) // this should NOT happen
        return;

    const auto RET = g_messageParser->handleMessage(data, client);

    if (RET != MESSAGE_PARSED_OK) {
        client->sendMessage(CFatalErrorMessage(nullptr, -1, "fatal: failed to handle message on wire"));
        client->m_error = true;
        return;
    }

    if (client->m_scheduledRoundtripSeq > 0) {
        client->sendMessage(CRoundtripDoneMessage{client->m_scheduledRoundtripSeq});
        client->m_scheduledRoundtripSeq = 0;
    }
}

int CServerSocket::extractLoopFD() {
    if (!m_exportFd.isValid()) {
        int pipes[2];
        pipe(pipes);

        m_exportFd      = CFileDescriptor{pipes[0]};
        m_exportWriteFd = CFileDescriptor{pipes[1]};

        pipe(pipes);

        m_exitFd      = CFileDescriptor{pipes[0]};
        m_exitWriteFd = CFileDescriptor{pipes[1]};

        m_threadCanPoll = true;

        recheckPollFds();

        m_pollThread = std::thread([this] {
            while (m_threadCanPoll) {
                m_pollmtx.lock();

                std::vector<pollfd> pollfds;
                if (!m_isFdListener) {
                    pollfds.emplace_back(pollfd{
                        .fd     = m_fd.get(),
                        .events = POLLIN,
                    });
                }

                pollfds.emplace_back(pollfd{
                    .fd     = m_exitFd.get(),
                    .events = POLLIN,
                });

                for (const auto& c : m_clients) {
                    pollfds.emplace_back(pollfd{
                        .fd     = c->m_fd.get(),
                        .events = POLLIN,
                    });
                }

                m_pollmtx.unlock();
                poll(pollfds.data(), pollfds.size(), -1);

                write(m_exportWriteFd.get(), "x", 1);
            }
        });
    }

    return m_exportFd.get();
}

SP<IObject> CServerSocket::createObject(SP<IServerClient> clientIface, SP<IObject> reference, const std::string& object, uint32_t seq) {
    if (!clientIface || !reference)
        return nullptr;

    auto client = reinterpretPointerCast<CServerClient>(clientIface);
    auto ref    = reinterpretPointerCast<CServerObject>(reference);

    auto newObject = client->createObject(ref->m_protocolName, object, ref->m_version, seq);

    if (!newObject)
        return nullptr;

    return newObject;
}
