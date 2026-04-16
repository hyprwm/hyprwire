#include "ClientObject.hpp"
#include "ClientSocket.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/FFI.hpp"
#include "../../Macros.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"

#include <cstdarg>
#include <cstring>
#include <ffi.h>

using namespace Hyprwire;

CClientObject::CClientObject(SP<CClientSocket> client) : m_client(client) {
    ;
}

CClientObject::~CClientObject() {
    if (!m_destroyed && m_id != 0 && m_spec && m_client && m_client->m_fd.isValid()) {
        const auto methods = methodsOut();
        for (const auto& method : methods) {
            if (!method.isDestructor)
                continue;

            if (method.since > m_version)
                continue;

            if (!method.returnsType.empty()) {
                Debug::log(WARN, "can't auto-call destructor for object {}: method {} has returns type", m_id, method.idx);
                break;
            }

            if (!method.params.empty()) {
                Debug::log(WARN, "can't auto-call destructor for object {}: method {} has params", m_id, method.idx);
                break;
            }

            TRACE(Debug::log(TRACE, "auto-calling protocol destructor {} for object {}", method.idx, m_id));
            call(method.idx);
            break;
        }
    }

    TRACE(Debug::log(TRACE, "destroying object {}", m_id));
}

const std::vector<SMethod>& CClientObject::methodsOut() {
    return m_spec->c2s();
}

const std::vector<SMethod>& CClientObject::methodsIn() {
    return m_spec->s2c();
}

void CClientObject::errd() {
    if (m_client)
        m_client->m_error = true;
}

void CClientObject::sendMessage(const IMessage& msg) {
    if (m_client)
        m_client->sendMessage(msg);
}

SP<IServerClient> CClientObject::client() {
    return nullptr;
}

bool CClientObject::server() {
    return false;
}

SP<IObject> CClientObject::self() {
    return m_self.lock();
}

SP<IClientSocket> CClientObject::clientSock() {
    if (!m_client)
        return nullptr;
    return m_client.lock();
}
