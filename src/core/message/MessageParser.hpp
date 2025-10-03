#pragma once

#include <vector>

#include "../../helpers/Memory.hpp"

namespace Hyprwire {
    class CServerClient;
    class CClientSocket;

    enum eMessageParsingResult : uint8_t {
        MESSAGE_PARSED_OK         = 0,
        MESSAGE_PARSED_ERROR      = 1,
        MESSAGE_PARSED_INCOMPLETE = 2,
    };

    class CMessageParser {
      public:
        CMessageParser()  = default;
        ~CMessageParser() = default;

        eMessageParsingResult     handleMessage(const std::vector<uint8_t>& data, SP<CServerClient> client);
        eMessageParsingResult     handleMessage(const std::vector<uint8_t>& data, SP<CClientSocket> client);
        std::pair<size_t, size_t> parseVarInt(const std::vector<uint8_t>& data, size_t offset);
        std::vector<uint8_t>      encodeVarInt(size_t num);

      private:
        size_t parseSingleMessage(const std::vector<uint8_t>& data, size_t off, SP<CServerClient> client);
        size_t parseSingleMessage(const std::vector<uint8_t>& data, size_t off, SP<CClientSocket> client);
    };

    inline UP<CMessageParser> g_messageParser;
};