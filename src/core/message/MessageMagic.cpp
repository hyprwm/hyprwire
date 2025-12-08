#include "MessageMagic.hpp"

const char* Hyprwire::magicToString(eMessageMagic magic) {
    switch (magic) {
        case HW_MESSAGE_MAGIC_END: return "END";
        case HW_MESSAGE_MAGIC_TYPE_UINT: return "UINT";
        case HW_MESSAGE_MAGIC_TYPE_INT: return "INT";
        case HW_MESSAGE_MAGIC_TYPE_F32: return "F32";
        case HW_MESSAGE_MAGIC_TYPE_SEQ: return "SEQUENCE";
        case HW_MESSAGE_MAGIC_TYPE_OBJECT_ID: return "OBJECT_ID";
        case HW_MESSAGE_MAGIC_TYPE_VARCHAR: return "VARCHAR";
        case HW_MESSAGE_MAGIC_TYPE_ARRAY: return "ARRAY";
        case HW_MESSAGE_MAGIC_TYPE_OBJECT: return "OBJECT";
    }

    return "ERROR";
}
