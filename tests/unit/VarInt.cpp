#include <gtest/gtest.h>

#include "core/message/MessageParser.hpp"

#include <array>

TEST(VarInt, EncodeThenParseRoundtripAtBoundaries) {
    Hyprwire::CMessageParser    parser;

    const std::array<size_t, 9> values = {
        0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455,
    };

    for (const auto value : values) {
        const auto encoded      = parser.encodeVarInt(value);
        const auto [decoded, n] = parser.parseVarInt(std::span<const uint8_t>{encoded.data(), encoded.size()});

        ASSERT_FALSE(encoded.empty());
        EXPECT_EQ(decoded, value);
        EXPECT_EQ(n, encoded.size());

        EXPECT_EQ(encoded.back() & 0x80, 0);
    }
}

TEST(VarInt, ParseFromVectorWithOffset) {
    Hyprwire::CMessageParser parser;

    const auto               encoded = parser.encodeVarInt(420);

    std::vector<uint8_t>     data = {0xAA, 0xBB};
    data.insert(data.end(), encoded.begin(), encoded.end());
    data.push_back(0xCC);

    const auto [decoded, n] = parser.parseVarInt(data, 2);
    EXPECT_EQ(decoded, 420);
    EXPECT_EQ(n, encoded.size());
}

TEST(VarInt, ParseOutOfBoundsOffsetReturnsZeroPair) {
    Hyprwire::CMessageParser   parser;

    const std::vector<uint8_t> data     = {1, 2, 3};
    const auto                 expected = std::pair<size_t, size_t>{0, 0};
    EXPECT_EQ(parser.parseVarInt(data, data.size()), expected);
    EXPECT_EQ(parser.parseVarInt(data, data.size() + 42), expected);
}
