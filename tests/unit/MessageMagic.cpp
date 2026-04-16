#include <gtest/gtest.h>

#include "core/message/MessageMagic.hpp"

#include <array>

TEST(MessageMagic, KnownValuesMapToReadableStrings) {
    using namespace Hyprwire;

    const std::array<eMessageMagic, 10> known = {
        HW_MESSAGE_MAGIC_END,         HW_MESSAGE_MAGIC_TYPE_UINT,      HW_MESSAGE_MAGIC_TYPE_INT,     HW_MESSAGE_MAGIC_TYPE_F32,
        HW_MESSAGE_MAGIC_TYPE_SEQ,    HW_MESSAGE_MAGIC_TYPE_OBJECT_ID, HW_MESSAGE_MAGIC_TYPE_VARCHAR, HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_OBJECT, HW_MESSAGE_MAGIC_TYPE_FD,
    };

    for (const auto magic : known) {
        const char* str = magicToString(magic);
        ASSERT_NE(str, nullptr);
        EXPECT_STRNE(str, "ERROR");
    }
}

TEST(MessageMagic, UnknownValueReturnsError) {
    using namespace Hyprwire;

    EXPECT_STREQ(magicToString(static_cast<eMessageMagic>(0xFF)), "ERROR");
}
