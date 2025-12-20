#include "ServerObject.hpp"
#include "ServerClient.hpp"
#include "ServerSocket.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"
#include "../message/MessageType.hpp"
#include "../message/MessageParser.hpp"
#include "../message/messages/GenericProtocolMessage.hpp"
#include "../message/messages/FatalProtocolError.hpp"
#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstdarg>
#include <cstring>
#include <string_view>
#include <ffi.h>

using namespace Hyprwire;

CServerObject::CServerObject(SP<CServerClient> client) : m_client(client) {
    ;
}

CServerObject::~CServerObject() {
    TRACE(Debug::log(TRACE, "[{}] destroying object {}", m_client->m_fd.get(), m_id));
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

void CServerObject::sendMessage(const IMessage& msg) {
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

void CServerObject::error(uint32_t id, const std::string_view& message) {
    CFatalErrorMessage msg(m_self.lock(), id, message);
    m_client->sendMessage(msg);
    errd();
}
