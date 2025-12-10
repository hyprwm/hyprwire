#pragma once

#include <cstdint>

namespace Hyprwire {
    enum eMessageMagic : uint8_t {
        /*
            Signifies an end of a message
        */
        HW_MESSAGE_MAGIC_END = 0x0,

        /*
            Primitive type identifiers
        */
        HW_MESSAGE_MAGIC_TYPE_UINT      = 0x10,
        HW_MESSAGE_MAGIC_TYPE_INT       = 0x11,
        HW_MESSAGE_MAGIC_TYPE_F32       = 0x12,
        HW_MESSAGE_MAGIC_TYPE_SEQ       = 0x13,
        HW_MESSAGE_MAGIC_TYPE_OBJECT_ID = 0x14,

        /*
            Variable length types
        */

        /*
            [magic : 1B][len : VLQ][data : len B]
        */
        HW_MESSAGE_MAGIC_TYPE_VARCHAR = 0x20,

        /*
            [magic : 1B][type : 1B][n_els : VLQ]{ [data...] }
        */
        HW_MESSAGE_MAGIC_TYPE_ARRAY = 0x21,

        /*
            [magic : 1B][id : UINT][name_len : VLQ][object name ...]
        */
        HW_MESSAGE_MAGIC_TYPE_OBJECT = 0x22,

        /*
            Special types
        */

        // FD has size 0. It's passed via control.
        HW_MESSAGE_MAGIC_TYPE_FD = 0x40,
    };
};