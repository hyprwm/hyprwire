#include <gtest/gtest.h>

#include "core/message/MessageParser.hpp"
#include "core/message/messages/GenericProtocolMessage.hpp"

#include <hyprwire/core/types/MessageMagic.hpp>

#include <cstring>

using namespace Hyprwire;

static void appendU32(std::vector<uint8_t>& data, uint32_t value) {
    data.resize(data.size() + 4);
    std::memcpy(data.data() + data.size() - 4, &value, sizeof(value));
}

static std::vector<uint8_t> makeGenericHeader(uint32_t object, uint32_t method) {
    std::vector<uint8_t> data = {
        HW_MESSAGE_TYPE_GENERIC_PROTOCOL_MESSAGE,
        HW_MESSAGE_MAGIC_TYPE_OBJECT,
    };
    appendU32(data, object);
    data.push_back(HW_MESSAGE_MAGIC_TYPE_UINT);
    appendU32(data, method);
    return data;
}

TEST(MessagesGenericProtocol, ParsesObjectMethodAndPayloadSpan) {
    auto raw = makeGenericHeader(0xABCD, 3);

    raw.push_back(HW_MESSAGE_MAGIC_TYPE_UINT);
    appendU32(raw, 55);
    raw.push_back(HW_MESSAGE_MAGIC_END);

    std::vector<int>        fds;
    CGenericProtocolMessage msg(raw, fds, 0);

    ASSERT_EQ(msg.m_len, raw.size());
    EXPECT_EQ(msg.m_object, 0xABCDu);
    EXPECT_EQ(msg.m_method, 3u);
    ASSERT_FALSE(msg.m_dataSpan.empty());
    EXPECT_EQ(msg.m_dataSpan.front(), HW_MESSAGE_MAGIC_TYPE_UINT);
    EXPECT_EQ(msg.m_dataSpan.back(), HW_MESSAGE_MAGIC_END);
    EXPECT_TRUE(msg.m_fds.empty());
    EXPECT_TRUE(fds.empty());
}

TEST(MessagesGenericProtocol, ConsumesSingleFdToken) {
    auto raw = makeGenericHeader(1, 2);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_FD);
    raw.push_back(HW_MESSAGE_MAGIC_END);

    std::vector<int>        fds = {11};
    CGenericProtocolMessage msg(raw, fds, 0);

    ASSERT_EQ(msg.m_len, raw.size());
    ASSERT_EQ(msg.m_fds.size(), 1u);
    EXPECT_EQ(msg.m_fds[0], 11);
    EXPECT_TRUE(fds.empty());
}

TEST(MessagesGenericProtocol, ConsumesArrayFdTokens) {
    CMessageParser parser;
    auto           raw = makeGenericHeader(1, 2);

    raw.push_back(HW_MESSAGE_MAGIC_TYPE_ARRAY);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_FD);
    const auto count = parser.encodeVarInt(2);
    raw.insert(raw.end(), count.begin(), count.end());
    raw.push_back(HW_MESSAGE_MAGIC_END);

    std::vector<int>        fds = {4, 5};
    CGenericProtocolMessage msg(raw, fds, 0);

    ASSERT_EQ(msg.m_len, raw.size());
    ASSERT_EQ(msg.m_fds.size(), 2u);
    EXPECT_EQ(msg.m_fds[0], 4);
    EXPECT_EQ(msg.m_fds[1], 5);
    EXPECT_TRUE(fds.empty());
}

TEST(MessagesGenericProtocol, RejectsFdTokenWithEmptyFdQueue) {
    auto raw = makeGenericHeader(1, 2);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_FD);
    raw.push_back(HW_MESSAGE_MAGIC_END);

    std::vector<int>        fds;
    CGenericProtocolMessage msg(raw, fds, 0);

    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesGenericProtocol, RejectsInvalidArrayType) {
    auto raw = makeGenericHeader(1, 2);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_ARRAY);
    raw.push_back(HW_MESSAGE_MAGIC_END); // invalid element type
    raw.push_back(0x00);                 // arrLen varint = 0
    raw.push_back(HW_MESSAGE_MAGIC_END);

    std::vector<int>        fds;
    CGenericProtocolMessage msg(raw, fds, 0);

    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesGenericProtocol, RejectsTooLargeArray) {
    CMessageParser parser;
    auto           raw = makeGenericHeader(1, 2);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_ARRAY);
    raw.push_back(HW_MESSAGE_MAGIC_TYPE_UINT);

    const auto oversized = parser.encodeVarInt(10000);
    raw.insert(raw.end(), oversized.begin(), oversized.end());

    std::vector<int>        fds;
    CGenericProtocolMessage msg(raw, fds, 0);

    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesGenericProtocol, ResolveSeqUpdatesObjectAndSerializedPayload) {
    auto raw = makeGenericHeader(1, 9);
    raw.push_back(HW_MESSAGE_MAGIC_END);

    CGenericProtocolMessage msg(std::move(raw), std::vector<int>{});
    msg.resolveSeq(0xAABBCCDD);

    EXPECT_EQ(msg.m_object, 0xAABBCCDDu);

    uint32_t encodedId = 0;
    std::memcpy(&encodedId, msg.m_data.data() + 2, sizeof(encodedId));
    EXPECT_EQ(encodedId, 0xAABBCCDDu);
}
