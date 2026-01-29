/*
 * test_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/utils.hpp"

namespace atom::secret::test {

class UtilsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Time Utilities Tests
// ============================================================================

TEST_F(UtilsTest, FormatTimeIso8601) {
    // Create a known time point
    std::tm tm = {};
    tm.tm_year = 124;  // 2024
    tm.tm_mon = 0;     // January
    tm.tm_mday = 15;
    tm.tm_hour = 10;
    tm.tm_min = 30;
    tm.tm_sec = 0;

#if defined(_WIN32)
    time_t time = _mkgmtime(&tm);
#else
    time_t time = timegm(&tm);
#endif

    auto tp = std::chrono::system_clock::from_time_t(time);
    std::string formatted = Utils::formatTimeIso8601(tp);

    EXPECT_EQ(formatted, "2024-01-15T10:30:00Z");
}

TEST_F(UtilsTest, FormatTimeIso8601Empty) {
    TimePoint empty{};
    std::string formatted = Utils::formatTimeIso8601(empty);
    EXPECT_TRUE(formatted.empty());
}

TEST_F(UtilsTest, ParseTimeIso8601) {
    std::string str = "2024-01-15T10:30:00Z";
    auto tp = Utils::parseTimeIso8601(str);

    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm* tm = std::gmtime(&time);

    EXPECT_EQ(tm->tm_year + 1900, 2024);
    EXPECT_EQ(tm->tm_mon + 1, 1);
    EXPECT_EQ(tm->tm_mday, 15);
    EXPECT_EQ(tm->tm_hour, 10);
    EXPECT_EQ(tm->tm_min, 30);
}

TEST_F(UtilsTest, ParseTimeIso8601Empty) {
    auto tp = Utils::parseTimeIso8601("");
    EXPECT_EQ(tp, TimePoint{});
}

TEST_F(UtilsTest, FormatDuration) {
    EXPECT_EQ(Utils::formatDuration(30), "30 seconds");
    EXPECT_EQ(Utils::formatDuration(120), "2 minutes");
    EXPECT_EQ(Utils::formatDuration(7200), "2 hours");
    EXPECT_EQ(Utils::formatDuration(172800), "2 days");
}

// ============================================================================
// String Utilities Tests
// ============================================================================

TEST_F(UtilsTest, Trim) {
    EXPECT_EQ(Utils::trim("  hello  "), "hello");
    EXPECT_EQ(Utils::trim("hello"), "hello");
    EXPECT_EQ(Utils::trim("  "), "");
    EXPECT_EQ(Utils::trim(""), "");
    EXPECT_EQ(Utils::trim("\t\nhello\t\n"), "hello");
}

TEST_F(UtilsTest, ToLower) {
    EXPECT_EQ(Utils::toLower("HELLO"), "hello");
    EXPECT_EQ(Utils::toLower("Hello World"), "hello world");
    EXPECT_EQ(Utils::toLower("hello"), "hello");
    EXPECT_EQ(Utils::toLower("123ABC"), "123abc");
}

TEST_F(UtilsTest, ToUpper) {
    EXPECT_EQ(Utils::toUpper("hello"), "HELLO");
    EXPECT_EQ(Utils::toUpper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(Utils::toUpper("HELLO"), "HELLO");
    EXPECT_EQ(Utils::toUpper("123abc"), "123ABC");
}

TEST_F(UtilsTest, ContainsIgnoreCase) {
    EXPECT_TRUE(Utils::containsIgnoreCase("Hello World", "world"));
    EXPECT_TRUE(Utils::containsIgnoreCase("Hello World", "HELLO"));
    EXPECT_TRUE(Utils::containsIgnoreCase("Hello World", "lo Wo"));
    EXPECT_FALSE(Utils::containsIgnoreCase("Hello World", "xyz"));
    EXPECT_TRUE(Utils::containsIgnoreCase("Hello", ""));
}

TEST_F(UtilsTest, Mask) {
    EXPECT_EQ(Utils::mask("password"), "********");
    EXPECT_EQ(Utils::mask("password", 2, 2), "pa****rd");
    EXPECT_EQ(Utils::mask("abc", 2, 2), "***");
    EXPECT_EQ(Utils::mask("password", 0, 0, '#'), "########");
}

// ============================================================================
// Validation Utilities Tests
// ============================================================================

TEST_F(UtilsTest, IsValidEmail) {
    EXPECT_TRUE(Utils::isValidEmail("test@example.com"));
    EXPECT_TRUE(Utils::isValidEmail("user.name@domain.org"));
    EXPECT_FALSE(Utils::isValidEmail("invalid"));
    EXPECT_FALSE(Utils::isValidEmail("@example.com"));
    EXPECT_FALSE(Utils::isValidEmail("test@"));
    EXPECT_FALSE(Utils::isValidEmail("test@@example.com"));
    EXPECT_FALSE(Utils::isValidEmail("test@example"));
    EXPECT_FALSE(Utils::isValidEmail(""));
}

TEST_F(UtilsTest, IsValidUrl) {
    EXPECT_TRUE(Utils::isValidUrl("http://example.com"));
    EXPECT_TRUE(Utils::isValidUrl("https://example.com"));
    EXPECT_TRUE(Utils::isValidUrl("ftp://files.example.com"));
    EXPECT_FALSE(Utils::isValidUrl("example.com"));
    EXPECT_FALSE(Utils::isValidUrl(""));
    EXPECT_FALSE(Utils::isValidUrl("file:///path"));
}

// ============================================================================
// Random Utilities Tests
// ============================================================================

TEST_F(UtilsTest, RandomInt) {
    for (int i = 0; i < 100; ++i) {
        int value = Utils::randomInt(1, 10);
        EXPECT_GE(value, 1);
        EXPECT_LE(value, 10);
    }
}

TEST_F(UtilsTest, ShuffleString) {
    std::string original = "abcdefghij";
    std::string shuffled = Utils::shuffleString(original);

    EXPECT_EQ(shuffled.length(), original.length());

    // Check all characters are present
    std::string sortedOriginal = original;
    std::string sortedShuffled = shuffled;
    std::sort(sortedOriginal.begin(), sortedOriginal.end());
    std::sort(sortedShuffled.begin(), sortedShuffled.end());
    EXPECT_EQ(sortedOriginal, sortedShuffled);
}

}  // namespace atom::secret::test
