#include <gtest/gtest.h>

#include "atom/type/static_string.hpp"

class StaticStringTest : public ::testing::Test {};

// Test default constructor
TEST_F(StaticStringTest, DefaultConstructor) {
    StaticString<10> str;
    EXPECT_EQ(str.size(), 0);
    EXPECT_TRUE(str.empty());
    EXPECT_STREQ(str.c_str(), "");
}

// Test constructor with C-style string
TEST_F(StaticStringTest, CStyleStringConstructor) {
    StaticString<10> str("hello");
    EXPECT_EQ(str.size(), 5);
    EXPECT_FALSE(str.empty());
    EXPECT_STREQ(str.c_str(), "hello");
}

// Test constructor with std::string_view
TEST_F(StaticStringTest, StringViewConstructor) {
    StaticString<10> str(std::string_view("world"));
    EXPECT_EQ(str.size(), 5);
    EXPECT_FALSE(str.empty());
    EXPECT_STREQ(str.c_str(), "world");
}

// Test size, empty, and c_str methods
TEST_F(StaticStringTest, SizeEmptyCStr) {
    StaticString<10> str("test");
    EXPECT_EQ(str.size(), 4);
    EXPECT_FALSE(str.empty());
    EXPECT_STREQ(str.c_str(), "test");
}

// Test iterators
TEST_F(StaticStringTest, Iterators) {
    StaticString<10> str("abc");
    EXPECT_EQ(*str.begin(), 'a');
    EXPECT_EQ(*(str.end() - 1), 'c');
}

// Test element access
TEST_F(StaticStringTest, ElementAccess) {
    StaticString<10> str("abc");
    EXPECT_EQ(str[0], 'a');
    EXPECT_EQ(str[1], 'b');
    EXPECT_EQ(str[2], 'c');
}

// Test push_back
TEST_F(StaticStringTest, PushBack) {
    StaticString<10> str("abc");
    str.push_back('d');
    EXPECT_EQ(str.size(), 4);
    EXPECT_STREQ(str.c_str(), "abcd");
}

// Test append
TEST_F(StaticStringTest, Append) {
    StaticString<10> str("abc");
    str.append("def");
    EXPECT_EQ(str.size(), 6);
    EXPECT_STREQ(str.c_str(), "abcdef");
}

// Test replace
TEST_F(StaticStringTest, Replace) {
    StaticString<10> str("abcdef");
    str.replace(2, 3, "xyz");
    EXPECT_EQ(str.size(), 6);
    EXPECT_STREQ(str.c_str(), "abxyzf");
}

// Test substr
TEST_F(StaticStringTest, Substr) {
    StaticString<10> str("abcdef");
    auto substr = str.substr(2, 3);
    EXPECT_EQ(substr.size(), 3);
    EXPECT_STREQ(substr.c_str(), "cde");
}

// Test find
TEST_F(StaticStringTest, Find) {
    StaticString<10> str("abcdef");
    EXPECT_EQ(str.find('c'), 2);
    EXPECT_EQ(str.find('z'), StaticString<10>::npos);
}

// Test comparison operators
TEST_F(StaticStringTest, ComparisonOperators) {
    StaticString<10> str1("abc");
    StaticString<10> str2("abc");
    StaticString<10> str3("def");
    EXPECT_TRUE(str1 == str2);
    EXPECT_FALSE(str1 == str3);
    EXPECT_TRUE(str1 != str3);
    EXPECT_FALSE(str1 != str2);
}

// Test concatenation operator
TEST_F(StaticStringTest, ConcatenationOperator) {
    StaticString<5> str1("abc");
    StaticString<5> str2("def");
    auto result = str1 + str2;
    EXPECT_EQ(result.size(), 6);
    EXPECT_STREQ(result.c_str(), "abcdef");
}