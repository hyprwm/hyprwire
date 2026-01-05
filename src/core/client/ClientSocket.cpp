#include "ClientSocket.hpp"
#include "../../helpers/Memory.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/IMessage.hpp"
#include "../message/messages/Hello.hpp"
#include "../message/messages/BindProtocol.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include "../message/messages/RoundtripRequest.hpp"
#include "../socket/SocketHelpers.hpp"
#include "ServerSpec.hpp"
#include "ClientObject.hpp"

#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/implementation/Types.hpp>
#include <hyprwire/core/implementation/Spec.hpp>
#include <hyprwire/core/implementation/ClientImpl.hpp>

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
    sock->m_self           = sock;

    if (!sock->attempt(path))
        return nullptr;

    return sock;
}

SP<IClientSocket> IClientSocket::open(const int fd) {
    SP<CClientSocket> sock = makeShared<CClientSocket>();
    sock->m_self           = sock;

    if (!sock->attemptFromFd(fd))
        return nullptr;

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

    m_fd.setFlags(O_NONBLOCK | O_CLOEXEC);

    m_pollfds = {pollfd{
        .fd     = m_fd.get(),
        .events = POLLIN,
    }};

    // send hello instantly
    sendMessage(CHelloMessage());

    return true;
}

bool CClientSocket::attemptFromFd(const int fd) {
    m_fd = CFileDescriptor{fd};

    m_fd.setFlags(O_NONBLOCK | O_CLOEXEC);

    m_pollfds = {pollfd{
        .fd     = m_fd.get(),
        .events = POLLIN,
    }};

    // send hello instantly
    sendMessage(CHelloMessage());

    return true;
}

void CClientSocket::addImplementation(SP<IProtocolClientImplementation>&& x) {
    m_impls.emplace_back(std::move(x));
}

constexpr const size_t HANDSHAKE_MAX_MS = 5000;

//
bool CClientSocket::dispatchEvents(bool block) {

    if (m_error)
        return false;

    if (!m_handshakeDone) {
        const auto MAX_MS =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds(HANDSHAKE_MAX_MS) - (std::chrono::steady_clock::now() - m_handshakeBegin)).count();
        int ret = poll(m_pollfds.data(), m_pollfds.size(), block ? MAX_MS : 0);
        if (block && !ret) {
            Debug::log(ERR, "handshake error: timed out");
            disconnectOnError();
            return false;
        }
    }

    if (!m_pendingSocketData.empty()) {
        auto CPY = std::move(m_pendingSocketData);
        for (auto& d : CPY) {
            const auto RET = g_messageParser->handleMessage(d, m_self.lock());

            if (RET != MESSAGE_PARSED_OK) {
                Debug::log(ERR, "fatal: failed to handle message on wire");
                disconnectOnError();
                return false;
            }
        }
    }

    if (m_handshakeDone)
        poll(m_pollfds.data(), m_pollfds.size(), block ? -1 : 0);

    if (m_pollfds[0].revents & POLLHUP)
        return false;

    if (!(m_pollfds[0].revents & POLLIN))
        return true;

    // dispatch

    auto data = parseFromFd(m_fd);

    if (data.bad) {
        Debug::log(ERR, "fatal: received malformed message from server");
        disconnectOnError();
        return false;
    }

    if (data.data.empty())
        return false;

    const auto RET = g_messageParser->handleMessage(data, m_self.lock());

    if (RET != MESSAGE_PARSED_OK) {
        Debug::log(ERR, "fatal: failed to handle message on wire");
        disconnectOnError();
        return false;
    }

    return !m_error;
}

void CClientSocket::sendMessage(const IMessage& message) {
    TRACE(Debug::log(TRACE, "[{} @ {:.3f}] -> {}", m_fd.get(), steadyMillis(), message.parseData()));

    // NOLINTNEXTLINE
    msghdr      msg = {0}; // NOLINTNEXTLINE
    iovec       io  = {0};

    const auto& FDS = message.fds();

    // fucking evil!
    io.iov_base    = cc<void*>(rc<const void*>(message.m_data.data()));
    io.iov_len     = message.m_data.size();
    msg.msg_iov    = &io;
    msg.msg_iovlen = 1;

    std::vector<uint8_t> controlBuf;

    if (!FDS.empty()) {
        controlBuf.resize(CMSG_SPACE(sizeof(int) * FDS.size()));

        msg.msg_control    = controlBuf.data();
        msg.msg_controllen = controlBuf.size();

        cmsghdr* cmsg    = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type  = SCM_RIGHTS;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(int) * FDS.size());

        int* data = rc<int*>(CMSG_DATA(cmsg));
        for (size_t i = 0; i < FDS.size(); ++i) {
            data[i] = FDS.at(i);
        }
    }

    sendmsg(m_fd.get(), &msg, 0);
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
    } catch (...) {
        Debug::log(ERR, "fatal: failed to parse server specs");
        disconnectOnError();
    }

    m_handshakeDone = true;
}

bool CClientSocket::waitForHandshake() {
    m_handshakeBegin = std::chrono::steady_clock::now();

    while (!m_error && !m_handshakeDone) {
        if (!dispatchEvents(true))
            return false;
    }

    return !m_error;
}

bool CClientSocket::isHandshakeDone() {
    return m_handshakeDone;
}

SP<IProtocolSpec> CClientSocket::getSpec(const std::string& name) {
    for (const auto& s : m_serverSpecs) {
        if (s->specName() == name)
            return s;
    }
    return nullptr;
}

void CClientSocket::onSeq(uint32_t seq, uint32_t id) {
    for (const auto& c : m_objects) {
        if (c->m_seq == seq) {
            c->m_id = id;
            break;
        }
    }
}

SP<IObject> CClientSocket::bindProtocol(const SP<IProtocolSpec>& spec, uint32_t version) {
    if (version > spec->specVer()) {
        Debug::log(ERR, "version {} is larger than current spec ver of {}", version, spec->specVer());
        disconnectOnError();
        return nullptr;
    }

    auto object            = makeShared<CClientObject>(m_self.lock());
    object->m_spec         = spec->objects().front();
    object->m_seq          = ++m_seq;
    object->m_version      = version;
    object->m_self         = object;
    object->m_protocolName = spec->specName();
    m_objects.emplace_back(object);

    auto bindMessage = CBindProtocolMessage(spec->specName(), object->m_seq, 1);
    sendMessage(bindMessage);

    waitForObject(object);

    return object;
}

SP<CClientObject> CClientSocket::makeObject(const std::string& protocolName, const std::string& objectName, uint32_t seq) {
    auto object            = makeShared<CClientObject>(m_self.lock());
    object->m_self         = object;
    object->m_protocolName = protocolName;

    for (const auto& p : m_impls) {
        if (p->protocol()->specName() != protocolName)
            continue;

        for (const auto& o : p->protocol()->objects()) {
            if (o->objectName() != objectName)
                continue;

            object->m_spec = o;
            break;
        }
        break;
    }

    if (!object->m_spec)
        return nullptr;

    object->m_seq     = seq;
    object->m_version = 0; // TODO: client version doesn't matter that much, but for verification's sake we could fix this
    m_objects.emplace_back(object);
    return object;
}

void CClientSocket::waitForObject(SP<CClientObject> x) {
    m_waitingOnObject = x;
    while (!x->m_id && !m_error) {
        dispatchEvents(true);
    }
    m_waitingOnObject.reset();
}

bool CClientSocket::shouldEndReading() {
    return m_waitingOnObject && m_waitingOnObject->m_id;
}

void CClientSocket::onGeneric(const CGenericProtocolMessage& msg) {
    for (const auto& o : m_objects) {
        if (o->m_id == msg.m_object) {
            o->called(msg.m_method, msg.m_dataSpan, msg.m_fds);
            break;
        }
    }
}

SP<IObject> CClientSocket::objectForId(uint32_t id) {
    for (const auto& o : m_objects) {
        if (o->m_id == id)
            return o;
    }
    return nullptr;
}

void CClientSocket::disconnectOnError() {
    m_error = true;
    m_fd.reset();
}

void CClientSocket::roundtrip() {
    if (m_error)
        return;

    auto nextSeq = m_lastAckdRoundtripSeq + 1;
    sendMessage(CRoundtripRequestMessage(nextSeq));

    while (m_lastAckdRoundtripSeq < nextSeq) {
        if (!dispatchEvents(true))
            break;
    }
}
