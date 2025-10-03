#include "IMessage.hpp"
#include "../MessageParser.hpp"
#include "../../../helpers/Memory.hpp"

#include <format>
#include <string_view>

#include <hyprwire/core/types/MessageMagic.hpp>

using namespace Hyprwire;

static std::pair<std::string, size_t> formatPrimitiveType(const std::span<uint8_t>& s, eMessageMagic type) {
    switch (type) {
        case HW_MESSAGE_MAGIC_TYPE_UINT: {
            return {std::format("{}", *rc<uint32_t*>(&s[0])), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_INT: {
            return {std::format("{}", *rc<int32_t*>(&s[0])), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_F32: {
            return {std::format("{}", *rc<float*>(&s[0])), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_OBJECT: {
            auto id = *rc<uint32_t*>(&s[0]);
            return {std::format("object: {}", id == 0 ? "null" : std::to_string(id)), 4};
        }
        case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
            auto [len, intLen] = g_messageParser->parseVarInt(std::vector<uint8_t>(s.data(), s.data() + s.size() - 1), 0);
            auto ptr           = rc<const char*>(&s[intLen]);
            return {std::format("\"{}\"", std::string_view{ptr, len}), len + intLen};
        }
    }

    return {"", 0};
}

std::string IMessage::parseData() {
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
                result += std::format("seq: {}", *rc<uint32_t*>(&m_data.at(needle)));
                needle += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_UINT: {
                result += std::format("{}", *rc<uint32_t*>(&m_data.at(needle)));
                needle += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_INT: {
                result += std::format("{}", *rc<int32_t*>(&m_data.at(needle)));
                needle += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_F32: {
                result += std::format("{}", *rc<float*>(&m_data.at(needle)));
                needle += 4;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_VARCHAR: {
                auto [len, intLen] = g_messageParser->parseVarInt(m_data, needle);
                auto ptr           = rc<const char*>(&m_data.at(needle + intLen));
                result += std::format("\"{}\"", std::string_view{ptr, len});
                needle += intLen + len;
                break;
            }
            case HW_MESSAGE_MAGIC_TYPE_ARRAY: {
                auto thisType      = sc<eMessageMagic>(m_data.at(needle++));
                auto [els, intLen] = g_messageParser->parseVarInt(m_data, needle);
                result += "{ ";
                needle += intLen;

                for (size_t i = 0; i < els; ++i) {
                    auto [str, len] = formatPrimitiveType(std::span<uint8_t>{&m_data[needle], m_data.size() - needle}, thisType);

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
                auto id = *rc<uint32_t*>(&m_data.at(needle));
                needle += 4;
                auto [strLen, intLen] = g_messageParser->parseVarInt(m_data, needle);
                auto ptr              = rc<const char*>(&m_data.at(needle + intLen));
                result += std::format("{}#{}", id == 0 ? "null" : std::to_string(id), std::string_view{ptr, strLen});
                needle += strLen + intLen;
                break;
            }
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