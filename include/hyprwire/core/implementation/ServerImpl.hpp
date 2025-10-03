#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <cstdint>

namespace Hyprwire {
    class IProtocolSpec;

    struct SServerObjectImplementation {
        std::string        objectName = "";
        uint32_t           version    = 0;
        std::vector<void*> c2s;
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