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

using namespace Hyprwire;

CServerClient::CServerClient(int fd) : m_fd(fd) {
    ;
}

CServerClient::~CServerClient() {
    TRACE(Debug::log(TRACE, "[{}] destroying client", m_fd.get()));
}

void CServerClient::sendMessage(const IMessage& message) {
    TRACE(Debug::log(TRACE, "[{} @ {:.3f}] -> {}", m_fd.get(), steadyMillis(), message.parseData()));
    write(m_fd.get(), message.m_data.data(), message.m_data.size());
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
            break;
        }
    }
}
