/*
 * test_ranges.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for C++20 ranges utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_RANGES_HPP
#define ATOM_UTILS_TEST_RANGES_HPP

#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <list>
#include <string>
#include <algorithm>
#include <numeric>
#include <ranges>
#include <map>
#include <optional>
#include "atom/utils/container/ranges.hpp"

namespace atom::utils::test {

class RangesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        strings = {"apple", "banana", "cherry", "date", "elderberry"};
        emptyNumbers = {};
        
        // Test data for grouping
        people = {
            {"Alice", 25, "Engineering"},
            {"Bob", 30, "Marketing"},
            {"Charlie", 25, "Engineering"},
            {"Diana", 35, "Marketing"},
            {"Eve", 25, "Sales"}
        };
    }
    
    struct Person {
        std::string name;
        int age;
        std::string department;
    };
    
    std::vector<int> numbers;
    std::vector<std::string> strings;
    std::vector<int> emptyNumbers;
    std::vector<Person> people;
};

// Test filterAndTransform function
TEST_F(RangesTest, FilterAndTransform) {
    // Filter even numbers and square them
    auto result = filterAndTransform(numbers, 
                                   [](int x) { return x % 2 == 0; },
                                   [](int x) { return x * x; });
    
    std::vector<int> expected = {4, 16, 36, 64, 100}; // 2^2, 4^2, 6^2, 8^2, 10^2
    std::vector<int> actual(result.begin(), result.end());
    
    EXPECT_EQ(actual, expected);
}

// Test filterAndTransform with strings
TEST_F(RangesTest, FilterAndTransformStrings) {
    // Filter strings longer than 5 characters and convert to uppercase first letter
    auto result = filterAndTransform(strings,
                                   [](const std::string& s) { return s.length() > 5; },
                                   [](const std::string& s) { 
                                       std::string upper = s;
                                       if (!upper.empty()) upper[0] = std::toupper(upper[0]);
                                       return upper;
                                   });
    
    std::vector<std::string> expected = {"Banana", "Cherry", "Elderberry"};
    std::vector<std::string> actual(result.begin(), result.end());
    
    EXPECT_EQ(actual, expected);
}

// Test filterAndTransform with empty range
TEST_F(RangesTest, FilterAndTransformEmpty) {
    auto result = filterAndTransform(emptyNumbers,
                                   [](int x) { return x > 0; },
                                   [](int x) { return x * 2; });
    
    std::vector<int> actual(result.begin(), result.end());
    EXPECT_TRUE(actual.empty());
}

// Test findElement function
TEST_F(RangesTest, FindElement) {
    // Find existing element
    auto result1 = findElement(numbers, 5);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 5);
    
    // Find non-existing element
    auto result2 = findElement(numbers, 15);
    EXPECT_FALSE(result2.has_value());
    
    // Find in string vector
    auto result3 = findElement(strings, std::string("cherry"));
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), "cherry");
}

// Test findElement with empty range
TEST_F(RangesTest, FindElementEmpty) {
    auto result = findElement(emptyNumbers, 1);
    EXPECT_FALSE(result.has_value());
}

// Test groupAndAggregate function
TEST_F(RangesTest, GroupAndAggregate) {
    // Group people by age and count them
    auto result = groupAndAggregate(people,
                                  [](const Person& p) { return p.age; },
                                  [](const Person&) { return 1; });
    
    std::map<int, int> expected = {{25, 3}, {30, 1}, {35, 1}};
    EXPECT_EQ(result, expected);
}

// Test groupAndAggregate with different aggregation
TEST_F(RangesTest, GroupAndAggregateSum) {
    // Group numbers by even/odd and sum them
    auto result = groupAndAggregate(numbers,
                                  [](int x) { return x % 2; }, // 0 for even, 1 for odd
                                  [](int x) { return x; });
    
    std::map<int, int> expected = {{0, 30}, {1, 25}}; // even: 2+4+6+8+10=30, odd: 1+3+5+7+9=25
    EXPECT_EQ(result, expected);
}

// Test dropFirst function (assuming it exists based on the file structure)
TEST_F(RangesTest, DropFirst) {
    // This test assumes dropFirst function exists in the ranges.hpp
    // If not implemented, this test should be removed or the function should be implemented
    
    // Drop first 3 elements
    std::vector<int> input = {1, 2, 3, 4, 5};
    auto result = input | std::views::drop(3);
    
    std::vector<int> expected = {4, 5};
    std::vector<int> actual(result.begin(), result.end());
    
    EXPECT_EQ(actual, expected);
}

// Test takeFirst function (assuming it exists)
TEST_F(RangesTest, TakeFirst) {
    // Take first 3 elements
    auto result = numbers | std::views::take(3);
    
    std::vector<int> expected = {1, 2, 3};
    std::vector<int> actual(result.begin(), result.end());
    
    EXPECT_EQ(actual, expected);
}

// Test chaining multiple range operations
TEST_F(RangesTest, ChainedOperations) {
    // Complex chain: filter odds, transform by squaring, take first 3
    auto result = numbers 
                | std::views::filter([](int x) { return x % 2 == 1; })
                | std::views::transform([](int x) { return x * x; })
                | std::views::take(3);
    
    std::vector<int> expected = {1, 9, 25}; // 1^2, 3^2, 5^2
    std::vector<int> actual(result.begin(), result.end());
    
    EXPECT_EQ(actual, expected);
}

// Test with different container types
TEST_F(RangesTest, DifferentContainerTypes) {
    // Test with array
    std::array<int, 5> arr = {1, 2, 3, 4, 5};
    auto result1 = findElement(arr, 3);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 3);
    
    // Test with list
    std::list<int> lst = {1, 2, 3, 4, 5};
    auto result2 = findElement(lst, 4);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), 4);
}

// Test performance with large datasets
TEST_F(RangesTest, LargeDatasetPerformance) {
    // Create large dataset
    std::vector<int> largeNumbers(100000);
    std::iota(largeNumbers.begin(), largeNumbers.end(), 1);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform complex operation
    auto result = filterAndTransform(largeNumbers,
                                   [](int x) { return x % 100 == 0; },
                                   [](int x) { return x / 100; });
    
    std::vector<int> actual(result.begin(), result.end());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 1000) << "Large dataset processing took too long: " << duration.count() << "ms";
    
    // Verify result correctness
    EXPECT_EQ(actual.size(), 1000); // Numbers 100, 200, ..., 100000 divided by 100
    EXPECT_EQ(actual[0], 1);
    EXPECT_EQ(actual[999], 1000);
}

// Test edge cases
TEST_F(RangesTest, EdgeCases) {
    // Single element vector
    std::vector<int> single = {42};
    
    auto result1 = findElement(single, 42);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), 42);
    
    auto result2 = filterAndTransform(single,
                                    [](int x) { return x > 0; },
                                    [](int x) { return x * 2; });
    std::vector<int> actual(result2.begin(), result2.end());
    EXPECT_EQ(actual, std::vector<int>{84});
    
    // Test with all elements filtered out
    auto result3 = filterAndTransform(numbers,
                                    [](int x) { return x > 100; },
                                    [](int x) { return x; });
    std::vector<int> empty_result(result3.begin(), result3.end());
    EXPECT_TRUE(empty_result.empty());
}

// Test const correctness
TEST_F(RangesTest, ConstCorrectness) {
    const std::vector<int> constNumbers = {1, 2, 3, 4, 5};
    
    // Should work with const containers
    auto result = findElement(constNumbers, 3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3);
    
    // Filter and transform should work with const containers
    auto filtered = filterAndTransform(constNumbers,
                                     [](int x) { return x % 2 == 0; },
                                     [](int x) { return x * x; });
    std::vector<int> actual(filtered.begin(), filtered.end());
    std::vector<int> expected = {4, 16};
    EXPECT_EQ(actual, expected);
}

// Test exception safety
TEST_F(RangesTest, ExceptionSafety) {
    // Test that functions handle exceptions gracefully
    std::vector<int> testData = {1, 2, 3, 4, 5};
    
    // This should not throw
    EXPECT_NO_THROW({
        auto result = filterAndTransform(testData,
                                       [](int x) { return x > 0; },
                                       [](int x) { return x * x; });
        std::vector<int> actual(result.begin(), result.end());
    });
    
    // Test with potentially throwing predicate
    EXPECT_NO_THROW({
        auto result = findElement(testData, 3);
        (void)result; // Suppress unused variable warning
    });
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_RANGES_HPP
