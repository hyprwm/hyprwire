#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

namespace Hyprwire {
    class IProtocolClientImplementation;
    class IProtocolSpec;
    class IObject;

    class IClientSocket {
      public:
        virtual ~IClientSocket() = default;

        static Hyprutils::Memory::CSharedPointer<IClientSocket> open(const std::string& path);

        // IClientSocket takes ownership of the fd.
        static Hyprutils::Memory::CSharedPointer<IClientSocket> open(const int fd);

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

        /*
            Bind a protocol object
        */
        virtual Hyprutils::Memory::CSharedPointer<IObject> bindProtocol(const Hyprutils::Memory::CSharedPointer<IProtocolSpec>& spec, uint32_t version) = 0;

        /*
            Get an object from an id
        */
        virtual Hyprutils::Memory::CSharedPointer<IObject> objectForId(uint32_t id) = 0;

        /*
            Perform a roundtrip
        */
        virtual void roundtrip() = 0;

        /*
            Check if handshake has been estabilished
        */
        virtual bool isHandshakeDone() = 0;

        /*
            Get an object from a sequence number
        */
        virtual Hyprutils::Memory::CSharedPointer<IObject> objectForSeq(uint32_t seq) = 0;

      protected:
        IClientSocket() = default;
    };
};