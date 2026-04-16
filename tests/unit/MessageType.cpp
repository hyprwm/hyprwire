#include <gtest/gtest.h>

#include "core/message/MessageType.hpp"

#include <array>

TEST(MessageType, KnownValuesMapToStrings) {
    using namespace Hyprwire;

    const std::array<eMessageType, 10> known = {
        HW_MESSAGE_TYPE_INVALID,       HW_MESSAGE_TYPE_SUP,        HW_MESSAGE_TYPE_HANDSHAKE_BEGIN,      HW_MESSAGE_TYPE_HANDSHAKE_ACK,     HW_MESSAGE_TYPE_HANDSHAKE_PROTOCOLS,
        HW_MESSAGE_TYPE_BIND_PROTOCOL, HW_MESSAGE_TYPE_NEW_OBJECT, HW_MESSAGE_TYPE_FATAL_PROTOCOL_ERROR, HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST, HW_MESSAGE_TYPE_ROUNDTRIP_DONE,
    };

    for (const auto type : known) {
        const char* str = messageTypeToStr(type);
        ASSERT_NE(str, nullptr);
        EXPECT_STRNE(str, "ERROR");
    }

    EXPECT_STREQ(messageTypeToStr(HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE), "GENERIC_PROTOCOL_MESSAGE");
}

TEST(MessageType, UnknownValueReturnsError) {
    using namespace Hyprwire;

    EXPECT_STREQ(messageTypeToStr(static_cast<eMessageType>(0xFF)), "ERROR");
}
