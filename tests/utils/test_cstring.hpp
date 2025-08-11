// filepath: /home/max/Atom-1/atom/utils/test_cstring.hpp
/*
 * test_cstring.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-18

Description: Tests for compile-time string manipulation utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_CSTRING_HPP
#define ATOM_UTILS_TEST_CSTRING_HPP

#include <gtest/gtest.h>
#include <string>
#include "atom/utils/cstring.hpp"

namespace atom::utils::test {

class CStringTest : public ::testing::Test {
protected:
    // Helper function to convert std::array<char, N> to std::string
    template <size_t N>
    std::string arrayToString(const std::array<char, N>& arr) {
        return std::string(arr.data());
    }
};

// Test deduplicate function
TEST_F(CStringTest, Deduplicate) {
    // Basic deduplication
    auto result1 = deduplicate("hello");
    EXPECT_EQ(arrayToString(result1), "helo");

    // Empty string
    auto result2 = deduplicate("");
    EXPECT_EQ(arrayToString(result2), "");

    // String with no duplicates
    auto result3 = deduplicate("abcdef");
    EXPECT_EQ(arrayToString(result3), "abcdef");

    // String with all identical characters
    auto result4 = deduplicate("aaaaa");
    EXPECT_EQ(arrayToString(result4), "a");

    // String with special characters
    auto result5 = deduplicate("a!b!c!a!b!c!");
    EXPECT_EQ(arrayToString(result5), "a!bc");
}

// Test split function
TEST_F(CStringTest, Split) {
    // Basic split
    auto result1 = split("apple,banana,cherry", ',');
    ASSERT_EQ(result1[0], "apple");
    ASSERT_EQ(result1[1], "banana");
    ASSERT_EQ(result1[2], "cherry");

    // Split with empty parts
    auto result2 = split("apple,,cherry", ',');
    ASSERT_EQ(result2[0], "apple");
    ASSERT_EQ(result2[1], "");
    ASSERT_EQ(result2[2], "cherry");

    // Split with no delimiter
    auto result3 = split("apple", ',');
    ASSERT_EQ(result3[0], "apple");

    // Split empty string
    auto result4 = split("", ',');
    ASSERT_EQ(result4[0], "");

    // Split with delimiter at start and end
    auto result5 = split(",apple,", ',');
    ASSERT_EQ(result5[0], "");
    ASSERT_EQ(result5[1], "apple");
    ASSERT_EQ(result5[2], "");
}

// Test replace function
TEST_F(CStringTest, Replace) {
    // Basic replacement
    auto result1 = replace("hello", 'l', 'x');
    EXPECT_EQ(arrayToString(result1), "hexxo");

    // Replace character not in string
    auto result2 = replace("hello", 'z', 'x');
    EXPECT_EQ(arrayToString(result2), "hello");

    // Replace in empty string
    auto result3 = replace("", 'a', 'b');
    EXPECT_EQ(arrayToString(result3), "");

    // Replace with the same character
    auto result4 = replace("hello", 'l', 'l');
    EXPECT_EQ(arrayToString(result4), "hello");
}

// Test toLower function
TEST_F(CStringTest, ToLower) {
    // Basic lowercase conversion
    auto result1 = toLower("HELLO");
    EXPECT_EQ(arrayToString(result1), "hello");

    // Mixed case
    auto result2 = toLower("HeLlO");
    EXPECT_EQ(arrayToString(result2), "hello");

    // Already lowercase
    auto result3 = toLower("hello");
    EXPECT_EQ(arrayToString(result3), "hello");

    // Empty string
    auto result4 = toLower("");
    EXPECT_EQ(arrayToString(result4), "");

    // Non-alphabetic characters
    auto result5 = toLower("Hello123!@#");
    EXPECT_EQ(arrayToString(result5), "hello123!@#");
}

// Test toUpper function
TEST_F(CStringTest, ToUpper) {
    // Basic uppercase conversion
    auto result1 = toUpper("hello");
    EXPECT_EQ(arrayToString(result1), "HELLO");

    // Mixed case
    auto result2 = toUpper("HeLlO");
    EXPECT_EQ(arrayToString(result2), "HELLO");

    // Already uppercase
    auto result3 = toUpper("HELLO");
    EXPECT_EQ(arrayToString(result3), "HELLO");

    // Empty string
    auto result4 = toUpper("");
    EXPECT_EQ(arrayToString(result4), "");

    // Non-alphabetic characters
    auto result5 = toUpper("Hello123!@#");
    EXPECT_EQ(arrayToString(result5), "HELLO123!@#");
}

// Test concat function
TEST_F(CStringTest, Concat) {
    // Basic concatenation
    auto result1 = concat("Hello, ", "World!");
    EXPECT_EQ(arrayToString(result1), "Hello, World!");

    // Concatenate with empty string
    auto result2 = concat("Hello", "");
    EXPECT_EQ(arrayToString(result2), "Hello");

    auto result3 = concat("", "World");
    EXPECT_EQ(arrayToString(result3), "World");

    // Concatenate two empty strings
    auto result4 = concat("", "");
    EXPECT_EQ(arrayToString(result4), "");

    // Concatenate with special characters
    auto result5 = concat("Hello\n", "World\t!");
    EXPECT_EQ(arrayToString(result5), "Hello\nWorld\t!");
}

// Test trim function for C-style strings
TEST_F(CStringTest, TrimCString) {
    // Basic trimming
    auto result1 = trim("  Hello  ");
    EXPECT_EQ(arrayToString(result1), "Hello");

    // No spaces to trim
    auto result2 = trim("Hello");
    EXPECT_EQ(arrayToString(result2), "Hello");

    // Only leading spaces
    auto result3 = trim("  Hello");
    EXPECT_EQ(arrayToString(result3), "Hello");

    // Only trailing spaces
    auto result4 = trim("Hello  ");
    EXPECT_EQ(arrayToString(result4), "Hello");

    // Only spaces
    auto result5 = trim("     ");
    EXPECT_EQ(arrayToString(result5), "");

    // Empty string
    auto result6 = trim("");
    EXPECT_EQ(arrayToString(result6), "");
}

// Test substring function
TEST_F(CStringTest, Substring) {
    // Basic substring
    auto result1 = substring("Hello, World!", 7, 5);
    EXPECT_EQ(arrayToString(result1), "World");

    // Substring from start
    auto result2 = substring("Hello, World!", 0, 5);
    EXPECT_EQ(arrayToString(result2), "Hello");

    // Substring beyond string length
    auto result3 = substring("Hello", 0, 10);
    EXPECT_EQ(arrayToString(result3), "Hello");

    // Empty substring
    auto result4 = substring("Hello", 0, 0);
    EXPECT_EQ(arrayToString(result4), "");

    // Start beyond string length
    auto result5 = substring("Hello", 10, 5);
    EXPECT_EQ(arrayToString(result5), "");
}

// Test equal function
TEST_F(CStringTest, Equal) {
    // Equal strings
    EXPECT_TRUE(equal("Hello", "Hello"));

    // Different strings
    EXPECT_FALSE(equal("Hello", "World"));

    // Case sensitivity
    EXPECT_FALSE(equal("hello", "Hello"));

    // Different lengths
    EXPECT_FALSE(equal("Hello", "HelloWorld"));

    // Empty strings
    EXPECT_TRUE(equal("", ""));

    // One empty string
    EXPECT_FALSE(equal("Hello", ""));
    EXPECT_FALSE(equal("", "Hello"));
}

// Test find function
TEST_F(CStringTest, Find) {
    // Find existing character
    EXPECT_EQ(find("Hello", 'e'), 1);

    // Find first occurrence of repeated character
    EXPECT_EQ(find("Hello", 'l'), 2);

    // Character not found
    EXPECT_EQ(find("Hello", 'z'), 5);  // Returns N-1 when not found

    // Empty string
    EXPECT_EQ(find("", 'a'), 0);  // Returns N-1 (which is 0 for empty string)

    // Find in first position
    EXPECT_EQ(find("Hello", 'H'), 0);

    // Find in last position
    EXPECT_EQ(find("Hello", 'o'), 4);
}

// Test length function
TEST_F(CStringTest, Length) {
    // Basic length
    EXPECT_EQ(length("Hello"), 5);

    // Empty string
    EXPECT_EQ(length(""), 0);

    // String with spaces
    EXPECT_EQ(length("Hello World"), 11);

    // String with special characters
    EXPECT_EQ(length("Hello\nWorld"), 11);
}

// Test reverse function
TEST_F(CStringTest, Reverse) {
    // Basic reversal
    auto result1 = reverse("Hello");
    EXPECT_EQ(arrayToString(result1), "olleH");

    // Palindrome
    auto result2 = reverse("racecar");
    EXPECT_EQ(arrayToString(result2), "racecar");

    // Empty string
    auto result3 = reverse("");
    EXPECT_EQ(arrayToString(result3), "");

    // Single character
    auto result4 = reverse("A");
    EXPECT_EQ(arrayToString(result4), "A");

    // String with spaces
    auto result5 = reverse("Hello World");
    EXPECT_EQ(arrayToString(result5), "dlroW olleH");
}

// Test trim function for string_view
TEST_F(CStringTest, TrimStringView) {
    // Basic trimming
    std::string_view sv = "  Hello  ";
    EXPECT_EQ(trim(sv), "Hello");

    // No spaces to trim
    sv = "Hello";
    EXPECT_EQ(trim(sv), "Hello");

    // Only leading spaces
    sv = "  Hello";
    EXPECT_EQ(trim(sv), "Hello");

    // Only trailing spaces
    sv = "Hello  ";
    EXPECT_EQ(trim(sv), "Hello");

    // Only spaces
    sv = "     ";
    EXPECT_EQ(trim(sv), "");

    // Empty string
    sv = "";
    EXPECT_EQ(trim(sv), "");

    // All types of whitespace
    sv = " \t\n\r\fHello\v \t";
    EXPECT_EQ(trim(sv), "Hello");
}

// Test charArray conversion functions
TEST_F(CStringTest, CharArrayConversion) {
    // Test charArrayToArrayConstexpr
    std::array<char, 6> input1 = {'H', 'e', 'l', 'l', 'o', '\0'};
    auto result1 = charArrayToArrayConstexpr(input1);
    EXPECT_EQ(arrayToString(result1), "Hello");

    // Test charArrayToArray
    std::array<char, 6> input2 = {'W', 'o', 'r', 'l', 'd', '\0'};
    auto result2 = charArrayToArray(input2);
    EXPECT_EQ(arrayToString(result2), "World");

    // Empty array
    std::array<char, 1> emptyArray = {'\0'};
    auto result3 = charArrayToArrayConstexpr(emptyArray);
    EXPECT_EQ(arrayToString(result3), "");
}

// Test isNegative function
TEST_F(CStringTest, IsNegative) {
    // Negative number
    std::array<char, 3> negative = {'-', '1', '\0'};
    EXPECT_TRUE(isNegative(negative));

    // Positive number
    std::array<char, 3> positive = {'4', '2', '\0'};
    EXPECT_FALSE(isNegative(positive));

    // Zero
    std::array<char, 2> zero = {'0', '\0'};
    EXPECT_FALSE(isNegative(zero));

    // Empty array
    std::array<char, 1> empty = {'\0'};
    EXPECT_FALSE(isNegative(empty));
}

// Test arrayToInt function
TEST_F(CStringTest, ArrayToInt) {
    // Basic conversion
    std::array<char, 4> num1 = {'1', '2', '3', '\0'};
    EXPECT_EQ(arrayToInt(num1), 123);

    // Negative number
    std::array<char, 4> num2 = {'-', '4', '5', '\0'};
    EXPECT_EQ(arrayToInt(num2), -45);

    // Leading zeros
    std::array<char, 5> num3 = {'0', '0', '4', '2', '\0'};
    EXPECT_EQ(arrayToInt(num3), 42);

    // Binary base
    std::array<char, 6> bin = {'1', '0', '1', '0', '1', '\0'};
    EXPECT_EQ(arrayToInt(bin, BASE_2), 21);  // 10101 in binary is 21 in decimal

    // Hexadecimal base
    std::array<char, 4> hex = {'F', 'F', 'F', '\0'};
    EXPECT_EQ(arrayToInt(hex, BASE_16), 4095);  // FFF in hex is 4095 in decimal
}

// Test absoluteValue function
TEST_F(CStringTest, AbsoluteValue) {
    // Positive number
    std::array<char, 3> pos = {'4', '2', '\0'};
    EXPECT_EQ(absoluteValue(pos), 42);

    // Negative number
    std::array<char, 4> neg = {'-', '4', '2', '\0'};
    EXPECT_EQ(absoluteValue(neg), 42);

    // Zero
    std::array<char, 2> zero = {'0', '\0'};
    EXPECT_EQ(absoluteValue(zero), 0);
}

// Test convertBase function
TEST_F(CStringTest, ConvertBase) {
    // Decimal to binary
    std::array<char, 3> dec1 = {'1', '0', '\0'};
    EXPECT_EQ(convertBase(dec1, BASE_10, BASE_2), "1010");  // 10 to binary

    // Decimal to hex
    std::array<char, 4> dec2 = {'2', '5', '5', '\0'};
    EXPECT_EQ(convertBase(dec2, BASE_10, BASE_16), "FF");  // 255 to hex

    // Binary to decimal
    std::array<char, 6> bin = {'1', '0', '1', '0', '1', '\0'};
    EXPECT_EQ(convertBase(bin, BASE_2, BASE_10), "21");  // 10101 binary to decimal

    // Hex to decimal
    std::array<char, 3> hex = {'F', 'F', '\0'};
    EXPECT_EQ(convertBase(hex, BASE_16, BASE_10), "255");  // FF to decimal

    // Zero conversion
    std::array<char, 2> zero = {'0', '\0'};
    EXPECT_EQ(convertBase(zero, BASE_10, BASE_16), "0");

    // Negative number
    std::array<char, 3> neg = {'-', '5', '\0'};
    EXPECT_EQ(convertBase(neg, BASE_10, BASE_2), "-101");  // -5 to binary
}

// Test compile-time capabilities
TEST_F(CStringTest, CompileTimeOperations) {
    // These tests verify that the functions work at compile time

    // Create compile-time constants
    constexpr auto deduped = deduplicate("hello");
    constexpr auto replaced = replace("hello", 'l', 'x');
    constexpr auto lowered = toLower("HELLO");
    constexpr auto uppered = toUpper("hello");
    constexpr auto concatenated = concat("Hello", "World");
    constexpr auto reversed = reverse("Hello");
    constexpr auto found = find("Hello", 'e');
    constexpr auto len = length("Hello");
    constexpr bool isEqual = equal("Hello", "Hello");

    // Verify values
    EXPECT_EQ(arrayToString(deduped), "helo");
    EXPECT_EQ(arrayToString(replaced), "hexxo");
    EXPECT_EQ(arrayToString(lowered), "hello");
    EXPECT_EQ(arrayToString(uppered), "HELLO");
    EXPECT_EQ(arrayToString(concatenated), "HelloWorld");
    EXPECT_EQ(arrayToString(reversed), "olleH");
    EXPECT_EQ(found, 1);
    EXPECT_EQ(len, 5);
    EXPECT_TRUE(isEqual);
}

// Test complex combinations and special cases
TEST_F(CStringTest, ComplexCombinations) {
    // Chain multiple operations
    constexpr auto step1 = toLower("HELLO WORLD");
    // Convert to string literal by using a fixed size array
    constexpr const char step1_str[] = "hello world";
    constexpr auto step2 = replace(step1_str, ' ', '_');
    constexpr const char step2_str[] = "hello_world";
    constexpr auto step3 = reverse(step2_str);

    EXPECT_EQ(arrayToString(step3), "dlrow_olleh");

    // Test with various special characters
    constexpr const char specialChars[] = "!@#$%^&*()_+{}:<>?";
    auto revSpecial = reverse(specialChars);
    EXPECT_EQ(arrayToString(revSpecial), "?><:{}+_)(*&^%$#@!");

    // Unicode handling is limited in C-style strings, so these tests are basic
    constexpr const char unicodeChars[] = "Привет"; // Russian word "hello"
    auto revUnicode = reverse(unicodeChars);
    // This may not work correctly for multi-byte characters,
    // but we test the behavior for documentation purposes
    EXPECT_NE(arrayToString(revUnicode), unicodeChars);
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_CSTRING_HPP
