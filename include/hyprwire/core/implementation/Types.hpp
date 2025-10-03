#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <vector>
#include <cstdint>

namespace Hyprwire {

    struct SMethod {
        uint32_t             idx = 0;
        std::vector<uint8_t> params;
        std::vector<uint8_t> returns;
    };

    class IProtocolObjectSpec {
      public:
        virtual ~IProtocolObjectSpec() = default;

        virtual std::string          objectName() = 0;

        virtual std::vector<SMethod> c2s() = 0;
        virtual std::vector<SMethod> s2c() = 0;

      protected:
        IProtocolObjectSpec() = default;
    };

};