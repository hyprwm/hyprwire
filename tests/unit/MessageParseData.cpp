#include <gtest/gtest.h>

#include "core/message/messages/BindProtocol.hpp"
#include "core/message/messages/FatalProtocolError.hpp"
#include "core/message/messages/HandshakeAck.hpp"
#include "core/message/messages/HandshakeProtocols.hpp"
#include "core/message/messages/Hello.hpp"
#include "core/message/messages/NewObject.hpp"
#include "core/message/messages/RoundtripDone.hpp"
#include "core/message/messages/RoundtripRequest.hpp"

using namespace Hyprwire;

TEST(IMessageParseData, HelloContainsTypeAndPayloadString) {
    CHelloMessage msg;
    const auto    parsed = msg.parseData();

    EXPECT_NE(parsed.find("SUP"), std::string::npos);
    EXPECT_NE(parsed.find("\"VAX\""), std::string::npos);
}

TEST(IMessageParseData, HandshakeAckContainsTypeAndVersion) {
    CHandshakeAckMessage msg(7);
    const auto           parsed = msg.parseData();

    EXPECT_NE(parsed.find("HANDSHAKE_ACK"), std::string::npos);
    EXPECT_NE(parsed.find('7'), std::string::npos);
}

TEST(IMessageParseData, HandshakeProtocolsContainsProtocolNames) {
    CHandshakeProtocolsMessage msg(std::vector<std::string>{"proto@1", "second@2"});
    const auto                 parsed = msg.parseData();

    EXPECT_NE(parsed.find("HANDSHAKE_PROTOCOLS"), std::string::npos);
    EXPECT_NE(parsed.find("\"proto@1\""), std::string::npos);
    EXPECT_NE(parsed.find("\"second@2\""), std::string::npos);
}

TEST(IMessageParseData, BindProtocolContainsCoreFields) {
    CBindProtocolMessage msg("my_proto", 12, 3);
    const auto           parsed = msg.parseData();

    EXPECT_NE(parsed.find("BIND_PROTOCOL"), std::string::npos);
    EXPECT_NE(parsed.find("12"), std::string::npos);
    EXPECT_NE(parsed.find("\"my_proto\""), std::string::npos);
    EXPECT_NE(parsed.find('3'), std::string::npos);
}

TEST(IMessageParseData, NewObjectContainsObjectAndSeq) {
    CNewObjectMessage msg(9, 77);
    const auto        parsed = msg.parseData();

    EXPECT_NE(parsed.find("NEW_OBJECT"), std::string::npos);
    EXPECT_NE(parsed.find("77"), std::string::npos);
    EXPECT_NE(parsed.find('9'), std::string::npos);
}

TEST(IMessageParseData, FatalErrorContainsIdentifiersAndMessage) {
    CFatalErrorMessage msg(nullptr, 123, "oops");
    const auto         parsed = msg.parseData();

    EXPECT_NE(parsed.find("PROTOCOL_ERROR"), std::string::npos);
    EXPECT_NE(parsed.find("123"), std::string::npos);
    EXPECT_NE(parsed.find("\"oops\""), std::string::npos);
}

TEST(IMessageParseData, RoundtripMessagesContainTypeAndSequence) {
    CRoundtripRequestMessage req(777);
    CRoundtripDoneMessage    done(888);

    const auto               reqParsed  = req.parseData();
    const auto               doneParsed = done.parseData();

    EXPECT_NE(reqParsed.find("ROUNDTRIP_REQUEST"), std::string::npos);
    EXPECT_NE(reqParsed.find("777"), std::string::npos);

    EXPECT_NE(doneParsed.find("ROUNDTRIP_DONE"), std::string::npos);
    EXPECT_NE(doneParsed.find("888"), std::string::npos);
}
