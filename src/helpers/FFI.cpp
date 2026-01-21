#include "FFI.hpp"

using namespace Hyprwire;

ffi_type* Hyprwire::FFI::ffiTypeFrom(eMessageMagic magic) {
    switch (magic) {
        case HW_MESSAGE_MAGIC_TYPE_UINT:
        case HW_MESSAGE_MAGIC_TYPE_OBJECT:
        case HW_MESSAGE_MAGIC_TYPE_SEQ: return &ffi_type_uint32;
        case HW_MESSAGE_MAGIC_TYPE_FD:
        case HW_MESSAGE_MAGIC_TYPE_INT: return &ffi_type_sint32;
        case HW_MESSAGE_MAGIC_TYPE_F32: return &ffi_type_float;
        case HW_MESSAGE_MAGIC_TYPE_VARCHAR:
        case HW_MESSAGE_MAGIC_TYPE_ARRAY: return &ffi_type_pointer;
        default: return nullptr;
    }

    return nullptr;
}