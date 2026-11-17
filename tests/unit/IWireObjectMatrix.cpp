#include <gtest/gtest.h>

#include "core/message/MessageParser.hpp"
#include "core/message/MessageType.hpp"
#include "core/message/messages/IMessage.hpp"
#include "core/wireObject/IWireObject.hpp"

#include <hyprwire/core/types/MessageMagic.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

using namespace Hyprwire;

namespace {

    template <typename Fn>
    void* fnToVoid(Fn fn) {
        static_assert(std::is_pointer_v<Fn>);
        static_assert(sizeof(Fn) == sizeof(void*));

        union {
            Fn    f;
            void* p;
        } caster = {
            .f = fn,
        };

        return caster.p;
    }

    class CTestWireObject final : public IWireObject {
      public:
        explicit CTestWireObject(bool isServer) : m_server(isServer) {
            ;
        }

        const std::vector<SMethod>& methodsOut() override {
            return m_methodsOut;
        }

        const std::vector<SMethod>& methodsIn() override {
            return m_methodsIn;
        }

        void errd() override {
            m_errd = true;
        }

        void sendMessage(const IMessage& msg) override {
            m_lastSentData = msg.m_data;
            m_lastSentFds  = msg.fds();
        }

        bool server() override {
            return m_server;
        }

        SP<IObject> self() override {
            return m_self.lock();
        }

        SP<IServerClient> client() override {
            return nullptr;
        }

        void error(uint32_t id, const std::string_view& message) override {
            m_lastErrorId  = id;
            m_lastErrorMsg = std::string{message};
        }

      public:
        bool                 m_server = false;
        bool                 m_errd   = false;

        uint32_t             m_lastErrorId = 0;
        std::string          m_lastErrorMsg;

        std::vector<uint8_t> m_lastSentData;
        std::vector<int>     m_lastSentFds;

        std::vector<SMethod> m_methodsOut;
        std::vector<SMethod> m_methodsIn;
    };

    SP<CTestWireObject> makeObject(bool isServer = false) {
        auto obj    = makeShared<CTestWireObject>(isServer);
        obj->m_self = reinterpretPointerCast<IWireObject>(obj);
        return obj;
    }

    int  g_destructorListenerCalls = 0;

    void onNoop(IObject*) {
        ;
    }

    void onDestructor(IObject*) {
        ++g_destructorListenerCalls;
    }

    struct SStringListenerState {
        size_t                   calls = 0;
        std::vector<std::string> values;
    };

    void onString(IObject* object, const char* value) {
        auto* state = static_cast<SStringListenerState*>(object->getData());
        ++state->calls;
        state->values = {value};
    }

    void onStringArray(IObject* object, const char** values, uint32_t count) {
        auto* state = static_cast<SStringListenerState*>(object->getData());
        ++state->calls;
        state->values.clear();
        for (size_t i = 0; i < count; ++i)
            state->values.emplace_back(values[i]);
    }

    std::vector<uint8_t> makeStringArguments(bool array, const std::vector<std::string>& values) {
        CMessageParser       parser;
        std::vector<uint8_t> data;
        if (array) {
            data = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR};
            data.append_range(parser.encodeVarInt(values.size()));
        }

        for (const auto& value : values) {
            if (!array)
                data.emplace_back(HW_MESSAGE_MAGIC_TYPE_VARCHAR);
            data.append_range(parser.encodeVarInt(value.size()));
            data.append_range(value);
        }

        data.emplace_back(HW_MESSAGE_MAGIC_END);
        return data;
    }

} // namespace

TEST(IWireObjectMatrix, CallRejectsInvalidMethodIndex) {
    auto obj  = makeObject();
    obj->m_id = 77;

    EXPECT_EQ(obj->call(0), 0u);
    EXPECT_NE(obj->m_lastErrorMsg.find("invalid method"), std::string::npos);
}

TEST(IWireObjectMatrix, CallRejectsMethodSinceNewerThanObjectVersion) {
    auto obj          = makeObject();
    obj->m_id         = 55;
    obj->m_version    = 1;
    obj->m_methodsOut = {
        SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 3},
    };

    EXPECT_EQ(obj->call(0), 0u);
    EXPECT_NE(obj->m_lastErrorMsg.find("since"), std::string::npos);
}

TEST(IWireObjectMatrix, ServerSideCallRejectsReturnsTypeMethods) {
    auto obj          = makeObject(true);
    obj->m_id         = 5;
    obj->m_methodsOut = {
        SMethod{.idx = 0, .params = {}, .returnsType = "child", .since = 0},
    };

    EXPECT_EQ(obj->call(0), 0u);
    EXPECT_NE(obj->m_lastErrorMsg.find("server cannot call returnsType methods"), std::string::npos);
}

TEST(IWireObjectMatrix, CallFailsForUnsupportedArrayElementType) {
    auto obj          = makeObject();
    obj->m_id         = 12;
    obj->m_methodsOut = {
        SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_OBJECT_ID}, .returnsType = "", .since = 0},
    };

    uint32_t dummy = 1;
    EXPECT_EQ(obj->call(0, &dummy, static_cast<uint32_t>(1)), 0u);
    EXPECT_TRUE(obj->m_errd);
}

TEST(IWireObjectMatrix, CallMarksObjectDestroyedWhenDestructorMethodIsCalled) {
    auto obj          = makeObject();
    obj->m_id         = 44;
    obj->m_methodsOut = {
        SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 0, .isDestructor = true},
    };

    EXPECT_FALSE(obj->m_destroyed);
    EXPECT_EQ(obj->call(0), 0u);
    EXPECT_TRUE(obj->m_destroyed);
    ASSERT_FALSE(obj->m_lastSentData.empty());
    EXPECT_EQ(obj->m_lastSentData[0], HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE);
}

TEST(IWireObjectMatrix, CalledRejectsInvalidMethodIndex) {
    auto obj  = makeObject();
    obj->m_id = 91;

    const std::array<uint8_t, 1> data = {HW_MESSAGE_MAGIC_END};
    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("invalid method"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledRejectsMethodSinceNewerThanObjectVersion) {
    auto obj         = makeObject();
    obj->m_id        = 77;
    obj->m_version   = 1;
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 2},
    };
    obj->listen(0, fnToVoid(&onNoop));

    const std::array<uint8_t, 1> data = {HW_MESSAGE_MAGIC_END};
    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("since"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledRejectsTypeMismatchBetweenSpecAndWire) {
    auto obj         = makeObject();
    obj->m_id        = 73;
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
    };
    obj->listen(0, fnToVoid(&onNoop));

    const std::array<uint8_t, 6> badData = {
        HW_MESSAGE_MAGIC_TYPE_INT, 0, 0, 0, 0, HW_MESSAGE_MAGIC_END,
    };

    obj->called(0, std::span<const uint8_t>{badData.data(), badData.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("should be"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledRejectsArrayWireTypeMismatch) {
    auto obj         = makeObject();
    obj->m_id        = 12;
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
    };
    obj->listen(0, fnToVoid(&onNoop));

    const std::array<uint8_t, 4> badData = {
        HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_INT,
        0x00,
        HW_MESSAGE_MAGIC_END,
    };

    obj->called(0, std::span<const uint8_t>{badData.data(), badData.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("should be"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledRejectsOversizedArrayPayload) {
    auto obj         = makeObject();
    obj->m_id        = 88;
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_UINT}, .returnsType = "", .since = 0},
    };
    obj->listen(0, fnToVoid(&onNoop));

    CMessageParser       parser;
    auto                 lenVarInt = parser.encodeVarInt(10001);

    std::vector<uint8_t> data = {
        HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_UINT,
    };
    data.insert(data.end(), lenVarInt.begin(), lenVarInt.end());
    data.push_back(HW_MESSAGE_MAGIC_END);

    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("max array size"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledRejectsObjectIdMagicType) {
    auto obj         = makeObject();
    obj->m_id        = 19;
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_OBJECT_ID}, .returnsType = "", .since = 0},
    };
    obj->listen(0, fnToVoid(&onNoop));

    const std::array<uint8_t, 1> data = {HW_MESSAGE_MAGIC_TYPE_OBJECT_ID};
    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_NE(obj->m_lastErrorMsg.find("object type is not impld"), std::string::npos);
}

TEST(IWireObjectMatrix, CalledMarksDestroyedForDestructorWithoutListener) {
    auto obj         = makeObject();
    obj->m_id        = 0; // avoid concrete cast path
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 0, .isDestructor = true},
    };

    const std::array<uint8_t, 1> data = {HW_MESSAGE_MAGIC_END};
    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_TRUE(obj->m_destroyed);
}

TEST(IWireObjectMatrix, CalledMarksDestroyedForDestructorWithListener) {
    auto obj         = makeObject();
    obj->m_id        = 0; // avoid concrete cast path
    obj->m_methodsIn = {
        SMethod{.idx = 0, .params = {}, .returnsType = "", .since = 0, .isDestructor = true},
    };
    obj->listen(0, fnToVoid(&onDestructor));

    g_destructorListenerCalls = 0;

    const std::array<uint8_t, 1> data = {HW_MESSAGE_MAGIC_END};
    obj->called(0, std::span<const uint8_t>{data.data(), data.size()}, {});

    EXPECT_EQ(g_destructorListenerCalls, 1);
    EXPECT_TRUE(obj->m_destroyed);
}

TEST(IWireObjectMatrix, CalledRejectsEmbeddedNulStringsBeforeCallbackOrDestruction) {
    const std::vector<std::string> invalidStrings = {
        std::string{"\0abc", 4},
        std::string{"a\0bc", 4},
        std::string{"abc\0", 4},
    };

    for (const bool array : {false, true}) {
        for (const bool withListener : {false, true}) {
            for (const auto& value : invalidStrings) {
                // Also reject a bad array element after a valid one.
                for (size_t validPrefix = 0; validPrefix <= static_cast<size_t>(array); ++validPrefix) {
                    SCOPED_TRACE(::testing::Message() << "array=" << array << " listener=" << withListener << " prefix=" << validPrefix
                                                      << " value=" << ::testing::PrintToString(value));
                    SStringListenerState state;
                    auto                 obj = makeObject();
                    obj->setData(&state);
                    obj->m_methodsIn = {
                        SMethod{.idx = 0,
                                .params =
                                    array ? std::vector<uint8_t>{HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR} : std::vector<uint8_t>{HW_MESSAGE_MAGIC_TYPE_VARCHAR},
                                .isDestructor = true},
                    };
                    if (withListener)
                        obj->listen(0, array ? fnToVoid(&onStringArray) : fnToVoid(&onString));

                    std::vector<std::string> values(validPrefix, "valid");
                    values.emplace_back(value);
                    const auto data = makeStringArguments(array, values);
                    obj->called(0, data, {});

                    EXPECT_NE(obj->m_lastErrorMsg.find("NUL-free VARCHAR"), std::string::npos);
                    EXPECT_EQ(state.calls, 0u);
                    EXPECT_FALSE(obj->m_destroyed);
                }
            }
        }
    }
}

TEST(IWireObjectMatrix, CalledAcceptsEmptyAndNormalStrings) {
    for (const bool withListener : {false, true}) {
        for (const auto& value : {std::string{}, std::string{"hello"}, std::string(128, 'x')}) {
            SCOPED_TRACE(::testing::Message() << "listener=" << withListener << " length=" << value.size());
            SStringListenerState state;
            auto                 obj = makeObject();
            obj->setData(&state);
            obj->m_methodsIn = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_VARCHAR}, .isDestructor = true},
            };
            if (withListener)
                obj->listen(0, fnToVoid(&onString));

            const auto data = makeStringArguments(false, {value});
            obj->called(0, data, {});

            EXPECT_TRUE(obj->m_lastErrorMsg.empty());
            EXPECT_EQ(state.calls, withListener ? 1u : 0u);
            if (withListener) {
                EXPECT_EQ(state.values, std::vector<std::string>{value});
            }
            EXPECT_TRUE(obj->m_destroyed);
        }
    }
}

TEST(IWireObjectMatrix, CalledAcceptsEmptyAndNormalStringArrays) {
    const std::vector<std::vector<std::string>> cases = {
        {},
        {""},
        {"hello"},
        {"", "hello", "", std::string(128, 'x')},
    };

    for (const bool withListener : {false, true}) {
        for (const auto& values : cases) {
            SCOPED_TRACE(::testing::Message() << "listener=" << withListener << " values=" << ::testing::PrintToString(values));
            SStringListenerState state;
            auto                 obj = makeObject();
            obj->setData(&state);
            obj->m_methodsIn = {
                SMethod{.idx = 0, .params = {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR}, .isDestructor = true},
            };
            if (withListener)
                obj->listen(0, fnToVoid(&onStringArray));

            const auto data = makeStringArguments(true, values);
            obj->called(0, data, {});

            EXPECT_TRUE(obj->m_lastErrorMsg.empty());
            EXPECT_EQ(state.calls, withListener ? 1u : 0u);
            if (withListener) {
                EXPECT_EQ(state.values, values);
            }
            EXPECT_TRUE(obj->m_destroyed);
        }
    }
}

TEST(IWireObjectMatrix, CalledRejectsMalformedStringArgumentsBeforeCallbackOrDestruction) {
    struct SCase {
        bool                 array = false;
        std::vector<uint8_t> data;
    };

    const std::vector<SCase> cases = {
        {false, {}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x80}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x80, 0x80, 0x80, 0x80, 0}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 3, 'a', 'b'}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 'a'}},
        {false, {HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 0, HW_MESSAGE_MAGIC_END}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x80}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x80, 0x80, 0x80, 0x80, 0}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 0x80}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 0x80, 0x80, 0x80, 0x80, 0}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 2, 'a'}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 2, 1, 'a'}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 2, 1, 'a', 0x80}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 2, 1, 'a', 2, 'b'}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0}},
        {true, {HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 1, 1, 'a'}},
    };

    for (const bool withListener : {false, true}) {
        for (const auto& [array, data] : cases) {
            SCOPED_TRACE(::testing::Message() << "listener=" << withListener << " data=" << ::testing::PrintToString(data));
            SStringListenerState state;
            auto                 obj = makeObject();
            obj->setData(&state);
            obj->m_methodsIn = {
                SMethod{.idx    = 0,
                        .params = array ? std::vector<uint8_t>{HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR} : std::vector<uint8_t>{HW_MESSAGE_MAGIC_TYPE_VARCHAR},
                        .isDestructor = true},
            };
            if (withListener)
                obj->listen(0, array ? fnToVoid(&onStringArray) : fnToVoid(&onString));

            obj->called(0, data, {});

            EXPECT_FALSE(obj->m_lastErrorMsg.empty());
            EXPECT_EQ(state.calls, 0u);
            EXPECT_FALSE(obj->m_destroyed);
        }
    }
}
