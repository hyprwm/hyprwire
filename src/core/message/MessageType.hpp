#pragma once

#include <cstdint>

namespace Hyprwire {
    enum eMessageType : uint8_t {
        HW_MESSAGE_TYPE_INVALID = 0,

        /*
            Sent by the client to initiate the handshake.
            No parameters.
        */
        HW_MESSAGE_TYPE_HELLO = 1,

        /*
            Sent by the server after a HELLO.
            Params: arr(uint) -> versions supported
        */
        HW_MESSAGE_TYPE_HANDSHAKE_BEGIN = 2,

        /*
            Sent by the client to confirm a choice of a protocol version
            Params: uint -> version chosen
        */
        HW_MESSAGE_TYPE_HANDSHAKE_ACK = 3,

        /*
            Sent by the server to advertise supported protocols
            Params: arr(str) -> protocols
        */
        HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS = 4,

        /*
            Sent by the client to bind to a specific protocol spec
            Params: str -> protocol spec
        */
        HW_MESSAGE_TYPE_BIND_PROTOCOL = 10,

        /*
            Sent by the server to acknowledge the bind and return a handle
            Params: uint -> object handle ID
        */
        HW_MESSAGE_TYPE_ACK_BIND_PROTOCOL = 11,

        /*
            Generic protocol message. Can be either direction.
            Params: uint -> object handle ID, uint -> method ID, data...
        */
        HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE = 100,
    };

    inline const char* messageTypeToStr(eMessageType t) {
        switch (t) {
            case HW_MESSAGE_TYPE_INVALID: return "INVALID";
            case HW_MESSAGE_TYPE_HELLO: return "HELLO";
            case HW_MESSAGE_TYPE_HANDSHAKE_BEGIN: return "HANDSHAKE_BEGIN";
            case HW_MESSAGE_TYPE_HANDSHAKE_ACK: return "HANDSHAKE_ACK";
            case HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS: return "HANDSHAKE_PROTOCOLS";
            case HW_MESSAGE_TYPE_BIND_PROTOCOL: return "BIND_PROTOCOL";
            case HW_MESSAGE_TYPE_ACK_BIND_PROTOCOL: return "ACK_BIND_PROTOCOL";
            case HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE: return "GENERIC_PROTOCOL_MESSAGE";
        }
        return "ERROR";
    }
};