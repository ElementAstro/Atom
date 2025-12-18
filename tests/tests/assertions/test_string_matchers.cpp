/*
 * test_string_matchers.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for string matchers in
atom/tests/assertions/string_matchers.hpp

**************************************************/

#include <gtest/gtest.h>

#include <string>

#include "atom/tests/assertions/string_matchers.hpp"

namespace atom::test::assertions::tests {

// ============================================================================
// IsBlank Matcher Tests
// ============================================================================

class IsBlankMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsBlankMatcherTest, EmptyString) {
    auto matcher = IsBlank();
    EXPECT_TRUE(matcher.matches(""));
}

TEST_F(IsBlankMatcherTest, WhitespaceOnly) {
    auto matcher = IsBlank();
    EXPECT_TRUE(matcher.matches("   "));
    EXPECT_TRUE(matcher.matches("\t\t"));
    EXPECT_TRUE(matcher.matches("\n\n"));
    EXPECT_TRUE(matcher.matches(" \t\n "));
}

TEST_F(IsBlankMatcherTest, NonBlankString) {
    auto matcher = IsBlank();
    EXPECT_FALSE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches(" hello "));
    EXPECT_FALSE(matcher.matches("a"));
}

// ============================================================================
// HasLength Matcher Tests
// ============================================================================

class HasLengthMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HasLengthMatcherTest, ExactLength) {
    auto matcher = HasLength(5);
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("hi"));
    EXPECT_FALSE(matcher.matches("hello world"));
}

TEST_F(HasLengthMatcherTest, ZeroLength) {
    auto matcher = HasLength(0);
    EXPECT_TRUE(matcher.matches(""));
    EXPECT_FALSE(matcher.matches("a"));
}

// ============================================================================
// ContainsIgnoreCase Matcher Tests
// ============================================================================

class ContainsIgnoreCaseMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ContainsIgnoreCaseMatcherTest, ExactMatch) {
    auto matcher = ContainsIgnoreCase("world");
    EXPECT_TRUE(matcher.matches("hello world"));
}

TEST_F(ContainsIgnoreCaseMatcherTest, CaseInsensitive) {
    auto matcher = ContainsIgnoreCase("WORLD");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("hello WORLD"));
    EXPECT_TRUE(matcher.matches("hello World"));
}

TEST_F(ContainsIgnoreCaseMatcherTest, NotContained) {
    auto matcher = ContainsIgnoreCase("foo");
    EXPECT_FALSE(matcher.matches("hello world"));
}

// ============================================================================
// StartsWithIgnoreCase Matcher Tests
// ============================================================================

class StartsWithIgnoreCaseMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StartsWithIgnoreCaseMatcherTest, ExactMatch) {
    auto matcher = StartsWithIgnoreCase("hello");
    EXPECT_TRUE(matcher.matches("hello world"));
}

TEST_F(StartsWithIgnoreCaseMatcherTest, CaseInsensitive) {
    auto matcher = StartsWithIgnoreCase("HELLO");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("HELLO world"));
    EXPECT_TRUE(matcher.matches("Hello World"));
}

TEST_F(StartsWithIgnoreCaseMatcherTest, DoesNotStartWith) {
    auto matcher = StartsWithIgnoreCase("world");
    EXPECT_FALSE(matcher.matches("hello world"));
}

// ============================================================================
// EndsWithIgnoreCase Matcher Tests
// ============================================================================

class EndsWithIgnoreCaseMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(EndsWithIgnoreCaseMatcherTest, ExactMatch) {
    auto matcher = EndsWithIgnoreCase("world");
    EXPECT_TRUE(matcher.matches("hello world"));
}

TEST_F(EndsWithIgnoreCaseMatcherTest, CaseInsensitive) {
    auto matcher = EndsWithIgnoreCase("WORLD");
    EXPECT_TRUE(matcher.matches("hello world"));
    EXPECT_TRUE(matcher.matches("hello WORLD"));
    EXPECT_TRUE(matcher.matches("Hello World"));
}

TEST_F(EndsWithIgnoreCaseMatcherTest, DoesNotEndWith) {
    auto matcher = EndsWithIgnoreCase("hello");
    EXPECT_FALSE(matcher.matches("hello world"));
}

// ============================================================================
// IsNumeric Matcher Tests
// ============================================================================

class IsNumericMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsNumericMatcherTest, IntegerString) {
    auto matcher = IsNumeric();
    EXPECT_TRUE(matcher.matches("12345"));
    EXPECT_TRUE(matcher.matches("0"));
    EXPECT_TRUE(matcher.matches("-123"));
    EXPECT_TRUE(matcher.matches("+456"));
}

TEST_F(IsNumericMatcherTest, FloatString) {
    auto matcher = IsNumeric();
    EXPECT_TRUE(matcher.matches("3.14"));
    EXPECT_TRUE(matcher.matches("-2.71"));
    EXPECT_TRUE(matcher.matches(".5"));
    EXPECT_TRUE(matcher.matches("1."));
}

TEST_F(IsNumericMatcherTest, ScientificNotation) {
    auto matcher = IsNumeric();
    EXPECT_TRUE(matcher.matches("1e10"));
    EXPECT_TRUE(matcher.matches("1E-5"));
    EXPECT_TRUE(matcher.matches("3.14e2"));
}

TEST_F(IsNumericMatcherTest, NonNumericString) {
    auto matcher = IsNumeric();
    EXPECT_FALSE(matcher.matches("abc"));
    EXPECT_FALSE(matcher.matches("12ab"));
    EXPECT_FALSE(matcher.matches(""));
}

// ============================================================================
// IsEmail Matcher Tests
// ============================================================================

class IsEmailMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsEmailMatcherTest, ValidEmail) {
    auto matcher = IsEmail();
    EXPECT_TRUE(matcher.matches("test@example.com"));
    EXPECT_TRUE(matcher.matches("user.name@domain.org"));
    EXPECT_TRUE(matcher.matches("user+tag@example.co.uk"));
}

TEST_F(IsEmailMatcherTest, InvalidEmail) {
    auto matcher = IsEmail();
    EXPECT_FALSE(matcher.matches("invalid"));
    EXPECT_FALSE(matcher.matches("@example.com"));
    EXPECT_FALSE(matcher.matches("test@"));
    EXPECT_FALSE(matcher.matches("test@.com"));
}

// ============================================================================
// IsUrl Matcher Tests
// ============================================================================

class IsUrlMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsUrlMatcherTest, ValidHttpUrl) {
    auto matcher = IsUrl();
    EXPECT_TRUE(matcher.matches("http://example.com"));
    EXPECT_TRUE(matcher.matches("https://example.com"));
    EXPECT_TRUE(matcher.matches("http://www.example.com/path"));
}

TEST_F(IsUrlMatcherTest, ValidUrlWithPort) {
    auto matcher = IsUrl();
    EXPECT_TRUE(matcher.matches("http://localhost:8080"));
    EXPECT_TRUE(matcher.matches("https://example.com:443/path"));
}

TEST_F(IsUrlMatcherTest, ValidUrlWithQuery) {
    auto matcher = IsUrl();
    EXPECT_TRUE(matcher.matches("http://example.com?q=test"));
    EXPECT_TRUE(matcher.matches("https://example.com/path?a=1&b=2"));
}

TEST_F(IsUrlMatcherTest, InvalidUrl) {
    auto matcher = IsUrl();
    EXPECT_FALSE(matcher.matches("invalid"));
    EXPECT_FALSE(matcher.matches("ftp://example.com"));  // If only http/https
    EXPECT_FALSE(matcher.matches("://example.com"));
}

// ============================================================================
// IsUuid Matcher Tests
// ============================================================================

class IsUuidMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsUuidMatcherTest, ValidUuid) {
    auto matcher = IsUuid();
    EXPECT_TRUE(matcher.matches("550e8400-e29b-41d4-a716-446655440000"));
    EXPECT_TRUE(matcher.matches("123e4567-e89b-12d3-a456-426614174000"));
}

TEST_F(IsUuidMatcherTest, ValidUuidUppercase) {
    auto matcher = IsUuid();
    EXPECT_TRUE(matcher.matches("550E8400-E29B-41D4-A716-446655440000"));
}

TEST_F(IsUuidMatcherTest, InvalidUuid) {
    auto matcher = IsUuid();
    EXPECT_FALSE(matcher.matches("invalid-uuid"));
    EXPECT_FALSE(matcher.matches("550e8400-e29b-41d4-a716"));
    EXPECT_FALSE(matcher.matches("550e8400e29b41d4a716446655440000"));
}

// ============================================================================
// IsJson Matcher Tests
// ============================================================================

class IsJsonMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsJsonMatcherTest, ValidJsonObject) {
    auto matcher = IsJson();
    EXPECT_TRUE(matcher.matches("{}"));
    EXPECT_TRUE(matcher.matches("{\"key\": \"value\"}"));
    EXPECT_TRUE(matcher.matches("{\"a\": 1, \"b\": 2}"));
}

TEST_F(IsJsonMatcherTest, ValidJsonArray) {
    auto matcher = IsJson();
    EXPECT_TRUE(matcher.matches("[]"));
    EXPECT_TRUE(matcher.matches("[1, 2, 3]"));
    EXPECT_TRUE(matcher.matches("[\"a\", \"b\"]"));
}

TEST_F(IsJsonMatcherTest, ValidJsonPrimitive) {
    auto matcher = IsJson();
    EXPECT_TRUE(matcher.matches("null"));
    EXPECT_TRUE(matcher.matches("true"));
    EXPECT_TRUE(matcher.matches("false"));
    EXPECT_TRUE(matcher.matches("123"));
    EXPECT_TRUE(matcher.matches("\"string\""));
}

TEST_F(IsJsonMatcherTest, InvalidJson) {
    auto matcher = IsJson();
    EXPECT_FALSE(matcher.matches("{invalid}"));
    EXPECT_FALSE(matcher.matches("{key: value}"));
    EXPECT_FALSE(matcher.matches("[1, 2,]"));
}

// ============================================================================
// IsPalindrome Matcher Tests
// ============================================================================

class IsPalindromeMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsPalindromeMatcherTest, SimplePalindrome) {
    auto matcher = IsPalindrome();
    EXPECT_TRUE(matcher.matches("racecar"));
    EXPECT_TRUE(matcher.matches("level"));
    EXPECT_TRUE(matcher.matches("radar"));
}

TEST_F(IsPalindromeMatcherTest, SingleCharacter) {
    auto matcher = IsPalindrome();
    EXPECT_TRUE(matcher.matches("a"));
    EXPECT_TRUE(matcher.matches(""));
}

TEST_F(IsPalindromeMatcherTest, NotPalindrome) {
    auto matcher = IsPalindrome();
    EXPECT_FALSE(matcher.matches("hello"));
    EXPECT_FALSE(matcher.matches("world"));
}

TEST_F(IsPalindromeMatcherTest, PalindromeIgnoreCase) {
    auto matcher = IsPalindromeIgnoreCase();
    EXPECT_TRUE(matcher.matches("RaceCar"));
    EXPECT_TRUE(matcher.matches("Level"));
}

// ============================================================================
// String Length Range Matcher Tests
// ============================================================================

class StringLengthRangeMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringLengthRangeMatcherTest, WithinRange) {
    auto matcher = HasLengthBetween(3, 10);
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("abc"));
    EXPECT_TRUE(matcher.matches("0123456789"));
}

TEST_F(StringLengthRangeMatcherTest, OutsideRange) {
    auto matcher = HasLengthBetween(3, 10);
    EXPECT_FALSE(matcher.matches("ab"));
    EXPECT_FALSE(matcher.matches("01234567890"));
}

// ============================================================================
// String Trimmed Matcher Tests
// ============================================================================

class StringTrimmedMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StringTrimmedMatcherTest, AlreadyTrimmed) {
    auto matcher = IsTrimmed();
    EXPECT_TRUE(matcher.matches("hello"));
    EXPECT_TRUE(matcher.matches("hello world"));
}

TEST_F(StringTrimmedMatcherTest, NotTrimmed) {
    auto matcher = IsTrimmed();
    EXPECT_FALSE(matcher.matches(" hello"));
    EXPECT_FALSE(matcher.matches("hello "));
    EXPECT_FALSE(matcher.matches(" hello "));
}

}  // namespace atom::test::assertions::tests
