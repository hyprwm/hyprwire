#include <gtest/gtest.h>

#include "helpers/Env.hpp"
#include "helpers/FFI.hpp"

#include <ffi.h>
#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstdlib>

using namespace Hyprwire;

TEST(FFI, MapsKnownMagicTypesToExpectedFfiTypes) {
    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_UINT), &ffi_type_uint32);
    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_OBJECT), &ffi_type_uint32);
    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_SEQ), &ffi_type_uint32);

    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_INT), &ffi_type_sint32);
    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_FD), &ffi_type_sint32);

    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_F32), &ffi_type_float);

    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_VARCHAR), &ffi_type_pointer);
    EXPECT_EQ(FFI::ffiTypeFrom(HW_MESSAGE_MAGIC_TYPE_ARRAY), &ffi_type_pointer);
}

TEST(FFI, UnknownMagicReturnsNull) {
    EXPECT_EQ(FFI::ffiTypeFrom(static_cast<eMessageMagic>(0xFF)), nullptr);
}

TEST(Env, EnvEnabledFollowsVariableContents) {
    constexpr const char* name = "HW_TEST_ENV_ENABLED";

    unsetenv(name);
    EXPECT_FALSE(Env::envEnabled(name));

    setenv(name, "0", 1);
    EXPECT_FALSE(Env::envEnabled(name));

    setenv(name, "1", 1);
    EXPECT_TRUE(Env::envEnabled(name));

    setenv(name, "hello", 1);
    EXPECT_TRUE(Env::envEnabled(name));

    unsetenv(name);
}

TEST(Env, TraceCacheCanBeResetForDeterministicTests) {
    constexpr const char* traceName = "HW_TRACE";

    unsetenv(traceName);
    Env::resetTraceCache();
    EXPECT_FALSE(Env::isTrace());

    setenv(traceName, "1", 1);
    EXPECT_FALSE(Env::isTrace()) << "isTrace should stay cached until reset";

    Env::resetTraceCache();
    EXPECT_TRUE(Env::isTrace());

    setenv(traceName, "0", 1);
    EXPECT_TRUE(Env::isTrace()) << "isTrace should stay cached until reset";

    Env::resetTraceCache();
    EXPECT_FALSE(Env::isTrace());

    unsetenv(traceName);
    Env::resetTraceCache();
}
