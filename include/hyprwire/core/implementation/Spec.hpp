#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <cstdint>

#include "Types.hpp"

namespace Hyprwire {
    class IProtocolSpec {
      public:
        virtual ~IProtocolSpec();

        virtual std::string specName() = 0;
        virtual uint32_t    specVer()  = 0;

        /*
            First object is the manager which will be created upon bind.
        */
        virtual std::vector<Hyprutils::Memory::CSharedPointer<IProtocolObjectSpec>> objects() = 0;

      protected:
        IProtocolSpec() = default;
    };
};