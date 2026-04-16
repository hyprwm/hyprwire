#include <gtest/gtest.h>

#include "core/message/MessageParser.hpp"
#include "core/message/messages/HandshakeAck.hpp"
#include "core/message/messages/HandshakeBegin.hpp"
#include "core/message/messages/HandshakeProtocols.hpp"
#include "core/message/messages/Hello.hpp"

#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

TEST(MessagesHelloHandshake, HelloCtorBuildsExpectedWireBytes) {
    CHelloMessage              msg;

    const std::vector<uint8_t> expected = {
        HW_MESSAGE_TYPE_SUP, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x03, 'V', 'A', 'X', HW_MESSAGE_MAGIC_END,
    };

    EXPECT_EQ(msg.m_data, expected);
}

TEST(MessagesHelloHandshake, HelloParserAcceptsValidAndRejectsInvalid) {
    const std::vector<uint8_t> valid = {
        HW_MESSAGE_TYPE_SUP, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x03, 'V', 'A', 'X', HW_MESSAGE_MAGIC_END,
    };

    CHelloMessage parsedValid(valid, 0);
    EXPECT_EQ(parsedValid.m_len, valid.size());

    auto invalid = valid;
    invalid[5]   = '!';
    CHelloMessage parsedInvalid(invalid, 0);
    EXPECT_EQ(parsedInvalid.m_len, 0);
}

TEST(MessagesHelloHandshake, HandshakeBeginRoundtripParsesVersions) {
    const std::vector<uint32_t> versions = {1, 2, 255};

    CHandshakeBeginMessage      out(versions);
    CHandshakeBeginMessage      in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_versionsSupported, versions);
}

TEST(MessagesHelloHandshake, HandshakeBeginRejectsWrongArrayType) {
    const std::vector<uint8_t> raw = {
        HW_MESSAGE_TYPE_HANDSHAKE_BEGIN, HW_MESSAGE_MAGIC_TYPE_ARRAY, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x00, HW_MESSAGE_MAGIC_END,
    };

    CHandshakeBeginMessage msg(raw, 0);
    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesHelloHandshake, HandshakeBeginRejectsTooManyVersions) {
    CMessageParser       parser;
    std::vector<uint8_t> raw = {
        HW_MESSAGE_TYPE_HANDSHAKE_BEGIN,
        HW_MESSAGE_MAGIC_TYPE_ARRAY,
        HW_MESSAGE_MAGIC_TYPE_UINT,
    };

    const auto encodedCount = parser.encodeVarInt(256);
    raw.insert(raw.end(), encodedCount.begin(), encodedCount.end());

    CHandshakeBeginMessage msg(raw, 0);
    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesHelloHandshake, HandshakeAckRoundtripParsesVersion) {
    CHandshakeAckMessage out(7);
    CHandshakeAckMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_version, 7);
}

TEST(MessagesHelloHandshake, HandshakeAckRejectsMalformedPayload) {
    const std::vector<uint8_t> raw = {
        HW_MESSAGE_TYPE_HANDSHAKE_ACK, HW_MESSAGE_MAGIC_TYPE_UINT, 0x01, 0x02, 0x03, 0x04, HW_MESSAGE_MAGIC_TYPE_UINT,
    };

    CHandshakeAckMessage msg(raw, 0);
    EXPECT_EQ(msg.m_len, 0);
}

TEST(MessagesHelloHandshake, HandshakeProtocolsRoundtripParsesProtocols) {
    const std::vector<std::string> protocols = {
        "test_protocol@1",
        "another@12",
    };

    CHandshakeProtocolsMessage out(protocols);
    CHandshakeProtocolsMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_protocols, protocols);
}

TEST(MessagesHelloHandshake, HandshakeProtocolsSupportsEmptyArray) {
    CHandshakeProtocolsMessage out(std::vector<std::string>{});
    CHandshakeProtocolsMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_TRUE(in.m_protocols.empty());
}
