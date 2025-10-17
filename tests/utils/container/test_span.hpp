/*
 * test_span.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for span utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_SPAN_HPP
#define ATOM_UTILS_TEST_SPAN_HPP

#include <gtest/gtest.h>
#include <vector>
#include <array>
#include <span>
#include <algorithm>
#include <numeric>
#include <string>
#include "atom/utils/container/span.hpp"

namespace atom::utils::test {

class SpanTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        intVector = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        doubleVector = {1.1, 2.2, 3.3, 4.4, 5.5};
        stringVector = {"apple", "banana", "cherry", "date", "elderberry"};
        emptyVector = {};

        // Initialize arrays
        intArray = {10, 20, 30, 40, 50};

        // Create spans
        intSpan = std::span<int>(intVector);
        constIntSpan = std::span<const int>(intVector);
        doubleSpan = std::span<double>(doubleVector);
        stringSpan = std::span<std::string>(stringVector);
        emptySpan = std::span<int>(emptyVector);
    }

    std::vector<int> intVector;
    std::vector<double> doubleVector;
    std::vector<std::string> stringVector;
    std::vector<int> emptyVector;
    std::array<int, 5> intArray;

    std::span<int> intSpan;
    std::span<const int> constIntSpan;
    std::span<double> doubleSpan;
    std::span<std::string> stringSpan;
    std::span<int> emptySpan;
};

// Test sum function
TEST_F(SpanTest, Sum) {
    // Test with integer span
    int result = sum(constIntSpan);
    int expected = std::accumulate(intVector.begin(), intVector.end(), 0);
    EXPECT_EQ(result, expected);
    EXPECT_EQ(result, 55); // 1+2+3+4+5+6+7+8+9+10 = 55

    // Test with double span
    double doubleResult = sum(std::span<const double>(doubleVector));
    EXPECT_DOUBLE_EQ(doubleResult, 16.5); // 1.1+2.2+3.3+4.4+5.5 = 16.5

    // Test with empty span
    int emptyResult = sum(std::span<const int>(emptyVector));
    EXPECT_EQ(emptyResult, 0);
}

// Test sum with array
TEST_F(SpanTest, SumArray) {
    std::span<const int> arraySpan(intArray);
    int result = sum(arraySpan);
    EXPECT_EQ(result, 150); // 10+20+30+40+50 = 150
}

// Test contains function
TEST_F(SpanTest, Contains) {
    // Test finding existing element
    EXPECT_TRUE(contains(constIntSpan, 5));
    EXPECT_TRUE(contains(constIntSpan, 1));
    EXPECT_TRUE(contains(constIntSpan, 10));

    // Test finding non-existing element
    EXPECT_FALSE(contains(constIntSpan, 0));
    EXPECT_FALSE(contains(constIntSpan, 11));
    EXPECT_FALSE(contains(constIntSpan, -1));

    // Test with empty span
    EXPECT_FALSE(contains(std::span<const int>(emptyVector), 1));

    // Test with string span
    std::span<const std::string> constStringSpan(stringVector);
    EXPECT_TRUE(contains(constStringSpan, std::string("apple")));
    EXPECT_FALSE(contains(constStringSpan, std::string("grape")));
}

// Test sortSpan function
TEST_F(SpanTest, SortSpan) {
    // Create unsorted vector
    std::vector<int> unsorted = {5, 2, 8, 1, 9, 3};
    std::span<int> unsortedSpan(unsorted);

    // Sort the span
    sortSpan(unsortedSpan);

    // Check if sorted
    std::vector<int> expected = {1, 2, 3, 5, 8, 9};
    EXPECT_EQ(unsorted, expected);

    // Test with already sorted data
    std::vector<int> sorted = {1, 2, 3, 4, 5};
    std::span<int> sortedSpan(sorted);
    sortSpan(sortedSpan);
    EXPECT_EQ(sorted, std::vector<int>({1, 2, 3, 4, 5}));
}

// Test sortSpan with strings
TEST_F(SpanTest, SortSpanStrings) {
    std::vector<std::string> unsortedStrings = {"zebra", "apple", "banana", "cherry"};
    std::span<std::string> stringSpan(unsortedStrings);

    sortSpan(stringSpan);

    std::vector<std::string> expected = {"apple", "banana", "cherry", "zebra"};
    EXPECT_EQ(unsortedStrings, expected);
}

// Test filterSpan function
TEST_F(SpanTest, FilterSpan) {
    // Filter even numbers
    auto evenNumbers = filterSpan(constIntSpan, [](int x) { return x % 2 == 0; });
    std::vector<int> expectedEven = {2, 4, 6, 8, 10};
    EXPECT_EQ(evenNumbers, expectedEven);

    // Filter numbers greater than 5
    auto greaterThanFive = filterSpan(constIntSpan, [](int x) { return x > 5; });
    std::vector<int> expectedGreater = {6, 7, 8, 9, 10};
    EXPECT_EQ(greaterThanFive, expectedGreater);

    // Filter with no matches
    auto noMatches = filterSpan(constIntSpan, [](int x) { return x > 100; });
    EXPECT_TRUE(noMatches.empty());

    // Filter all elements
    auto allElements = filterSpan(constIntSpan, [](int x) { return x > 0; });
    EXPECT_EQ(allElements, intVector);
}

// Test filterSpan with strings
TEST_F(SpanTest, FilterSpanStrings) {
    std::span<const std::string> constStringSpan(stringVector);

    // Filter strings longer than 5 characters
    auto longStrings = filterSpan(constStringSpan,
                                 [](const std::string& s) { return s.length() > 5; });
    std::vector<std::string> expected = {"banana", "cherry", "elderberry"};
    EXPECT_EQ(longStrings, expected);
}

// Test countIfSpan function
TEST_F(SpanTest, CountIfSpan) {
    // Count even numbers
    size_t evenCount = countIfSpan(constIntSpan, [](int x) { return x % 2 == 0; });
    EXPECT_EQ(evenCount, 5); // 2, 4, 6, 8, 10

    // Count odd numbers
    size_t oddCount = countIfSpan(constIntSpan, [](int x) { return x % 2 == 1; });
    EXPECT_EQ(oddCount, 5); // 1, 3, 5, 7, 9

    // Count numbers greater than 5
    size_t greaterCount = countIfSpan(constIntSpan, [](int x) { return x > 5; });
    EXPECT_EQ(greaterCount, 5); // 6, 7, 8, 9, 10

    // Count with no matches
    size_t noMatches = countIfSpan(constIntSpan, [](int x) { return x > 100; });
    EXPECT_EQ(noMatches, 0);

    // Count all elements
    size_t allCount = countIfSpan(constIntSpan, [](int x) { return x > 0; });
    EXPECT_EQ(allCount, intVector.size());
}

// Test with empty spans
TEST_F(SpanTest, EmptySpanOperations) {
    std::span<const int> emptyConstSpan(emptyVector);

    // Sum of empty span should be 0
    EXPECT_EQ(sum(emptyConstSpan), 0);

    // Contains on empty span should return false
    EXPECT_FALSE(contains(emptyConstSpan, 1));

    // Filter on empty span should return empty vector
    auto filtered = filterSpan(emptyConstSpan, [](int x) { return x > 0; });
    EXPECT_TRUE(filtered.empty());

    // Count on empty span should return 0
    size_t count = countIfSpan(emptyConstSpan, [](int x) { return x > 0; });
    EXPECT_EQ(count, 0);
}

// Test with single element spans
TEST_F(SpanTest, SingleElementSpan) {
    std::vector<int> singleElement = {42};
    std::span<const int> singleSpan(singleElement);

    // Test sum
    EXPECT_EQ(sum(singleSpan), 42);

    // Test contains
    EXPECT_TRUE(contains(singleSpan, 42));
    EXPECT_FALSE(contains(singleSpan, 43));

    // Test filter
    auto filtered = filterSpan(singleSpan, [](int x) { return x == 42; });
    EXPECT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered[0], 42);

    // Test count
    size_t count = countIfSpan(singleSpan, [](int x) { return x == 42; });
    EXPECT_EQ(count, 1);
}

// Test span subranges
TEST_F(SpanTest, SpanSubranges) {
    // Test with subspan
    auto subspan = constIntSpan.subspan(2, 3); // Elements 3, 4, 5

    EXPECT_EQ(sum(subspan), 12); // 3+4+5 = 12
    EXPECT_TRUE(contains(subspan, 4));
    EXPECT_FALSE(contains(subspan, 1));

    auto filtered = filterSpan(subspan, [](int x) { return x % 2 == 0; });
    std::vector<int> expected = {4};
    EXPECT_EQ(filtered, expected);
}

// Test performance with large spans
TEST_F(SpanTest, LargeSpanPerformance) {
    // Create large vector
    std::vector<int> largeVector(100000);
    std::iota(largeVector.begin(), largeVector.end(), 1);
    std::span<const int> largeSpan(largeVector);

    auto start = std::chrono::high_resolution_clock::now();

    // Perform operations
    int sumResult = sum(largeSpan);
    bool containsResult = contains(largeSpan, 50000);
    size_t countResult = countIfSpan(largeSpan, [](int x) { return x % 2 == 0; });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify results
    EXPECT_EQ(sumResult, 5000050000LL); // Sum of 1 to 100000
    EXPECT_TRUE(containsResult);
    EXPECT_EQ(countResult, 50000); // Half are even

    // Performance should be reasonable
    EXPECT_LT(duration.count(), 1000) << "Large span operations took too long: " << duration.count() << "ms";
}

// Test const correctness
TEST_F(SpanTest, ConstCorrectness) {
    const std::vector<int> constVector = {1, 2, 3, 4, 5};
    std::span<const int> constSpan(constVector);

    // These operations should work with const spans
    EXPECT_EQ(sum(constSpan), 15);
    EXPECT_TRUE(contains(constSpan, 3));

    auto filtered = filterSpan(constSpan, [](int x) { return x % 2 == 0; });
    std::vector<int> expected = {2, 4};
    EXPECT_EQ(filtered, expected);

    size_t count = countIfSpan(constSpan, [](int x) { return x > 3; });
    EXPECT_EQ(count, 2);
}

// Test exception safety
TEST_F(SpanTest, ExceptionSafety) {
    // Test that functions handle edge cases gracefully
    EXPECT_NO_THROW({
        int result = sum(constIntSpan);
        (void)result;
    });

    EXPECT_NO_THROW({
        bool result = contains(constIntSpan, 5);
        (void)result;
    });

    EXPECT_NO_THROW({
        auto result = filterSpan(constIntSpan, [](int x) { return x > 0; });
        (void)result;
    });

    EXPECT_NO_THROW({
        size_t result = countIfSpan(constIntSpan, [](int x) { return x > 0; });
        (void)result;
    });
}

// Test with different numeric types
TEST_F(SpanTest, DifferentNumericTypes) {
    // Test with float
    std::vector<float> floatVector = {1.5f, 2.5f, 3.5f};
    std::span<const float> floatSpan(floatVector);

    EXPECT_FLOAT_EQ(sum(floatSpan), 7.5f);
    EXPECT_TRUE(contains(floatSpan, 2.5f));

    // Test with long
    std::vector<long> longVector = {100L, 200L, 300L};
    std::span<const long> longSpan(longVector);

    EXPECT_EQ(sum(longSpan), 600L);
    EXPECT_TRUE(contains(longSpan, 200L));
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_SPAN_HPP
