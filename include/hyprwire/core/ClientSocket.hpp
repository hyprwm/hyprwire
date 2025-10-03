#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

namespace Hyprwire {
    class IProtocolClientImplementation;

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

      protected:
        IClientSocket() = default;
    };
};