#include <hyprwire/core/implementation/ClientImpl.hpp>
#include <hyprwire/core/implementation/Spec.hpp>
#include <hyprwire/core/implementation/ServerImpl.hpp>
#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/ServerSocket.hpp>

#include "../../helpers/Memory.hpp"

using namespace Hyprwire;

SP<IServerSocket> IObject::serverSock() {
    return nullptr;
}

SP<IClientSocket> IObject::clientSock() {
    return nullptr;
}
