#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

#include "atom/type/static_string.hpp"

class StaticStringTest : public ::testing::Test {
protected:
    // Common test setup
    void SetUp() override {}
    void TearDown() override {}

    // Helper functions for common test operations
    template <size_t N>
    void verifyStringEquals(const StaticString<N>& str,
                            const std::string& expected) {
        EXPECT_EQ(str.size(), expected.size());
        EXPECT_EQ(std::string_view(str.data(), str.size()), expected);

        // Also verify null termination
        EXPECT_EQ(str.data()[str.size()], '\0');
    }
};

// Basic Constructors Tests
TEST_F(StaticStringTest, DefaultConstructor) {
    StaticString<10> str;
    EXPECT_TRUE(str.empty());
    EXPECT_EQ(str.size(), 0);
    EXPECT_EQ(str.data()[0], '\0');
}

TEST_F(StaticStringTest, CStringConstructor) {
    const char* cstr = "Hello";
    StaticString<10> str(cstr);

    verifyStringEquals(str, "Hello");
}

TEST_F(StaticStringTest, StringLiteralConstructor) {
    StaticString<10> str("Hello");

    verifyStringEquals(str, "Hello");
}

TEST_F(StaticStringTest, StringViewConstructor) {
    std::string_view sv = "Hello";
    StaticString<10> str(sv);

    verifyStringEquals(str, "Hello");
}

TEST_F(StaticStringTest, ArrayConstructor) {
    std::array<char, 11> arr = {'H', 'e', 'l', 'l', 'o', '\0'};
    StaticString<10> str(arr);

    verifyStringEquals(str, "Hello");
}

TEST_F(StaticStringTest, CopyConstructor) {
    StaticString<10> str1("Hello");
    StaticString<10> str2(str1);

    verifyStringEquals(str2, "Hello");
}

TEST_F(StaticStringTest, MoveConstructor) {
    StaticString<10> str1("Hello");
    StaticString<10> str2(std::move(str1));

    verifyStringEquals(str2, "Hello");
    EXPECT_TRUE(str1.empty());  // Original should be cleared after move
}

TEST_F(StaticStringTest, CopyAssignment) {
    StaticString<10> str1("Hello");
    StaticString<10> str2;
    str2 = str1;

    verifyStringEquals(str2, "Hello");
}

TEST_F(StaticStringTest, MoveAssignment) {
    StaticString<10> str1("Hello");
    StaticString<10> str2;
    str2 = std::move(str1);

    verifyStringEquals(str2, "Hello");
    EXPECT_TRUE(str1.empty());  // Original should be cleared after move
}

// Error Handling Tests
TEST_F(StaticStringTest, ConstructionExceptions) {
    // Null pointer
    EXPECT_THROW(StaticString<10>(static_cast<const char*>(nullptr)),
                 std::invalid_argument);

    // String too large
    const char* long_string =
        "This string is definitely too long for a StaticString<10>";
    EXPECT_THROW(StaticString<10>(long_string), std::runtime_error);

    // String view too large
    std::string_view long_sv = long_string;
    EXPECT_THROW(StaticString<10>(long_sv), std::runtime_error);
}

TEST_F(StaticStringTest, StaticAssertCompileTimeCheck) {
    // This would trigger a static_assert if uncommented:
    // StaticString<5> too_small("longer than 5");

    // These should compile fine:
    StaticString<5> just_right_1("five");
    StaticString<5> just_right_2("12345");
}

// Element Access Tests
TEST_F(StaticStringTest, ElementAccess) {
    StaticString<10> str("Hello");

    // operator[]
    EXPECT_EQ(str[0], 'H');
    EXPECT_EQ(str[4], 'o');

    // at() with bounds checking
    EXPECT_EQ(str.at(0), 'H');
    EXPECT_EQ(str.at(4), 'o');
    EXPECT_THROW(str.at(5), std::out_of_range);

    // front/back
    EXPECT_EQ(str.front(), 'H');
    EXPECT_EQ(str.back(), 'o');

    // front/back on empty string
    StaticString<10> empty;
    EXPECT_THROW(empty.front(), std::out_of_range);
    EXPECT_THROW(empty.back(), std::out_of_range);

    // Modify elements
    str[0] = 'J';
    EXPECT_EQ(str[0], 'J');
    verifyStringEquals(str, "Jello");

    str.at(4) = 'y';
    EXPECT_EQ(str[4], 'y');
    verifyStringEquals(str, "Jelly");
}

// Iterator Tests
TEST_F(StaticStringTest, Iterators) {
    StaticString<10> str("Hello");

    // Basic iteration
    std::string result;
    for (auto it = str.begin(); it != str.end(); ++it) {
        result += *it;
    }
    EXPECT_EQ(result, "Hello");

    // Range-based for loop
    result.clear();
    for (char c : str) {
        result += c;
    }
    EXPECT_EQ(result, "Hello");

    // const iterators
    result.clear();
    for (auto it = str.cbegin(); it != str.cend(); ++it) {
        result += *it;
    }
    EXPECT_EQ(result, "Hello");

    // Modify through iterator
    *(str.begin()) = 'J';
    verifyStringEquals(str, "Jello");

    // Empty string iterators
    StaticString<10> empty;
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.cbegin(), empty.cend());
}

// Modification Tests
TEST_F(StaticStringTest, Clear) {
    StaticString<10> str("Hello");
    EXPECT_FALSE(str.empty());

    str.clear();
    EXPECT_TRUE(str.empty());
    EXPECT_EQ(str.size(), 0);
    EXPECT_EQ(str.data()[0], '\0');
}

TEST_F(StaticStringTest, PushBackAndPopBack) {
    StaticString<10> str("Hello");

    // Push back
    str.push_back('!');
    verifyStringEquals(str, "Hello!");

    // Push back overflow
    StaticString<5> small("12345");
    EXPECT_THROW(small.push_back('6'), std::runtime_error);

    // Pop back
    str.pop_back();
    verifyStringEquals(str, "Hello");

    // Pop back empty
    StaticString<10> empty;
    EXPECT_THROW(empty.pop_back(), std::runtime_error);
}

TEST_F(StaticStringTest, Append) {
    StaticString<20> str("Hello");

    // Append string_view
    str.append(std::string_view(" World"));
    verifyStringEquals(str, "Hello World");

    // Append StaticString
    StaticString<10> suffix("!");
    str.append(suffix);
    verifyStringEquals(str, "Hello World!");

    // Append overflow
    StaticString<10> small("12345");
    EXPECT_THROW(small.append("678901"), std::runtime_error);
}

TEST_F(StaticStringTest, Resize) {
    StaticString<10> str("Hello");

    // Resize smaller
    str.resize(3);
    verifyStringEquals(str, "Hel");

    // Resize larger with default char
    str.resize(5);
    verifyStringEquals(str, "Hel\0\0");

    // Resize larger with custom char
    str.resize(7, 'x');
    verifyStringEquals(str, "Hel\0\0xx");

    // Resize overflow
    EXPECT_THROW(str.resize(11), std::runtime_error);
}

TEST_F(StaticStringTest, Substr) {
    StaticString<20> str("Hello World");

    // Normal substring
    auto sub1 = str.substr(6, 5);
    verifyStringEquals(sub1, "World");

    // Substring from start
    auto sub2 = str.substr(0, 5);
    verifyStringEquals(sub2, "Hello");

    // Substring to end
    auto sub3 = str.substr(6);
    verifyStringEquals(sub3, "World");

    // Empty substring
    auto sub4 = str.substr(0, 0);
    verifyStringEquals(sub4, "");

    // Out of range
    EXPECT_THROW(str.substr(20), std::out_of_range);

    // Count beyond string length (should truncate)
    auto sub5 = str.substr(6, 100);
    verifyStringEquals(sub5, "World");
}

TEST_F(StaticStringTest, Find) {
    StaticString<20> str("Hello World");

    // Find char
    EXPECT_EQ(str.find('W'), 6);
    EXPECT_EQ(str.find('o'), 4);                       // First occurrence
    EXPECT_EQ(str.find('o', 5), 7);                    // With pos
    EXPECT_EQ(str.find('z'), StaticString<20>::npos);  // Not found

    // Find string
    EXPECT_EQ(str.find("World"), 6);
    EXPECT_EQ(str.find("llo"), 2);
    EXPECT_EQ(str.find("llo", 3), StaticString<20>::npos);  // After pos
    EXPECT_EQ(str.find("xyz"), StaticString<20>::npos);     // Not found

    // Find empty string
    EXPECT_EQ(str.find(""), 0);

    // Find in empty string
    StaticString<10> empty;
    EXPECT_EQ(empty.find('a'), StaticString<10>::npos);
    EXPECT_EQ(empty.find("a"), StaticString<10>::npos);
}

TEST_F(StaticStringTest, Replace) {
    StaticString<20> str("Hello World");

    // Replace middle
    str.replace(6, 5, "Earth");
    verifyStringEquals(str, "Hello Earth");

    // Replace at beginning
    str.replace(0, 5, "Goodbye");
    verifyStringEquals(str, "Goodbye Earth");

    // Replace at end
    str.replace(8, 5, "Moon");
    verifyStringEquals(str, "Goodbye Moon");

    // Replace with shorter string
    str.replace(0, 7, "Hi");
    verifyStringEquals(str, "Hi Moon");

    // Replace with longer string
    str.replace(3, 4, " beautiful World");
    verifyStringEquals(str, "Hi beautiful World");

    // Replace out of range
    EXPECT_THROW(str.replace(50, 1, "x"), std::out_of_range);

    // Replace overflow
    EXPECT_THROW(str.replace(0, 0, std::string(30, 'x')), std::runtime_error);
}

TEST_F(StaticStringTest, Insert) {
    StaticString<20> str("Hello World");

    // Insert in middle
    str.insert(5, " beautiful");
    verifyStringEquals(str, "Hello beautiful World");

    // Insert at start
    str.insert(0, "Oh, ");
    verifyStringEquals(str, "Oh, Hello beautiful World");

    // Insert overflow
    EXPECT_THROW(str.insert(0, std::string(10, 'x')), std::runtime_error);

    // Insert out of range
    EXPECT_THROW(str.insert(50, "x"), std::out_of_range);
}

TEST_F(StaticStringTest, Erase) {
    StaticString<20> str("Hello beautiful World");

    // Erase middle
    str.erase(6, 10);
    verifyStringEquals(str, "Hello World");

    // Erase to end
    str.erase(5);
    verifyStringEquals(str, "Hello");

    // Erase all
    str.erase(0);
    EXPECT_TRUE(str.empty());

    // Erase from empty
    EXPECT_NO_THROW(str.erase(0));  // Should be safe

    // Erase out of range
    EXPECT_THROW(str.erase(50), std::out_of_range);
}

// Comparison Tests
TEST_F(StaticStringTest, Comparisons) {
    StaticString<10> str1("Hello");
    StaticString<10> str2("Hello");
    StaticString<10> str3("World");

    // StaticString comparisons
    EXPECT_TRUE(str1 == str2);
    EXPECT_FALSE(str1 == str3);
    EXPECT_TRUE(str1 != str3);
    EXPECT_FALSE(str1 != str2);

    // string_view comparisons
    EXPECT_TRUE(str1 == std::string_view("Hello"));
    EXPECT_FALSE(str1 == std::string_view("World"));
}

// Operator Tests
TEST_F(StaticStringTest, AppendOperators) {
    StaticString<20> str("Hello");

    // += char
    str += '!';
    verifyStringEquals(str, "Hello!");

    // += string_view
    str += std::string_view(" World");
    verifyStringEquals(str, "Hello! World");

    // += StaticString
    StaticString<5> suffix("!");
    str += suffix;
    verifyStringEquals(str, "Hello! World!");
}

TEST_F(StaticStringTest, ConcatenationOperator) {
    StaticString<5> str1("Hello");
    StaticString<10> str2(" World!");

    // + operator
    auto result = str1 + str2;
    verifyStringEquals(result, "Hello World!");

    // Check types
    EXPECT_EQ(typeid(result), typeid(StaticString<15>));

    // Concatenation overflow
    StaticString<5> small1("12345");
    StaticString<5> small2("67890");
    // This should throw at runtime:
    EXPECT_THROW(small1 + small2 + small2, std::runtime_error);
}

// Conversion Tests
TEST_F(StaticStringTest, Conversions) {
    StaticString<10> str("Hello");

    // string_view conversion
    std::string_view sv = str;
    EXPECT_EQ(sv, "Hello");

    // Stream operator
    std::ostringstream oss;
    oss << str;
    EXPECT_EQ(oss.str(), "Hello");
}

// Special Functions Tests
TEST_F(StaticStringTest, MakeSafe) {
    // Safe case
    auto str1 = StaticString<10>::make_safe("Hello");
    ASSERT_TRUE(str1.has_value());
    verifyStringEquals(*str1, "Hello");

    // Unsafe case (too long)
    auto str2 = StaticString<5>::make_safe("Too long");
    EXPECT_FALSE(str2.has_value());
}

// Capacity Tests
TEST_F(StaticStringTest, Capacity) {
    StaticString<10> str;
    EXPECT_EQ(str.capacity(), 10);

    StaticString<100> large;
    EXPECT_EQ(large.capacity(), 100);
}

// SIMD Optimization Tests
TEST_F(StaticStringTest, SimdFindChar) {
    // Create a string long enough for SIMD to kick in
    std::string long_text(1000, 'a');
    long_text[500] = 'X';

    StaticString<1000> str(long_text);

    // This should use SIMD-accelerated method if available
    EXPECT_EQ(str.find('X'), 500);

    // Test with a character that doesn't exist
    EXPECT_EQ(str.find('Z'), StaticString<1000>::npos);
}

TEST_F(StaticStringTest, SimdEqual) {
    // Create two identical long strings
    std::string long_text(1000, 'a');

    StaticString<1000> str1(long_text);
    StaticString<1000> str2(long_text);

    // This should use SIMD-accelerated comparison if available
    EXPECT_TRUE(str1 == str2);

    // Make strings different
    str2[500] = 'b';
    EXPECT_FALSE(str1 == str2);
}

// Performance Tests (these can be DISABLED for regular runs)
TEST_F(StaticStringTest, DISABLED_PerformanceComparison) {
    const size_t size = 10000;
    const int iterations = 1000;

    // Create test data
    std::string std_str(size, 'a');
    StaticString<10000> static_str(std_str);

    // Find character
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile auto pos = std_str.find('a', i % size);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto std_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile auto pos = static_str.find('a', i % size);
        }
        end = std::chrono::high_resolution_clock::now();
        auto static_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        std::cout << "Find character performance:\n"
                  << "  std::string: " << std_duration << " μs\n"
                  << "  StaticString: " << static_duration << " μs\n"
                  << "  Ratio: "
                  << static_cast<double>(std_duration) / static_duration
                  << "x\n";
    }

    // Comparison
    {
        std::string std_str2 = std_str;
        StaticString<10000> static_str2 = static_str;

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile bool result = std_str == std_str2;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto std_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            volatile bool result = static_str == static_str2;
        }
        end = std::chrono::high_resolution_clock::now();
        auto static_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start)
                .count();

        std::cout << "Comparison performance:\n"
                  << "  std::string: " << std_duration << " μs\n"
                  << "  StaticString: " << static_duration << " μs\n"
                  << "  Ratio: "
                  << static_cast<double>(std_duration) / static_duration
                  << "x\n";
    }
}

// Edge Cases and Corner Cases
TEST_F(StaticStringTest, EdgeCases) {
    // Empty string
    StaticString<10> empty;
    verifyStringEquals(empty, "");
    EXPECT_TRUE(empty.empty());

    // Maximum capacity string
    std::string max_string(10, 'x');
    StaticString<10> max_capacity(max_string);
    verifyStringEquals(max_capacity, max_string);
    EXPECT_EQ(max_capacity.size(), 10);

    // One character string
    StaticString<10> single("X");
    verifyStringEquals(single, "X");
    EXPECT_EQ(single.size(), 1);

    // String with null bytes in the middle
    std::string null_string = "Hello";
    null_string[2] = '\0';

    // We need to use string_view constructor to include null bytes
    StaticString<10> null_str(
        std::string_view(null_string.data(), null_string.size()));

    // Verify the string including null bytes
    EXPECT_EQ(null_str.size(), 5);
    EXPECT_EQ(null_str[0], 'H');
    EXPECT_EQ(null_str[1], 'e');
    EXPECT_EQ(null_str[2], '\0');
    EXPECT_EQ(null_str[3], 'l');
    EXPECT_EQ(null_str[4], 'o');
}

// Deduction Guide Tests
TEST_F(StaticStringTest, DeductionGuides) {
    // Test the deduction guide for array initialization
    StaticString str1 = "Hello";  // Should deduce StaticString<5>

    EXPECT_EQ(str1.capacity(), 5);
    verifyStringEquals(str1, "Hello");

    // Ensure the deduced type is what we expect
    StaticString hello = "Hello";
    StaticString<5> expected = "Hello";
    EXPECT_EQ(typeid(hello), typeid(expected));
}

// Thread Safety Tests (for parallel operations)
TEST_F(StaticStringTest, ParallelOperations) {
    // Test very large string operations to engage parallel processing
    std::string large_str(2000, 'a');
    std::string large_suffix(2000, 'b');

    StaticString<5000> str(large_str);

    // This should trigger parallel append for large strings
    EXPECT_NO_THROW(str.append(large_suffix));

    // Verify the result
    EXPECT_EQ(str.size(), 4000);
    for (size_t i = 0; i < 2000; ++i) {
        EXPECT_EQ(str[i], 'a');
    }
    for (size_t i = 2000; i < 4000; ++i) {
        EXPECT_EQ(str[i], 'b');
    }
}

// Constexpr feature test - these must be at namespace scope, not in a function
namespace {
    // Test that StaticString can be used in constexpr contexts
    constexpr StaticString<5> constexpr_str = "Hello";
    static_assert(constexpr_str.size() == 5, "Constexpr size check failed");
    static_assert(constexpr_str.capacity() == 5,
                  "Constexpr capacity check failed");

    // Test constexpr operations with a function at namespace scope
    constexpr StaticString<10> get_static_string() {
        StaticString<10> str = "Hello";
        return str;
    }

    constexpr auto str = get_static_string();
    static_assert(str.size() == 5,
                  "Constexpr function return size check failed");
}

// Runtime validation of constexpr features
TEST_F(StaticStringTest, ConstexprUsage) {
    // Verify the constexpr values at runtime
    verifyStringEquals(constexpr_str, "Hello");
    verifyStringEquals(str, "Hello");

    // Additional runtime checks can go here
    EXPECT_EQ(constexpr_str.size(), 5);
    EXPECT_EQ(str.size(), 5);
}
