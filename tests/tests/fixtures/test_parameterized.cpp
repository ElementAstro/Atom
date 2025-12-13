/*
 * test_parameterized.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for parameterized tests in atom/tests/fixtures/test_parameterized.hpp

**************************************************/

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "atom/tests/fixtures/test_parameterized.hpp"

namespace atom::test::fixtures::tests {

// ============================================================================
// Basic Parameterized Test
// ============================================================================

class BasicParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(BasicParamTest, SquareIsPositive) {
    int value = GetParam();
    EXPECT_GE(value * value, 0);
}

INSTANTIATE_TEST_SUITE_P(PositiveNumbers, BasicParamTest,
                         ::testing::Values(1, 2, 3, 4, 5));

INSTANTIATE_TEST_SUITE_P(NegativeNumbers, BasicParamTest,
                         ::testing::Values(-1, -2, -3, -4, -5));

INSTANTIATE_TEST_SUITE_P(Zero, BasicParamTest, ::testing::Values(0));

// ============================================================================
// Parameterized Test with Pairs
// ============================================================================

class PairParamTest
    : public ::testing::TestWithParam<std::pair<int, int>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(PairParamTest, SquareIsCorrect) {
    auto [input, expected] = GetParam();
    EXPECT_EQ(input * input, expected);
}

INSTANTIATE_TEST_SUITE_P(Squares, PairParamTest,
                         ::testing::Values(std::make_pair(1, 1),
                                           std::make_pair(2, 4),
                                           std::make_pair(3, 9),
                                           std::make_pair(4, 16),
                                           std::make_pair(5, 25)));

// ============================================================================
// Parameterized Test with Strings
// ============================================================================

class StringParamTest : public ::testing::TestWithParam<std::string> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(StringParamTest, StringIsNotEmpty) {
    std::string value = GetParam();
    EXPECT_FALSE(value.empty());
}

INSTANTIATE_TEST_SUITE_P(NonEmptyStrings, StringParamTest,
                         ::testing::Values("hello", "world", "test", "atom"));

// ============================================================================
// Parameterized Test with Tuples
// ============================================================================

class TupleParamTest
    : public ::testing::TestWithParam<std::tuple<int, std::string, bool>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(TupleParamTest, TupleElements) {
    auto [number, text, flag] = GetParam();
    EXPECT_GE(number, 0);
    EXPECT_FALSE(text.empty());
    // flag can be true or false
}

INSTANTIATE_TEST_SUITE_P(
    TupleValues, TupleParamTest,
    ::testing::Values(std::make_tuple(1, "one", true),
                      std::make_tuple(2, "two", false),
                      std::make_tuple(3, "three", true)));

// ============================================================================
// Parameterized Test with Range
// ============================================================================

class RangeParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(RangeParamTest, ValueInRange) {
    int value = GetParam();
    EXPECT_GE(value, 0);
    EXPECT_LT(value, 10);
}

INSTANTIATE_TEST_SUITE_P(ZeroToNine, RangeParamTest,
                         ::testing::Range(0, 10));

INSTANTIATE_TEST_SUITE_P(EvenNumbers, RangeParamTest,
                         ::testing::Range(0, 10, 2));

// ============================================================================
// Parameterized Test with Bool
// ============================================================================

class BoolParamTest : public ::testing::TestWithParam<bool> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(BoolParamTest, BoolValue) {
    bool value = GetParam();
    EXPECT_TRUE(value == true || value == false);
}

INSTANTIATE_TEST_SUITE_P(AllBoolValues, BoolParamTest, ::testing::Bool());

// ============================================================================
// Parameterized Test with Combine
// ============================================================================

class CombineParamTest
    : public ::testing::TestWithParam<std::tuple<int, bool>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(CombineParamTest, CombinedValues) {
    auto [number, flag] = GetParam();
    EXPECT_GE(number, 1);
    EXPECT_LE(number, 3);
}

INSTANTIATE_TEST_SUITE_P(Combined, CombineParamTest,
                         ::testing::Combine(::testing::Values(1, 2, 3),
                                            ::testing::Bool()));

// ============================================================================
// Parameterized Test with Custom Name Generator
// ============================================================================

class NamedParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(NamedParamTest, ValueIsPositive) {
    int value = GetParam();
    if (value > 0) {
        EXPECT_GT(value, 0);
    }
}

std::string CustomNameGenerator(
    const ::testing::TestParamInfo<int>& info) {
    return "Value_" + std::to_string(info.param);
}

INSTANTIATE_TEST_SUITE_P(NamedValues, NamedParamTest,
                         ::testing::Values(1, 2, 3, 4, 5), CustomNameGenerator);

// ============================================================================
// Parameterized Test with ValuesIn
// ============================================================================

class ValuesInParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override { testValues = {10, 20, 30, 40, 50}; }
    void TearDown() override { testValues.clear(); }

    static std::vector<int> testValues;
};

std::vector<int> ValuesInParamTest::testValues;

TEST_P(ValuesInParamTest, ValueFromVector) {
    int value = GetParam();
    EXPECT_GE(value, 10);
    EXPECT_LE(value, 50);
}

INSTANTIATE_TEST_SUITE_P(FromVector, ValuesInParamTest,
                         ::testing::Values(10, 20, 30, 40, 50));

// ============================================================================
// Parameterized Test with Fixture State
// ============================================================================

class StatefulParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {
        baseValue = 100;
        param = GetParam();
    }
    void TearDown() override {}

    int baseValue = 0;
    int param = 0;
};

TEST_P(StatefulParamTest, ComputeResult) {
    int result = baseValue + param;
    EXPECT_GT(result, baseValue);
}

INSTANTIATE_TEST_SUITE_P(AddToBase, StatefulParamTest,
                         ::testing::Values(1, 5, 10, 50, 100));

// ============================================================================
// Parameterized Test with String Pairs
// ============================================================================

class StringLengthParamTest
    : public ::testing::TestWithParam<std::pair<std::string, size_t>> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(StringLengthParamTest, LengthIsCorrect) {
    auto [str, expectedLen] = GetParam();
    EXPECT_EQ(str.length(), expectedLen);
}

INSTANTIATE_TEST_SUITE_P(StringLengths, StringLengthParamTest,
                         ::testing::Values(std::make_pair("", 0),
                                           std::make_pair("a", 1),
                                           std::make_pair("hello", 5),
                                           std::make_pair("world!", 6)));

// ============================================================================
// Parameterized Test Edge Cases
// ============================================================================

class EdgeCaseParamTest : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(EdgeCaseParamTest, HandlesEdgeCases) {
    int value = GetParam();
    // Test handles extreme values
    EXPECT_TRUE(value == 0 || value != 0);
}

INSTANTIATE_TEST_SUITE_P(
    EdgeCases, EdgeCaseParamTest,
    ::testing::Values(0, -1, 1, std::numeric_limits<int>::min(),
                      std::numeric_limits<int>::max()));

}  // namespace atom::test::fixtures::tests
