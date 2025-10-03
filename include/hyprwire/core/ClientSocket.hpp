#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

namespace Hyprwire {
    class IProtocolClientImplementation;
    class IProtocolSpec;

    class IClientSocket {
      public:
        virtual ~IClientSocket() = default;

        static Hyprutils::Memory::CSharedPointer<IClientSocket> open(const std::string& path);

        /*
            Add an implementation to the socket
        */
        virtual void addImplementation(Hyprutils::Memory::CSharedPointer<IProtocolClientImplementation>&&) = 0;

        /*
            Synchronously dispatch pending events. Returns false on failure.
        */
        virtual bool dispatchEvents(bool block = false) = 0;

        /*
            Extract the loop FD. FD is owned by this socket, do not close it.
        */
        virtual int extractLoopFD() = 0;

        /*
            Wait for proper connection to be estabilished.
        */
        virtual bool waitForHandshake() = 0;

        /*
            Get a protocol spec from the server list. If the spec is supported, will be returned.
        */
        virtual Hyprutils::Memory::CSharedPointer<IProtocolSpec> getSpec(const std::string& name) = 0;

      protected:
        IClientSocket() = default;
    };
};