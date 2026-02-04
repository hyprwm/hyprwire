#pragma once

#include <cstdint>

namespace Hyprwire {
    enum eMessageMagic : uint8_t {
        /*
            Signifies an end of a message
        */
        HW_MESSAGE_MAGIC_END = 0x0,

        /*
            Primitive type identifiers. These are all 4 bytes.
            SEQ and OBJECT_ID are U32.
        */
        HW_MESSAGE_MAGIC_TYPE_UINT      = 0x10,
        HW_MESSAGE_MAGIC_TYPE_INT       = 0x11,
        HW_MESSAGE_MAGIC_TYPE_F32       = 0x12,
        HW_MESSAGE_MAGIC_TYPE_SEQ       = 0x13,
        HW_MESSAGE_MAGIC_TYPE_OBJECT_ID = 0x14,

        /*
            Variable length types.

            VLQ = Variable Length Quantity. See https://en.wikipedia.org/wiki/Variable-length_quantity.
        */

        /*
            [magic : 1B][len : VLQ][data : len B]
        */
        HW_MESSAGE_MAGIC_TYPE_VARCHAR = 0x20,

        /*
            [magic : 1B][type : 1B][n_els : VLQ]{ [data...] }

            Note that ARRAY strips the magic from each element, as it's already in the arr.
            For example, for UINT data would be packs of 4 bytes.
            For a string, it's [len : VLQ][data : len B].
        */
        HW_MESSAGE_MAGIC_TYPE_ARRAY = 0x21,

        /*
            [magic : 1B][id : UINT][name_len : VLQ][object name ...]
        */
        HW_MESSAGE_MAGIC_TYPE_OBJECT = 0x22,

        /*
            Special types
        */

        /*
            [magic : 1B]

            FD has nothing but the magic. The FD is passed via control messages (sendmsg)
            and should be read with recvmsg.
        */
        HW_MESSAGE_MAGIC_TYPE_FD = 0x40,
    };
};