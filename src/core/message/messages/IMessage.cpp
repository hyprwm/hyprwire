#include "IMessage.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Memory.hpp"

#include <cstring>
#include <format>
#include <string_view>

#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

static std::pair<std::string, size_t> formatPrimitiveType(const std::span<const uint8_t>& s, eMessageMagic type) {
    switch (type) {
        case HW_MESSAGE_MAGIC_TYPE_UINT: {
            if (s.size() < 4)
                return {"", 0};
            uint32_t val = 0;
            std::memcpy(&val, &s[0], sizeof(val));
            return {std::format("{}", val), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_INT: {
            if (s.size() < 4)
                return {"", 0};
            int32_t val = 0;
            std::memcpy(&val, &s[0], sizeof(val));
            return {std::format("{}", val), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_F32: {
            if (s.size() < 4)
                return {"", 0};
            float val = 0;
            std::memcpy(&val, &s[0], sizeof(val));
            return {std::format("{}", val), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_FD: {
            return {"<fd>", 0};
        }
        case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
            if (s.size() < 4)
                return {"", 0};
            uint32_t id = 0;
            std::memcpy(&id, &s[0], sizeof(id));
            return {std::format("object: {}", id == 0 ? "null" : std::to_string(id)), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
            auto [len, intLen] = g_messageParser->parseVarInt(std::vector<uint8_t>(s.data(), s.data() + s.size() - 1), 0);
            auto ptr           = rc<const char*>(&s[intLen]);
            return {std::format("\"{}\"", std::string_view{ptr, len}), len + intLen};
        }
        default: break;
    }

    return {"", 0};
}

std::string IMessage::parseData() const {
    std::string result;
    result += messageTypeToStr(m_type);
    result += " ( ";

    size_t needle = 1;
    while (needle < m_data.size()) {
        switch (sc<eMessageMagic>(m_data.at(needle++))) {
            case HW_MESSAGE_MAGIC_END: {
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_SEQ: {
                int32_t seq = 0;
                if (m_data.size() - needle >= 4) {
                    std::memcpy(&seq, &m_data.at(needle), 4);
                    result += std::format("seq: {}", seq);
                    needle += 4;
                }
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_UINT: {
                uint32_t val = 0;
                if (m_data.size() - needle >= 4) {
                    std::memcpy(&val, &m_data.at(needle), 4);
                    result += std::format("{}", val);
                    needle += 4;
                }
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_INT: {
                int32_t val = 0;
                if (m_data.size() - needle >= 4) {
                    std::memcpy(&val, &m_data.at(needle), 4);
                    result += std::format("{}", val);
                    needle += 4;
                }
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_F32: {
                float val = 0;
                if (m_data.size() - needle >= 4) {
                    std::memcpy(&val, &m_data.at(needle), 4);
                    result += std::format("{}", val);
                    needle += 4;
                }
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                auto [len, intLen] = g_messageParser->parseVarInt(m_data, needle);
                if (len > 0) {
                    auto ptr = rc<const char*>(&m_data.at(needle + intLen));
                    result += std::format("\"{}\"", std::string_view{ptr, len});
                } else
                    result += "\"\"";
                needle += intLen + len;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                auto thisType      = sc<eMessageMagic>(m_data.at(needle++));
                auto [els, intLen] = g_messageParser->parseVarInt(m_data, needle);
                result += "{ ";
                needle += intLen;

                for (size_t i = 0; i < els; ++i) {
                    auto [str, len] = formatPrimitiveType(std::span<const uint8_t>{&m_data[needle], m_data.size() - needle}, thisType);

                    needle += len;

                    result += std::format("{}, ", str);
                }
                if (els != 0) {
                    result.pop_back();
                    result.pop_back();
                }

                result += " }";
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
                uint32_t id = 0;
                if (m_data.size() - needle >= 4) {
                    std::memcpy(&id, &m_data.at(needle), 4);
                    needle += 4;
                    result += std::format("object({})", id);
                }
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_FD: {
                result += "<fd>";
                break;
            }
            default: break;
        }

        result += ", ";
    }

    if (result.at(result.size() - 2) == ',') {
        result.pop_back();
        result.pop_back();
    }
    if (result.at(result.size() - 2) == ',') {
        result.pop_back();
        result.pop_back();
    }

    result += " ) ";
    return result;
}

const std::vector<int>& IMessage::fds() const {
    static const std::vector<int> emptyVec;
    return emptyVec;
}
