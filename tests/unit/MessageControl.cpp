#include <gtest/gtest.h>

#include "core/message/messages/BindProtocol.hpp"
#include "core/message/messages/FatalProtocolError.hpp"
#include "core/message/messages/NewObject.hpp"
#include "core/message/messages/RoundtripDone.hpp"
#include "core/message/messages/RoundtripRequest.hpp"

#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

TEST(MessagesControl, BindProtocolRoundtripParsesFields) {
    CBindProtocolMessage out("my_proto", 42, 7);
    CBindProtocolMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_protocol, "my_proto");
    EXPECT_EQ(in.m_seq, 42);
    EXPECT_EQ(in.m_version, 7);
}

TEST(MessagesControl, BindProtocolRejectsZeroVersion) {
    CBindProtocolMessage out("my_proto", 42, 0);
    CBindProtocolMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, 0);
}

TEST(MessagesControl, NewObjectRoundtripParsesFields) {
    CNewObjectMessage out(123, 0xBEEF);
    CNewObjectMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_seq, 123);
    EXPECT_EQ(in.m_id, 0xBEEF);
}

TEST(MessagesControl, FatalErrorRoundtripParsesFields) {
    CFatalErrorMessage out(nullptr, 99, "boom");
    CFatalErrorMessage in(out.m_data, 0);

    EXPECT_EQ(in.m_len, out.m_data.size());
    EXPECT_EQ(in.m_objectId, 0u);
    EXPECT_EQ(in.m_errorId, 99u);
    EXPECT_EQ(in.m_errorMsg, "boom");
}

TEST(MessagesControl, RoundtripMessagesRoundtripParsesSeq) {
    CRoundtripRequestMessage reqOut(777);
    CRoundtripRequestMessage reqIn(reqOut.m_data, 0);

    EXPECT_EQ(reqIn.m_len, reqOut.m_data.size());
    EXPECT_EQ(reqIn.m_seq, 777u);

    CRoundtripDoneMessage doneOut(888);
    CRoundtripDoneMessage doneIn(doneOut.m_data, 0);

    EXPECT_EQ(doneIn.m_len, doneOut.m_data.size());
    EXPECT_EQ(doneIn.m_seq, 888u);
}

TEST(MessagesControl, RoundtripRejectsMalformedPayload) {
    const std::vector<uint8_t> badReq = {
        HW_MESSAGE_TYPE_ROUNDTRIP_REQUEST, HW_MESSAGE_MAGIC_TYPE_VARCHAR, 0x01, 'x', HW_MESSAGE_MAGIC_END,
    };

    const std::vector<uint8_t> badDone = {
        HW_MESSAGE_TYPE_ROUNDTRIP_DONE, HW_MESSAGE_MAGIC_TYPE_UINT, 0x01, 0x02, 0x03, 0x04, HW_MESSAGE_MAGIC_TYPE_UINT,
    };

    CRoundtripRequestMessage req(badReq, 0);
    CRoundtripDoneMessage    done(badDone, 0);

    EXPECT_EQ(req.m_len, 0);
    EXPECT_EQ(done.m_len, 0);
}
