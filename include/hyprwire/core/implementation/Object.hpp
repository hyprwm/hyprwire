#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

namespace Hyprwire {
    class IServerClient;

    class IObject {
      public:
        virtual ~IObject();

        virtual uint32_t call(uint32_t id, ...)        = 0;
        virtual void     listen(uint32_t id, void* fn) = 0;

        // only for server objects
        virtual Hyprutils::Memory::CSharedPointer<IServerClient> client() = 0;
        virtual Hyprutils::Memory::CSharedPointer<IObject>       self()   = 0;

      protected:
        IObject() = default;
    };
};