#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <functional>
#include <string_view>

namespace Hyprwire {
    class IServerClient;
    class IServerSocket;
    class IClientSocket;

    class IObject {
      public:
        virtual ~IObject();

        virtual uint32_t                                   call(uint32_t id, ...)        = 0;
        virtual void                                       listen(uint32_t id, void* fn) = 0;

        virtual void                                       setData(void* data);
        virtual void*                                      getData();

        virtual void                                       setOnDestroy(std::function<void()>&& fn);

        virtual Hyprutils::Memory::CSharedPointer<IObject> self() = 0;

        // only for server objects
        virtual Hyprutils::Memory::CSharedPointer<IServerSocket> serverSock();
        virtual Hyprutils::Memory::CSharedPointer<IServerClient> client() = 0;
        virtual void                                             error(uint32_t id, const std::string_view& message);

        // only for client objects
        virtual Hyprutils::Memory::CSharedPointer<IClientSocket> clientSock();

      protected:
        IObject() = default;

      private:
        void*                 m_data = nullptr;
        std::function<void()> m_onDestroy;
    };
};
