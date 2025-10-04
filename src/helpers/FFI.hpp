#pragma once

#include <ffi.h>
#include <hyprwire/core/types/MessageMagic.hpp>

namespace Hyprwire::FFI {
    ffi_type* ffiTypeFrom(eMessageMagic magic);
}
