#include "atom/utils/string.hpp"
#include <gtest/gtest.h>
#include <string_view>

using namespace atom::utils;

TEST(StringUtilsTest, HasUppercase) {
    EXPECT_TRUE(hasUppercase("Hello"));
    EXPECT_FALSE(hasUppercase("hello"));
}

TEST(StringUtilsTest, ToUnderscore) {
    EXPECT_EQ(toUnderscore("HelloWorld"), "hello_world");
    EXPECT_EQ(toUnderscore("helloWorld"), "hello_world");
    EXPECT_EQ(toUnderscore("Hello World"), "hello_world");
}

TEST(StringUtilsTest, ToCamelCase) {
    EXPECT_EQ(toCamelCase("hello_world"), "helloWorld");
    EXPECT_EQ(toCamelCase("Hello_world"), "helloWorld");
    EXPECT_EQ(toCamelCase("hello world"), "helloWorld");
}

TEST(StringUtilsTest, URLEncode) {
    EXPECT_EQ(urlEncode("hello world"), "hello%20world");
    EXPECT_EQ(urlEncode("a+b=c"), "a%2Bb%3Dc");
}

TEST(StringUtilsTest, URLDecode) {
    EXPECT_EQ(urlDecode("hello%20world"), "hello world");
    EXPECT_EQ(urlDecode("a%2Bb%3Dc"), "a+b=c");
}

TEST(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(startsWith("hello world", "hello"));
    EXPECT_FALSE(startsWith("hello world", "world"));
}

TEST(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(endsWith("hello world", "world"));
    EXPECT_FALSE(endsWith("hello world", "hello"));
}

TEST(StringUtilsTest, SplitString) {
    std::vector<std::string> result = splitString("a,b,c", ',');
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
}

TEST(StringUtilsTest, JoinStrings) {
    std::vector<std::string_view> input = {"a", "b", "c"};
    EXPECT_EQ(joinStrings(input, ","), "a,b,c");
}

TEST(StringUtilsTest, ReplaceString) {
    EXPECT_EQ(replaceString("hello world", "world", "universe"),
              "hello universe");
    EXPECT_EQ(replaceString("hello world world", "world", "universe"),
              "hello universe universe");
}

TEST(StringUtilsTest, ReplaceStrings) {
    std::vector<std::pair<std::string_view, std::string_view>> replacements = {
        {"world", "universe"}, {"hello", "hi"}};
    EXPECT_EQ(replaceStrings("hello world", replacements), "hi universe");
}

TEST(StringUtilsTest, SVVtoSV) {
    std::vector<std::string_view> svv = {"a", "b", "c"};
    std::vector<std::string> result = SVVtoSV(svv);
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
}

TEST(StringUtilsTest, Explode) {
    std::vector<std::string> result = explode("a,b,c", ',');
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
}

TEST(StringUtilsTest, Trim) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\nhello\n", "\n"), "hello");
    EXPECT_EQ(trim("\thello\t"), "hello");
}

TEST(StringUtilsTest, StringToWString) {
    EXPECT_EQ(stringToWString("hello"), L"hello");
}

TEST(StringUtilsTest, WStringToString) {
    EXPECT_EQ(wstringToString(L"hello"), "hello");
}

TEST(SplitStringTest, BasicSplitCharDelimiter) {
    std::string str = "apple,banana,grape,orange";
    auto result = split(str, ',').collectVector();
    std::vector<std::string> expected = {"apple", "banana", "grape", "orange"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, BasicSplitStringDelimiter) {
    std::string str = "apple--banana--grape--orange";
    auto result = split(str, std::string_view("--")).collectVector();
    std::vector<std::string> expected = {"apple", "banana", "grape", "orange"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, CustomDelimiterFunction) {
    std::string str = "a1b2c3d4e5f";
    auto isDigit = [](char c) { return std::isdigit(c); };
    auto result = split(str, isDigit).collectVector();
    std::vector<std::string> expected = {"a", "b", "c", "d", "e", "f"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, TrimWhitespace) {
    std::string str = " apple , banana , grape , orange ";
    auto result = split(str, ',', true, false).collectVector();
    std::vector<std::string> expected = {"apple", "banana", "grape", "orange"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, SkipEmptySegments) {
    std::string str = "apple,,banana,,grape,,orange";
    auto result = split(str, ',', false, true).collectVector();
    std::vector<std::string> expected = {"apple", "banana", "grape", "orange"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, CollectToList) {
    std::string str = "apple,banana,grape,orange";
    auto result = split(str, ',').collectList();
    std::list<std::string> expected = {"apple", "banana", "grape", "orange"};
    EXPECT_EQ(result, expected);
}

TEST(SplitStringTest, CollectToArray) {
    std::string str = "apple,banana,grape,orange";
    auto result = split(str, ',').collectArray<4>();
    std::array<std::string, 4> expected = {"apple", "banana", "grape",
                                           "orange"};
    EXPECT_EQ(result, expected);
}
