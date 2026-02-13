/*
 * test_hex_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Unit Tests for Hex Utility Functions

**************************************************/

#include <gtest/gtest.h>
#include "atom/algorithm/core/hex_utils.hpp"

using namespace atom::algorithm;

class HexUtilsTest : public ::testing::Test {};

// =============================================================================
// hexToNibble Tests
// =============================================================================

TEST_F(HexUtilsTest, HexToNibbleDigits) {
    for (char c = '0'; c <= '9'; ++c) {
        auto result = hexToNibble(c);
        ASSERT_TRUE(result.has_value()) << "Failed for char: " << c;
        EXPECT_EQ(result.value(), c - '0');
    }
}

TEST_F(HexUtilsTest, HexToNibbleUppercase) {
    const char* hex = "ABCDEF";
    for (int i = 0; i < 6; ++i) {
        auto result = hexToNibble(hex[i]);
        ASSERT_TRUE(result.has_value()) << "Failed for char: " << hex[i];
        EXPECT_EQ(result.value(), 10 + i);
    }
}

TEST_F(HexUtilsTest, HexToNibbleLowercase) {
    const char* hex = "abcdef";
    for (int i = 0; i < 6; ++i) {
        auto result = hexToNibble(hex[i]);
        ASSERT_TRUE(result.has_value()) << "Failed for char: " << hex[i];
        EXPECT_EQ(result.value(), 10 + i);
    }
}

TEST_F(HexUtilsTest, HexToNibbleInvalid) {
    EXPECT_FALSE(hexToNibble('G').has_value());
    EXPECT_FALSE(hexToNibble('g').has_value());
    EXPECT_FALSE(hexToNibble(' ').has_value());
    EXPECT_FALSE(hexToNibble('\0').has_value());
    EXPECT_FALSE(hexToNibble('Z').has_value());
    EXPECT_FALSE(hexToNibble('-').has_value());
}

// =============================================================================
// nibbleToHex Tests
// =============================================================================

TEST_F(HexUtilsTest, NibbleToHexUppercase) {
    EXPECT_EQ(nibbleToHex(0, true), '0');
    EXPECT_EQ(nibbleToHex(9, true), '9');
    EXPECT_EQ(nibbleToHex(10, true), 'A');
    EXPECT_EQ(nibbleToHex(15, true), 'F');
}

TEST_F(HexUtilsTest, NibbleToHexLowercase) {
    EXPECT_EQ(nibbleToHex(0, false), '0');
    EXPECT_EQ(nibbleToHex(9, false), '9');
    EXPECT_EQ(nibbleToHex(10, false), 'a');
    EXPECT_EQ(nibbleToHex(15, false), 'f');
}

// =============================================================================
// isHexDigit Tests
// =============================================================================

TEST_F(HexUtilsTest, IsHexDigitValid) {
    for (char c = '0'; c <= '9'; ++c) {
        EXPECT_TRUE(isHexDigit(c)) << "Failed for: " << c;
    }
    for (char c = 'A'; c <= 'F'; ++c) {
        EXPECT_TRUE(isHexDigit(c)) << "Failed for: " << c;
    }
    for (char c = 'a'; c <= 'f'; ++c) {
        EXPECT_TRUE(isHexDigit(c)) << "Failed for: " << c;
    }
}

TEST_F(HexUtilsTest, IsHexDigitInvalid) {
    EXPECT_FALSE(isHexDigit('G'));
    EXPECT_FALSE(isHexDigit('g'));
    EXPECT_FALSE(isHexDigit(' '));
    EXPECT_FALSE(isHexDigit('\n'));
    EXPECT_FALSE(isHexDigit('Z'));
}

// =============================================================================
// Round-trip Tests
// =============================================================================

TEST_F(HexUtilsTest, RoundTrip) {
    for (u8 i = 0; i < 16; ++i) {
        char hex_upper = nibbleToHex(i, true);
        char hex_lower = nibbleToHex(i, false);

        auto result_upper = hexToNibble(hex_upper);
        auto result_lower = hexToNibble(hex_lower);

        ASSERT_TRUE(result_upper.has_value());
        ASSERT_TRUE(result_lower.has_value());
        EXPECT_EQ(result_upper.value(), i);
        EXPECT_EQ(result_lower.value(), i);
    }
}
