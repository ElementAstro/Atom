#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "string.hpp"

using namespace atom::utils;
using namespace std::string_literals;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

class StringUtilsTest : public ::testing::Test {
protected:
    // Common test strings and fixtures
    const std::string empty_string = "";
    const std::string camel_case = "testStringCamelCase";
    const std::string snake_case = "test_string_snake_case";
    const std::string special_chars =
        "a!b@c#d$e%f^g&h*i(j)k_l+m=n[o]p{q}r|s;t:u,v.w<x>y?z/";
    const std::string url_safe =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.~";
    const std::vector<std::string_view> string_array = {"one", "two", "three",
                                                        "four", "five"};
};

// Test hasUppercase function
TEST_F(StringUtilsTest, HasUppercase) {
    EXPECT_FALSE(hasUppercase(""));
    EXPECT_FALSE(hasUppercase("all lowercase string"));
    EXPECT_TRUE(hasUppercase("Mixed Case String"));
    EXPECT_TRUE(hasUppercase("ALL UPPERCASE STRING"));
    EXPECT_TRUE(hasUppercase("mostly lowercase but One uppercase"));
    EXPECT_TRUE(hasUppercase("A"));
}

// Test toUnderscore function
TEST_F(StringUtilsTest, ToUnderscore) {
    EXPECT_EQ(toUnderscore(""), "");
    EXPECT_EQ(toUnderscore("alllowercase"), "alllowercase");
    EXPECT_EQ(toUnderscore("camelCase"), "camel_case");
    EXPECT_EQ(toUnderscore("CamelCase"), "camel_case");
    EXPECT_EQ(toUnderscore("PascalCase"), "pascal_case");
    EXPECT_EQ(toUnderscore("ABCacronym"), "a_b_cacronym");
    EXPECT_EQ(toUnderscore("already_snake_case"), "already_snake_case");
    EXPECT_EQ(toUnderscore("Mixed_Case_With_Underscores"),
              "mixed_case_with_underscores");
    EXPECT_EQ(toUnderscore("XMLHttpRequest"), "x_m_l_http_request");

    // Test with large input to check reserve behavior
    std::string long_input;
    for (int i = 0; i < 1000; i++) {
        long_input += (i % 2 == 0) ? "Aa" : "bb";
    }
    EXPECT_EQ(toUnderscore(long_input).size(),
              long_input.size() + 1000);  // One underscore per uppercase
}

// Test toCamelCase function
TEST_F(StringUtilsTest, ToCamelCase) {
    EXPECT_EQ(toCamelCase(""), "");
    EXPECT_EQ(toCamelCase("alllowercase"), "alllowercase");
    EXPECT_EQ(toCamelCase("snake_case"), "snakeCase");
    EXPECT_EQ(toCamelCase("multiple_word_snake_case"), "multipleWordSnakeCase");
    EXPECT_EQ(toCamelCase("already_camel_case"), "alreadyCamelCase");
    EXPECT_EQ(toCamelCase("_leading_underscore"), "LeadingUnderscore");
    EXPECT_EQ(toCamelCase("trailing_underscore_"), "trailingUnderscore");
    EXPECT_EQ(toCamelCase("__multiple___underscores____"),
              "MultipleUnderscores");
}

// Test urlEncode function
TEST_F(StringUtilsTest, UrlEncode) {
    EXPECT_EQ(urlEncode(""), "");
    EXPECT_EQ(urlEncode("abcABC123"), "abcABC123");
    EXPECT_EQ(urlEncode("hello world"), "hello+world");
    EXPECT_EQ(urlEncode("hello!world"), "hello%21world");
    EXPECT_EQ(urlEncode("特殊字符"), "%E7%89%B9%E6%AE%8A%E5%AD%97%E7%AC%A6");
    EXPECT_EQ(urlEncode("?key=value&other=param"),
              "%3Fkey%3Dvalue%26other%3Dparam");
    EXPECT_EQ(urlEncode(url_safe),
              url_safe);  // URL safe chars should remain as is

    // Test exception safety
    EXPECT_NO_THROW(urlEncode(std::string(10000, 'a')));  // Large string
}

// Test urlDecode function
TEST_F(StringUtilsTest, UrlDecode) {
    EXPECT_EQ(urlDecode(""), "");
    EXPECT_EQ(urlDecode("abcABC123"), "abcABC123");
    EXPECT_EQ(urlDecode("hello+world"), "hello world");
    EXPECT_EQ(urlDecode("hello%21world"), "hello!world");
    EXPECT_EQ(urlDecode("%E7%89%B9%E6%AE%8A%E5%AD%97%E7%AC%A6"), "特殊字符");
    EXPECT_EQ(urlDecode("%3Fkey%3Dvalue%26other%3Dparam"),
              "?key=value&other=param");

    // Error cases
    EXPECT_THROW(urlDecode("incomplete%2"), std::invalid_argument);
    EXPECT_THROW(urlDecode("%XX"), std::invalid_argument);
    EXPECT_THROW(urlDecode("%"), std::invalid_argument);
}

// Test startsWith function
TEST_F(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(startsWith("", ""));
    EXPECT_TRUE(startsWith("hello", ""));
    EXPECT_TRUE(startsWith("hello", "h"));
    EXPECT_TRUE(startsWith("hello", "he"));
    EXPECT_TRUE(startsWith("hello", "hello"));
    EXPECT_FALSE(startsWith("hello", "hello world"));
    EXPECT_FALSE(startsWith("hello", "a"));
    EXPECT_FALSE(startsWith("", "a"));

    // Case sensitivity
    EXPECT_FALSE(startsWith("Hello", "h"));
    EXPECT_TRUE(startsWith("Hello", "H"));
}

// Test endsWith function
TEST_F(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(endsWith("", ""));
    EXPECT_TRUE(endsWith("hello", ""));
    EXPECT_TRUE(endsWith("hello", "o"));
    EXPECT_TRUE(endsWith("hello", "lo"));
    EXPECT_TRUE(endsWith("hello", "hello"));
    EXPECT_FALSE(endsWith("hello", "hello world"));
    EXPECT_FALSE(endsWith("hello", "a"));
    EXPECT_FALSE(endsWith("", "a"));

    // Case sensitivity
    EXPECT_FALSE(endsWith("Hello", "O"));
    EXPECT_TRUE(endsWith("Hello", "o"));
}

// Test splitString function
TEST_F(StringUtilsTest, SplitString) {
    EXPECT_EQ(splitString("", ',').size(), 0);
    EXPECT_THAT(splitString("hello", ','), ElementsAre("hello"));
    EXPECT_THAT(splitString("hello,world", ','), ElementsAre("hello", "world"));
    EXPECT_THAT(splitString("hello,world,test", ','),
                ElementsAre("hello", "world", "test"));
    EXPECT_THAT(splitString(",hello,,world,", ','),
                ElementsAre("", "hello", "", "world", ""));

    // Test with different delimiters
    EXPECT_THAT(splitString("hello world test", ' '),
                ElementsAre("hello", "world", "test"));
    EXPECT_THAT(splitString("one|two|three", '|'),
                ElementsAre("one", "two", "three"));
}

// Test joinStrings function
TEST_F(StringUtilsTest, JoinStrings) {
    std::vector<std::string_view> empty_array;
    EXPECT_EQ(joinStrings(empty_array, ","), "");
    EXPECT_EQ(joinStrings(string_array, ""), "onetwothreefourfive");
    EXPECT_EQ(joinStrings(string_array, ","), "one,two,three,four,five");
    EXPECT_EQ(joinStrings(string_array, ", "), "one, two, three, four, five");
    EXPECT_EQ(joinStrings(string_array, " | "),
              "one | two | three | four | five");

    // Test with single element
    std::vector<std::string_view> single_element = {"alone"};
    EXPECT_EQ(joinStrings(single_element, ","), "alone");
}

// Test replaceString function
TEST_F(StringUtilsTest, ReplaceString) {
    EXPECT_EQ(replaceString("", "old", "new"), "");
    EXPECT_EQ(replaceString("hello", "", "new"), "hello");
    EXPECT_EQ(replaceString("hello", "h", "j"), "jello");
    EXPECT_EQ(replaceString("hello", "l", "L"), "heLLo");
    EXPECT_EQ(replaceString("hello", "hello", "hi"), "hi");
    EXPECT_EQ(replaceString("hello hello", "hello", "hi"), "hi hi");

    // Replace with empty string
    EXPECT_EQ(replaceString("hello", "l", ""), "heo");
    EXPECT_EQ(replaceString("hello", "hello", ""), "");

    // Replace with longer string
    EXPECT_EQ(replaceString("hello", "l", "lll"), "helllllo");

    // No matches
    EXPECT_EQ(replaceString("hello", "z", "x"), "hello");

    // Test performance with large string
    std::string large_string(10000, 'a');
    large_string += "needle";
    large_string += std::string(10000, 'a');
    EXPECT_EQ(
        replaceString(large_string, "needle", "replacement").size(),
        large_string.size() - 6 + 11);  // "needle" (6) -> "replacement" (11)
}

// Test replaceStrings function
TEST_F(StringUtilsTest, ReplaceStrings) {
    std::vector<std::pair<std::string_view, std::string_view>> replacements = {
        {"a", "A"}, {"e", "E"}, {"i", "I"}, {"o", "O"}, {"u", "U"}};
    EXPECT_EQ(replaceStrings("", replacements), "");
    EXPECT_EQ(replaceStrings("hello", {}), "hello");
    EXPECT_EQ(replaceStrings("hello", replacements), "hEllO");
    EXPECT_EQ(replaceStrings("aeiou", replacements), "AEIOU");

    // Test with empty old string (should be skipped)
    std::vector<std::pair<std::string_view, std::string_view>> with_empty = {
        {"", "EMPTY"}, {"a", "A"}};
    EXPECT_EQ(replaceStrings("banana", with_empty), "bAnAnA");

    // Test order of replacements
    std::vector<std::pair<std::string_view, std::string_view>> order_test = {
        {"banana", "orange"}, {"b", "c"}, {"na", "NO"}};
    // Should replace "banana" first, then the other replacements don't apply
    EXPECT_EQ(replaceStrings("banana", order_test), "orange");
}

// Test SVVtoSV function
TEST_F(StringUtilsTest, SVVtoSV) {
    std::vector<std::string_view> input = {"one", "two", "three"};
    auto result = SVVtoSV(input);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "one");
    EXPECT_EQ(result[1], "two");
    EXPECT_EQ(result[2], "three");

    // Test with empty array
    std::vector<std::string_view> empty_input;
    auto empty_result = SVVtoSV(empty_input);
    EXPECT_TRUE(empty_result.empty());
}

// Test explode function
TEST_F(StringUtilsTest, Explode) {
    EXPECT_TRUE(explode("", ',').empty());
    EXPECT_THAT(explode("hello", ','), ElementsAre("hello"));
    EXPECT_THAT(explode("hello,world", ','), ElementsAre("hello", "world"));
    EXPECT_THAT(explode("one,two,three", ','),
                ElementsAre("one", "two", "three"));
    EXPECT_THAT(explode(",one,,two,", ','),
                ElementsAre("", "one", "", "two", ""));

    // Test with different delimiters
    EXPECT_THAT(explode("a b c", ' '), ElementsAre("a", "b", "c"));
    EXPECT_THAT(explode("1|2|3", '|'), ElementsAre("1", "2", "3"));
}

// Test trim function
TEST_F(StringUtilsTest, Trim) {
    EXPECT_EQ(trim(""), "");
    EXPECT_EQ(trim("hello"), "hello");
    EXPECT_EQ(trim(" hello"), "hello");
    EXPECT_EQ(trim("hello "), "hello");
    EXPECT_EQ(trim(" hello "), "hello");
    EXPECT_EQ(trim("\t hello \n"), "hello");
    EXPECT_EQ(trim("\r\n\t hello world \t\r\n"), "hello world");

    // Test with all whitespace
    EXPECT_EQ(trim("   "), "");
    EXPECT_EQ(trim("\t\n\r "), "");

    // Test with custom symbols
    EXPECT_EQ(trim("***hello***", "*"), "hello");
    EXPECT_EQ(trim("123hello123", "123"), "hello");
    EXPECT_EQ(trim("ab hello cd", "abcd"), " hello ");
}

// Test stringToWString and wstringToString function
TEST_F(StringUtilsTest, StringWStringConversions) {
    std::string original = "Hello, world! 123";
    auto wide = stringToWString(original);
    auto back = wstringToString(wide);

    EXPECT_EQ(back, original);
    EXPECT_EQ(stringToWString(""), L"");
    EXPECT_EQ(wstringToString(L""), "");

    // Test with non-ASCII characters
    std::string utf8_str = "こんにちは世界";
    auto wide_utf8 = stringToWString(utf8_str);
    auto back_utf8 = wstringToString(wide_utf8);

    EXPECT_EQ(back_utf8, utf8_str);
}

// Test stod function
TEST_F(StringUtilsTest, Stod) {
    EXPECT_DOUBLE_EQ(stod("123.45"), 123.45);
    EXPECT_DOUBLE_EQ(stod("-123.45"), -123.45);
    EXPECT_DOUBLE_EQ(stod("0"), 0.0);
    EXPECT_DOUBLE_EQ(stod("1e10"), 1e10);
    EXPECT_DOUBLE_EQ(stod("-1.23e-10"), -1.23e-10);

    // Test with idx parameter
    std::size_t idx;
    EXPECT_DOUBLE_EQ(stod("123.45abc", &idx), 123.45);
    EXPECT_EQ(idx, 6);

    // Test exceptions
    EXPECT_THROW(stod(""), std::invalid_argument);
    EXPECT_THROW(stod("abc"), std::invalid_argument);
    EXPECT_THROW(stod("1.2.3"), std::invalid_argument);

    // Test large values
    EXPECT_DOUBLE_EQ(stod("1.7976931348623157e+308"),
                     1.7976931348623157e+308);  // Max double
    EXPECT_THROW(stod("1.7976931348623157e+309"),
                 std::out_of_range);  // Overflow
}

// Test stof function
TEST_F(StringUtilsTest, Stof) {
    EXPECT_FLOAT_EQ(stof("123.45"), 123.45f);
    EXPECT_FLOAT_EQ(stof("-123.45"), -123.45f);
    EXPECT_FLOAT_EQ(stof("0"), 0.0f);
    EXPECT_FLOAT_EQ(stof("1e10"), 1e10f);
    EXPECT_FLOAT_EQ(stof("-1.23e-10"), -1.23e-10f);

    // Test with idx parameter
    std::size_t idx;
    EXPECT_FLOAT_EQ(stof("123.45abc", &idx), 123.45f);
    EXPECT_EQ(idx, 6);

    // Test exceptions
    EXPECT_THROW(stof(""), std::invalid_argument);
    EXPECT_THROW(stof("abc"), std::invalid_argument);
    EXPECT_THROW(stof("1.2.3"), std::invalid_argument);

    // Test large values
    EXPECT_THROW(stof("3.5e+38"), std::out_of_range);  // Overflow
}

// Test stoi function
TEST_F(StringUtilsTest, Stoi) {
    EXPECT_EQ(stoi("123"), 123);
    EXPECT_EQ(stoi("-123"), -123);
    EXPECT_EQ(stoi("0"), 0);

    // Test with idx parameter
    std::size_t idx;
    EXPECT_EQ(stoi("123abc", &idx), 123);
    EXPECT_EQ(idx, 3);

    // Test with different bases
    EXPECT_EQ(stoi("1010", nullptr, 2), 10);  // Binary
    EXPECT_EQ(stoi("1A", nullptr, 16), 26);   // Hex
    EXPECT_EQ(stoi("777", nullptr, 8), 511);  // Octal

    // Test exceptions
    EXPECT_THROW(stoi(""), std::invalid_argument);
    EXPECT_THROW(stoi("abc"), std::invalid_argument);
    EXPECT_THROW(stoi("9", nullptr, 8),
                 std::invalid_argument);  // Invalid digit for base

    // Test large values
    EXPECT_THROW(stoi("2147483648"), std::out_of_range);   // INT_MAX + 1
    EXPECT_THROW(stoi("-2147483649"), std::out_of_range);  // INT_MIN - 1
}

// Test stol function
TEST_F(StringUtilsTest, Stol) {
    EXPECT_EQ(stol("123"), 123L);
    EXPECT_EQ(stol("-123"), -123L);
    EXPECT_EQ(stol("0"), 0L);

    // Test with idx parameter
    std::size_t idx;
    EXPECT_EQ(stol("123abc", &idx), 123L);
    EXPECT_EQ(idx, 3);

    // Test with different bases
    EXPECT_EQ(stol("1010", nullptr, 2), 10L);  // Binary
    EXPECT_EQ(stol("1A", nullptr, 16), 26L);   // Hex
    EXPECT_EQ(stol("777", nullptr, 8), 511L);  // Octal

    // Test exceptions
    EXPECT_THROW(stol(""), std::invalid_argument);
    EXPECT_THROW(stol("abc"), std::invalid_argument);
    EXPECT_THROW(stol("9", nullptr, 8),
                 std::invalid_argument);  // Invalid digit for base

    // Test large values (depends on platform)
    std::string max_plus_one =
        std::to_string(std::numeric_limits<long>::max()) + "0";
    EXPECT_THROW(stol(max_plus_one), std::out_of_range);
}

// Test nstrtok function
TEST_F(StringUtilsTest, Nstrtok) {
    std::string_view text = "  hello,world; test\tstring  ";
    std::string_view delims = " ,;\t";

    auto token1 = nstrtok(text, delims);
    ASSERT_TRUE(token1.has_value());
    EXPECT_EQ(*token1, "hello");

    auto token2 = nstrtok(text, delims);
    ASSERT_TRUE(token2.has_value());
    EXPECT_EQ(*token2, "world");

    auto token3 = nstrtok(text, delims);
    ASSERT_TRUE(token3.has_value());
    EXPECT_EQ(*token3, "test");

    auto token4 = nstrtok(text, delims);
    ASSERT_TRUE(token4.has_value());
    EXPECT_EQ(*token4, "string");

    auto token5 = nstrtok(text, delims);
    EXPECT_FALSE(token5.has_value());

    // Test with empty string
    std::string_view empty_text;
    EXPECT_FALSE(nstrtok(empty_text, delims).has_value());

    // Test with string containing only delimiters
    std::string_view only_delims = " \t,;";
    EXPECT_FALSE(nstrtok(only_delims, delims).has_value());
}

// Test split function with char delimiter
TEST_F(StringUtilsTest, SplitWithChar) {
    std::vector<std::string> result;

    // Basic split
    for (const auto& part : split("a,b,c", ',')) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "b", "c"));

    // Split with trim
    result.clear();
    for (const auto& part : split(" a , b , c ", ',', true)) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "b", "c"));

    // Split with skip empty
    result.clear();
    for (const auto& part : split("a,,c", ',', false, true)) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "c"));

    // Split with trim and skip empty
    result.clear();
    for (const auto& part : split(" a ,  , c ", ',', true, true)) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "c"));

    // Empty string
    result.clear();
    for (const auto& part : split("", ',')) {
        result.push_back(std::string(part));
    }
    EXPECT_TRUE(result.empty());
}

// Test split function with string_view delimiter
TEST_F(StringUtilsTest, SplitWithStringView) {
    std::vector<std::string> result;

    // Basic split
    for (const auto& part : split("a::b::c", "::")) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "b", "c"));

    // Split with trim
    result.clear();
    for (const auto& part : split(" a :: b :: c ", "::", true)) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "b", "c"));

    // Complex delimiter
    result.clear();
    for (const auto& part : split("a<=>b<=>c", "<=>")) {
        result.push_back(std::string(part));
    }
    EXPECT_THAT(result, ElementsAre("a", "b", "c"));
}

// Test split function with predicate
TEST_F(StringUtilsTest, SplitWithPredicate) {
    std::vector<std::string> result;

    // Split on spaces and punctuation
    auto is_space_or_punct = [](char c) {
        return std::isspace(c) || std::ispunct(c);
    };

    for (const auto& part :
         split("Hello, world! This is a test.", is_space_or_punct)) {
        if (!part.empty()) {
            result.push_back(std::string(part));
        }
    }
    EXPECT_THAT(result,
                ElementsAre("Hello", "world", "This", "is", "a", "test"));

    // Split on digits
    result.clear();
    auto is_digit = [](char c) { return std::isdigit(c); };
    for (const auto& part : split("abc123def456ghi", is_digit)) {
        if (!part.empty()) {
            result.push_back(std::string(part));
        }
    }
    EXPECT_THAT(result, ElementsAre("abc", "def", "ghi"));
}

// Test collectVector and collectList methods
TEST_F(StringUtilsTest, SplitCollectMethods) {
    // collectVector
    auto vec = split("a,b,c", ',').collectVector();
    EXPECT_THAT(vec, ElementsAre("a", "b", "c"));

    // collectList
    auto list = split("a,b,c", ',').collectList();
    ASSERT_EQ(list.size(), 3);
    auto it = list.begin();
    EXPECT_EQ(*it++, "a");
    EXPECT_EQ(*it++, "b");
    EXPECT_EQ(*it++, "c");

    // collectArray
    auto arr = split("a,b,c", ',').collectArray<3>();
    EXPECT_EQ(arr[0], "a");
    EXPECT_EQ(arr[1], "b");
    EXPECT_EQ(arr[2], "c");

    // collectArray with more elements in string than array
    auto arr2 = split("a,b,c,d,e", ',').collectArray<3>();
    EXPECT_EQ(arr2[0], "a");
    EXPECT_EQ(arr2[1], "b");
    EXPECT_EQ(arr2[2], "c");

    // collectArray with fewer elements in string than array
    auto arr3 = split("a,b", ',').collectArray<3>();
    EXPECT_EQ(arr3[0], "a");
    EXPECT_EQ(arr3[1], "b");
    EXPECT_EQ(arr3[2], "");
}

// Test the additional helper functions
TEST_F(StringUtilsTest, ParallelReplaceString) {
    std::string original = "abcabcabc";
    std::string result = parallelReplaceString(original, "abc", "xyz");
    EXPECT_EQ(result, "xyzxyzxyz");

    // Test with empty inputs
    EXPECT_EQ(parallelReplaceString("", "abc", "xyz"), "");
    EXPECT_EQ(parallelReplaceString("abc", "", "xyz"), "abc");
    EXPECT_EQ(parallelReplaceString("abc", "abc", ""), "");

    // Test with no matches
    EXPECT_EQ(parallelReplaceString("abc", "xyz", "123"), "abc");

    // Test large string to force parallel version
    std::string large_string(20000, 'a');
    EXPECT_EQ(parallelReplaceString(large_string, "a", "b").size(), 20000);
    EXPECT_EQ(parallelReplaceString(large_string, "a", "b")[0], 'b');
}

TEST_F(StringUtilsTest, ParallelSVVtoSV) {
    // Test with small array
    std::vector<std::string_view> small_array = {"a", "b", "c"};
    auto small_result = parallelSVVtoSV(small_array);
    EXPECT_THAT(small_result, ElementsAre("a", "b", "c"));

    // Test with empty array
    std::vector<std::string_view> empty_array;
    auto empty_result = parallelSVVtoSV(empty_array);
    EXPECT_TRUE(empty_result.empty());

    // Test with large array to force parallel version
    std::vector<std::string_view> large_array(2000, "test");
    auto large_result = parallelSVVtoSV(large_array);
    EXPECT_EQ(large_result.size(), 2000);
    EXPECT_EQ(large_result[0], "test");
}

TEST_F(StringUtilsTest, ToLower) {
    EXPECT_EQ(toLower(""), "");
    EXPECT_EQ(toLower("abcdef"), "abcdef");
    EXPECT_EQ(toLower("ABCDEF"), "abcdef");
    EXPECT_EQ(toLower("AbCdEf"), "abcdef");
    EXPECT_EQ(toLower("123!@#"), "123!@#");
    EXPECT_EQ(toLower("MIXED Case 123"), "mixed case 123");
}

TEST_F(StringUtilsTest, ToUpper) {
    EXPECT_EQ(toUpper(""), "");
    EXPECT_EQ(toUpper("abcdef"), "ABCDEF");
    EXPECT_EQ(toUpper("ABCDEF"), "ABCDEF");
    EXPECT_EQ(toUpper("AbCdEf"), "ABCDEF");
    EXPECT_EQ(toUpper("123!@#"), "123!@#");
    EXPECT_EQ(toUpper("mixed Case 123"), "MIXED CASE 123");
}
