#pragma once

#include <vector>
#include <span>

#include "../../helpers/Memory.hpp"
#include "../socket/SocketHelpers.hpp"

namespace Hyprwire {
    class CServerClient;
    class CClientSocket;

    enum eMessageParsingResult : uint8_t {
        MESSAGE_PARSED_OK         = 0,
        MESSAGE_PARSED_ERROR      = 1,
        MESSAGE_PARSED_INCOMPLETE = 2,
        MESSAGE_PARSED_STRAY_FDS  = 3,
    };

    class CMessageParser {
      public:
        CMessageParser()  = default;
        ~CMessageParser() = default;

        eMessageParsingResult     handleMessage(SSocketRawParsedMessage& data, SP<CServerClient> client);
        eMessageParsingResult     handleMessage(SSocketRawParsedMessage& data, SP<CClientSocket> client);
        std::pair<size_t, size_t> parseVarInt(const std::vector<uint8_t>& data, size_t offset);
        std::pair<size_t, size_t> parseVarInt(const std::span<const uint8_t>& data);
        std::vector<uint8_t>      encodeVarInt(size_t num);

      private:
        size_t parseSingleMessage(SSocketRawParsedMessage& data, size_t off, SP<CServerClient> client);
        size_t parseSingleMessage(SSocketRawParsedMessage& data, size_t off, SP<CClientSocket> client);
    };

    inline UP<CMessageParser> g_messageParser = makeUnique<CMessageParser>();
};
