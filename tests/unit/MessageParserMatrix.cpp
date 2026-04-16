#include <gtest/gtest.h>

#include "core/client/ClientSocket.hpp"
#include "core/message/MessageParser.hpp"
#include "core/message/MessageType.hpp"
#include "core/message/messages/GenericProtocolMessage.hpp"
#include "core/message/messages/BindProtocol.hpp"
#include "core/message/messages/FatalProtocolError.hpp"
#include "core/message/messages/HandshakeBegin.hpp"
#include "core/message/messages/HandshakeProtocols.hpp"
#include "core/message/messages/Hello.hpp"
#include "core/server/ServerClient.hpp"

#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

namespace {
    SP<CClientSocket> makeClientSocket() {
        auto client    = makeShared<CClientSocket>();
        client->m_self = client;
        return client;
    }

    SP<CServerClient> makeServerClient() {
        auto client    = makeShared<CServerClient>(-1);
        client->m_self = client;
        return client;
    }
}

TEST(MessageParserMatrix, ServerParserRejectsInvalidTypeCode) {
    CMessageParser          parser;
    auto                    serverClient = makeServerClient();

    SSocketRawParsedMessage raw = {
        .data = {0xFF},
    };

    EXPECT_EQ(parser.handleMessage(raw, serverClient), MESSAGE_PARSED_ERROR);
    EXPECT_TRUE(serverClient->m_error);
}

TEST(MessageParserMatrix, ServerParserReportsStrayFds) {
    CMessageParser          parser;
    auto                    serverClient = makeServerClient();

    SSocketRawParsedMessage raw = {
        .data = CHelloMessage().m_data,
        .fds  = {123},
    };

    EXPECT_EQ(parser.handleMessage(raw, serverClient), MESSAGE_PARSED_STRAY_FDS);
}

TEST(MessageParserMatrix, ServerParserRejectsMalformedBindProtocol) {
    CMessageParser          parser;
    auto                    serverClient = makeServerClient();

    SSocketRawParsedMessage raw = {
        .data = {HW_MESSAGE_TYPE_BIND_PROTOCOL, HW_MESSAGE_MAGIC_END},
    };

    EXPECT_EQ(parser.handleMessage(raw, serverClient), MESSAGE_PARSED_ERROR);
}

TEST(MessageParserMatrix, ClientParserRejectsUnsupportedVersionNegotiation) {
    CMessageParser          parser;
    auto                    client = makeClientSocket();

    CHandshakeBeginMessage  begin({9999});

    SSocketRawParsedMessage raw = {
        .data = begin.m_data,
    };

    EXPECT_EQ(parser.handleMessage(raw, client), MESSAGE_PARSED_ERROR);
    EXPECT_FALSE(client->m_handshakeDone);
}

TEST(MessageParserMatrix, ClientParserReportsStrayFds) {
    CMessageParser             parser;
    auto                       client = makeClientSocket();

    CHandshakeProtocolsMessage protocols(std::vector<std::string>{});

    SSocketRawParsedMessage    raw = {
        .data = protocols.m_data,
        .fds  = {11},
    };

    EXPECT_EQ(parser.handleMessage(raw, client), MESSAGE_PARSED_STRAY_FDS);
    EXPECT_TRUE(client->m_handshakeDone);
}

TEST(MessageParserMatrix, ClientParserRejectsInvalidTypeCode) {
    CMessageParser          parser;
    auto                    client = makeClientSocket();

    SSocketRawParsedMessage raw = {
        .data = {0xFF},
    };

    EXPECT_EQ(parser.handleMessage(raw, client), MESSAGE_PARSED_ERROR);
}

TEST(MessageParserMatrix, ClientParserHandlesFatalProtocolErrorAndFlagsClient) {
    CMessageParser          parser;
    auto                    client = makeClientSocket();

    CFatalErrorMessage      msg(7, 123, "boom");

    SSocketRawParsedMessage raw = {
        .data = msg.m_data,
    };

    EXPECT_EQ(parser.handleMessage(raw, client), MESSAGE_PARSED_OK);
    EXPECT_TRUE(client->m_error);
}
