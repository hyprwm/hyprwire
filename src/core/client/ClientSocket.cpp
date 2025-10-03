#include "ClientSocket.hpp"
#include "../../helpers/Memory.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/IMessage.hpp"
#include "../message/messages/Hello.hpp"
#include "ServerSpec.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <filesystem>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprwire;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

SP<IClientSocket> IClientSocket::open(const std::string& path) {
    SP<CClientSocket> sock = makeShared<CClientSocket>();
    if (!sock->attempt(path))
        return nullptr;
    sock->m_self = sock;
    return sock;
}

bool CClientSocket::attempt(const std::string& path) {
    m_fd                          = CFileDescriptor{socket(AF_UNIX, SOCK_STREAM, 0)};
    sockaddr_un     serverAddress = {.sun_family = AF_UNIX};

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec)
        return false;

    if (path.size() >= 108)
        return false;

    strcpy(serverAddress.sun_path, path.c_str());

    if (connect(m_fd.get(), (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        Debug::log(ERR, "err: {}", errno);
        return false;
    }

    m_fd.setFlags(O_NONBLOCK);

    m_pollfds = {pollfd{
        .fd     = m_fd.get(),
        .events = POLLIN,
    }};

    // send hello instantly
    sendMessage(makeShared<CHelloMessage>());

    return true;
}

void CClientSocket::addImplementation(SP<IProtocolClientImplementation>&& x) {
    m_impls.emplace_back(std::move(x));
}

bool CClientSocket::dispatchEvents(bool block) {

    poll(m_pollfds.data(), m_pollfds.size(), block ? -1 : 0);

    if (m_pollfds[0].revents & POLLHUP)
        return false;

    poll(m_pollfds.data(), m_pollfds.size(), block ? -1 : 0);

    if (m_pollfds[0].revents & POLLHUP)
        return false;

    if (!(m_pollfds[0].revents & POLLIN))
        return true;

    // dispatch

    std::vector<uint8_t> data;
    constexpr size_t     BUFFER_SIZE         = 8192;
    uint8_t              buffer[BUFFER_SIZE] = {0};

    ssize_t              sizeWritten = read(m_fd.get(), buffer, BUFFER_SIZE);

    if (sizeWritten <= 0)
        return false;

    data.append_range(std::span<uint8_t>(buffer, sizeWritten));

    while (sizeWritten == BUFFER_SIZE) {
        sizeWritten = read(m_fd.get(), buffer, BUFFER_SIZE);
        if (sizeWritten < 0)
            return false;

        data.append_range(std::span<uint8_t>(buffer, sizeWritten));
    }

    g_messageParser->handleMessage(data, m_self.lock());

    return true;
}

void CClientSocket::sendMessage(const SP<IMessage>& message) {
    TRACE(Debug::log(TRACE, "[{} @ {:.3f}] -> {}", m_fd.get(), steadyMillis(), message->parseData()));
    write(m_fd.get(), message->m_data.data(), message->m_data.size());
}

int CClientSocket::extractLoopFD() {
    return m_fd.get();
}

void CClientSocket::serverSpecs(const std::vector<std::string>& s) {
    try {
        for (const auto& specName : s) {
            size_t atPos = specName.find_last_of('@');
            m_serverSpecs.emplace_back(makeShared<CServerSpec>(specName.substr(0, atPos), std::stoul(specName.substr(atPos + 1))));
        }
    } catch (...) { m_error = true; }

    m_handshakeDone = true;
}

bool CClientSocket::waitForHandshake() {
    while (!m_error && !m_handshakeDone) {
        if (!dispatchEvents(true))
            return false;
    }

    return true;
}

SP<IProtocolSpec> CClientSocket::getSpec(const std::string& name) {
    for (const auto& s : m_serverSpecs) {
        if (s->specName() == name)
            return s;
    }
    return nullptr;
}
