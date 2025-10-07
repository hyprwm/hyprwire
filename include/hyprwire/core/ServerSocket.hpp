#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

namespace Hyprwire {
    class IProtocolServerImplementation;
    class IObject;

    class IServerClient {
      public:
        virtual ~IServerClient();

      protected:
        IServerClient() = default;
    };

    class IServerSocket {
      public:
        virtual ~IServerSocket() = default;

        static Hyprutils::Memory::CSharedPointer<IServerSocket> open(const std::string& path);

        /*
            Add an implementation to the socket
        */
        virtual void addImplementation(Hyprutils::Memory::CSharedPointer<IProtocolServerImplementation>&&) = 0;

        /*
            Create an object after client's request
        */
        virtual Hyprutils::Memory::CSharedPointer<IObject> createObject(Hyprutils::Memory::CSharedPointer<IServerClient> client,
                                                                        Hyprutils::Memory::CSharedPointer<IObject> reference, const std::string& object, uint32_t seq) = 0;

        /*
            Synchronously dispatch pending events. Returns false on failure.
        */
        virtual bool dispatchEvents(bool block = false) = 0;

        /*
            Extract the loop FD. FD is owned by this socket, do not close it.
        */
        virtual int extractLoopFD() = 0;

      protected:
        IServerSocket() = default;
    };
};