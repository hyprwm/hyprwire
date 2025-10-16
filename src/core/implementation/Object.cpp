#include <hyprwire/core/implementation/ClientImpl.hpp>
#include <hyprwire/core/implementation/Spec.hpp>
#include <hyprwire/core/implementation/ServerImpl.hpp>
#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/ServerSocket.hpp>

#include "../../helpers/Memory.hpp"

using namespace Hyprwire;

IObject::~IObject() {
    if (m_onDestroy)
        m_onDestroy();
}

SP<IServerSocket> IObject::serverSock() {
    return nullptr;
}

SP<IClientSocket> IObject::clientSock() {
    return nullptr;
}

void IObject::setData(void* data) {
    m_data = data;
}

void* IObject::getData() {
    return m_data;
}

void IObject::setOnDestroy(std::function<void()>&& fn) {
    m_onDestroy = std::move(fn);
}
