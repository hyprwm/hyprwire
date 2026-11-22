#pragma once

#include <cstdint>
#include <limits>

namespace Hyprwire {
    constexpr const uint32_t HYPRWIRE_PROTOCOL_VER     = 1;
    constexpr const uint32_t HYPRWIRE_MAX_ARRAY_LENGTH = std::numeric_limits<uint16_t>::max();
}
