#include "ServerClient.hpp"
#include "../message/messages/IMessage.hpp"
#include "../../helpers/Log.hpp"
#include "../../Macros.hpp"

using namespace Hyprwire;

CServerClient::CServerClient(int fd) : m_fd(fd) {
    ;
}

void CServerClient::sendMessage(const SP<IMessage>& message) {
    TRACE(Debug::log(TRACE, "[{} @ {:.3f}] -> {}", m_fd.get(), steadyMillis(), message->parseData()));
    write(m_fd.get(), message->m_data.data(), message->m_data.size());
}
