#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <cstdint>
#include <functional>

namespace Hyprwire {
    class IProtocolSpec;
    class IObject;

    struct SServerObjectImplementation {
        std::string                                                     objectName = "";
        uint32_t                                                        version    = 0;
        std::function<void(Hyprutils::Memory::CSharedPointer<IObject>)> onBind;
    };

    class IProtocolServerImplementation {
      public:
        virtual ~IProtocolServerImplementation();

        virtual Hyprutils::Memory::CSharedPointer<IProtocolSpec> protocol()       = 0;
        virtual std::vector<SServerObjectImplementation>         implementation() = 0;

      protected:
        IProtocolServerImplementation() = default;
    };
};