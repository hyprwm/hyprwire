#include "ServerClient.hpp"
#include "ServerObject.hpp"
#include "ServerSocket.hpp"
#include "../message/messages/IMessage.hpp"
#include "../message/messages/NewObject.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"

#include <hyprwire/core/implementation/ServerImpl.hpp>
#include <hyprwire/core/implementation/Spec.hpp>

#include <sys/socket.h>
#include <sys/types.h>

using namespace Hyprwire;

CServerClient::CServerClient(int fd) : m_fd(fd) {
    m_fd.setFlags(O_CLOEXEC);
}

CServerClient::~CServerClient() {
    TRACE(Debug::log(TRACE, "[{}] destroying client", m_fd.get()));
}

void CServerClient::dispatchFirstPoll() {
    if (m_firstPollDone)
        return;

    m_firstPollDone = true;

    // get peer's pid

#if defined(__OpenBSD__)
    struct sockpeercred cred;
#else
    ucred cred;
#endif
    socklen_t len = sizeof(cred);

    if (getsockopt(m_fd.get(), SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        TRACE(Debug::log(TRACE, "dispatchFirstPoll: failed to get pid"));
        return;
    }

    m_pid = cred.pid;
}

void CServerClient::sendMessage(const IMessage& message) {
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

    while (m_fd.isValid()) {
        int ret = sendmsg(m_fd.get(), &msg, 0);
        if (ret < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            pollfd pfd = {
                .fd     = m_fd.get(),
                .events = POLLOUT | POLLWRBAND,
            };
            poll(&pfd, 1, -1);
        } else
            break;
    }
}

SP<CServerObject> CServerClient::createObject(const std::string& protocol, const std::string& object, uint32_t version, uint32_t seq) {
    auto obj       = makeShared<CServerObject>(m_self.lock());
    obj->m_id      = m_maxId++;
    obj->m_self    = obj;
    obj->m_version = version;
    m_objects.emplace_back(obj);

    for (const auto& p : m_server->m_impls) {
        if (p->protocol()->specName() != protocol)
            continue;

        for (const auto& s : p->protocol()->objects()) {
            if (s->objectName() != object && !object.empty())
                continue;

            obj->m_spec = s;
            break;
        }

        obj->m_protocolName = protocol;

        if (!obj->m_spec) {
            Debug::log(ERR, "[{} @ {:.3f}] Error: createObject has no spec", m_fd.get(), steadyMillis());
            m_error = true;
            return nullptr;
        }

        if (p->protocol()->specVer() < version) {
            Debug::log(ERR, "[{} @ {:.3f}] Error: createObject for protocol {} object {} for version {}, but we have only {}", m_fd.get(), steadyMillis(), obj->m_protocolName,
                       object, version, p->protocol()->specVer());
            m_error = true;
            return nullptr;
        }

        break;
    }

    if (!obj->m_spec) {
        Debug::log(ERR, "[{} @ {:.3f}] Error: createObject has no spec", m_fd.get(), steadyMillis());
        m_error = true;
        return nullptr;
    }

    auto ret = CNewObjectMessage(seq, obj->m_id);
    sendMessage(ret);

    onBind(obj);

    return obj;
}

void CServerClient::onBind(SP<CServerObject> obj) {
    for (const auto& p : m_server->m_impls) {
        if (p->protocol()->specName() != obj->m_protocolName)
            continue;

        for (const auto& on : p->implementation()) {
            if (on->objectName != obj->m_spec->objectName())
                continue;

            if (on->onBind)
                on->onBind(obj);
            break;
        }

        break;
    }
}

void CServerClient::onGeneric(const CGenericProtocolMessage& msg) {
    for (const auto& o : m_objects) {
        if (o->m_id == msg.m_object) {
            o->called(msg.m_method, msg.m_dataSpan, msg.m_fds);
            return;
        }
    }

    Debug::log(WARN, "[{} @ {:.3f}] -> Generic message not handled. No object with id {}!", m_fd.get(), steadyMillis(), msg.m_object);
}

int CServerClient::getPID() {
    return m_pid;
}
