#include <hyprwire/core/implementation/ClientImpl.hpp>
#include <hyprwire/core/implementation/Spec.hpp>
#include <hyprwire/core/implementation/ServerImpl.hpp>
#include <hyprwire/core/implementation/Object.hpp>
#include <hyprwire/core/ServerSocket.hpp>

using namespace Hyprwire;

IProtocolSpec::~IProtocolSpec()                                 = default;
IProtocolServerImplementation::~IProtocolServerImplementation() = default;
IProtocolClientImplementation::~IProtocolClientImplementation() = default;
IServerClient::~IServerClient()                                 = default;