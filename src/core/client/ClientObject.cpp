#include "ClientObject.hpp"
#include "ClientSocket.hpp"
#include "../../helpers/Log.hpp"
#include "../../helpers/FFI.hpp"
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

void CClientObject::sendMessage(SP<CGenericProtocolMessage> msg) {
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
