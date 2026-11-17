#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <functional>
#include <optional>
#include <string_view>
#include <sys/types.h>

namespace Hyprwire {
    class IProtocolServerImplementation;
    class IObject;

    struct SPeerCredentials {
        pid_t pid = -1;
        uid_t uid = static_cast<uid_t>(-1);
        gid_t gid = static_cast<gid_t>(-1);
    };

    class IServerClient {
      public:
        virtual ~IServerClient();

        virtual int getPID() = 0;

        /*
            Socket peer credentials captured when the client is added, before the
            new-client handler or handshake. Empty if they could not be obtained.
            These remain a snapshot, not the current credentials of a process or
            an attestation of the original sender behind a forwarded socket.
        */
        virtual std::optional<SPeerCredentials> getPeerCredentials() const = 0;

        /*
            Set a protocol filter. If the function returns false,
            the protocol will not be exposed in the handshake, and will not 
            be bindable.
        */
        virtual void setProtocolFilter(std::function<bool(std::string_view)>&& fn) = 0;

        /*
            Called once after the socket is closed and wire objects are torn down,
            even if the application retains this client. Runs during dispatch,
            explicit removal, or server destruction, not on the polling thread.
            Replaces the previous handler; an empty handler clears it. Installing
            a handler on an already disconnected client calls it immediately.
        */
        virtual void setDisconnectHandler(std::function<void()>&& fn) = 0;

      protected:
        IServerClient() = default;
    };

    class IServerSocket {
      public:
        virtual ~IServerSocket() = default;

        static Hyprutils::Memory::CSharedPointer<IServerSocket> open(const std::string& path);

        // anonymous socket, you can add and remove clients manually
        static Hyprutils::Memory::CSharedPointer<IServerSocket> open();

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

        /*
            Add a client by fd manually. Hyprwire takes ownership of the fd.
        */
        virtual Hyprutils::Memory::CSharedPointer<IServerClient> addClient(int fd) = 0;

        /*
            Remove a client by fd. Do not close the fd, hyprwire will manage it.
            Returns true if it removed anything.
        */
        virtual bool removeClient(int fd) = 0;

        /*
            Add a listener to each new client. This is emitted before we even
            start a handshake, and is mostly exposed for setting up filters.
        */
        virtual void setNewClientHandler(std::function<void(Hyprutils::Memory::CSharedPointer<IServerClient>)>&& fn) = 0;

      protected:
        IServerSocket() = default;
    };
};
