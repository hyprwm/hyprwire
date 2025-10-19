#include "ServerObject.hpp"
#include "ServerClient.hpp"
#include "ServerSocket.hpp"
#include "../../helpers/Log.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstdarg>
#include <cstring>
#include <ffi.h>

using namespace Hyprwire;

CServerObject::CServerObject(SP<CServerClient> client) : m_client(client) {
    ;
}

const std::vector<SMethod>& CServerObject::methodsOut() {
    return m_spec->s2c();
}

const std::vector<SMethod>& CServerObject::methodsIn() {
    return m_spec->c2s();
}

void CServerObject::errd() {
    if (m_client)
        m_client->m_error = true;
}

void CServerObject::sendMessage(SP<CGenericProtocolMessage> msg) {
    if (m_client)
        m_client->sendMessage(msg);
}

SP<IServerClient> CServerObject::client() {
    return m_client.lock();
}

bool CServerObject::server() {
    return true;
}

SP<IObject> CServerObject::self() {
    return m_self.lock();
}

SP<IServerSocket> CServerObject::serverSock() {
    if (!m_client || !m_client->m_server)
        return nullptr;
    return m_client->m_server.lock();
}
