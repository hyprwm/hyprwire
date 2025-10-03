#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <cstdint>

namespace Hyprwire {
    class IProtocolSpec;

    struct SClientObjectImplementation {
        std::string        objectName = "";
        uint32_t           version    = 0;
        std::vector<void*> s2c;
    };

    class IProtocolClientImplementation {
      public:
        virtual ~IProtocolClientImplementation();

        virtual Hyprutils::Memory::CSharedPointer<IProtocolSpec> protocol()       = 0;
        virtual std::vector<SClientObjectImplementation>         implementation() = 0;

      protected:
        IProtocolClientImplementation() = default;
    };
};