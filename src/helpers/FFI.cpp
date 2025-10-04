#include "FFI.hpp"

using namespace Hyprwire;

ffi_type* Hyprwire::FFI::ffiTypeFrom(eMessageMagic magic) {
    switch (magic) {
        case HW_MESSAGE_MAGIC_TYPE_UINT:
        case HW_MESSAGE_MAGIC_TYPE_OBJECT:
        case HW_MESSAGE_MAGIC_TYPE_SEQ:
        case HW_MESSAGE_MAGIC_TYPE_INT: return &ffi_type_uint32;
        case HW_MESSAGE_MAGIC_TYPE_F32: return &ffi_type_float;
        case HW_MESSAGE_MAGIC_TYPE_VARCHAR:
        case HW_MESSAGE_MAGIC_TYPE_ARRAY: return &ffi_type_pointer;
        case HW_MESSAGE_MAGIC_END: return nullptr;
    }

    return nullptr;
}