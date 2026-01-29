/**
 * @file test_parameterized.cpp
 * @brief Test parameterized test functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <cmath>
#include <string>

using namespace atom::test;

// ============================================================================
// Integer Parameterized Tests
// ============================================================================

class IntegerTest : public ParameterizedTest<int> {};

INSTANTIATE_TEST_SUITE_P(Integers, IntegerTest, Values({1, 2, 3, 4, 5}));

TEST_P(IntegerTest, IsPositive) { expect_gt(GetParam(), 0); }

TEST_P(IntegerTest, SquareIsPositive) {
    int value = GetParam();
    expect_gt(value * value, 0);
}

// ============================================================================
// Range Parameterized Tests
// ============================================================================

class RangeTest : public ParameterizedTest<int> {};

INSTANTIATE_TEST_SUITE_P(Range0To10, RangeTest, Range(0, 10, 2));

TEST_P(RangeTest, IsEven) { expect_eq(GetParam() % 2, 0); }

// ============================================================================
// Boolean Parameterized Tests
// ============================================================================

class BoolTest : public ParameterizedTest<bool> {};

INSTANTIATE_TEST_SUITE_P(Booleans, BoolTest, Bool());

TEST_P(BoolTest, DoubleNegationIsIdentity) {
    bool value = GetParam();
    expect_eq(!!value, value);
}

// ============================================================================
// String Parameterized Tests
// ============================================================================

class StringTest : public ParameterizedTest<std::string> {};

INSTANTIATE_TEST_SUITE_P(Strings, StringTest,
                         Values<std::string>({"hello", "world", "test",
                                              "atom"}));

TEST_P(StringTest, IsNotEmpty) { expect_not_empty(GetParam()); }

TEST_P(StringTest, HasPositiveLength) { expect_gt(GetParam().length(), 0); }

// ============================================================================
// Floating Point Parameterized Tests
// ============================================================================

class FloatTest : public ParameterizedTest<double> {};

INSTANTIATE_TEST_SUITE_P(Floats, FloatTest,
                         Values<double>({0.1, 0.5, 1.0, 2.5, 10.0}));

TEST_P(FloatTest, SquareRootIsPositive) {
    double value = GetParam();
    expect_gt(std::sqrt(value), 0.0);
}

TEST_P(FloatTest, LogIsFinite) {
    double value = GetParam();
    double logValue = std::log(value);
    expect_true(std::isfinite(logValue));
}

// ============================================================================
// Struct Parameterized Tests
// ============================================================================

struct TestData {
    int input;
    int expected;
    std::string name;
};

class StructTest : public ParameterizedTest<TestData> {};

INSTANTIATE_TEST_SUITE_P(
    StructData, StructTest,
    Values<TestData>({{1, 1, "one"}, {2, 4, "two"}, {3, 9, "three"}}));

TEST_P(StructTest, SquareIsCorrect) {
    const auto& data = GetParam();
    expect_eq(data.input * data.input, data.expected);
}

TEST_P(StructTest, NameIsNotEmpty) {
    const auto& data = GetParam();
    expect_not_empty(data.name);
}

// ============================================================================
// Pair Parameterized Tests
// ============================================================================

class PairTest : public ParameterizedTest<std::pair<int, int>> {};

INSTANTIATE_TEST_SUITE_P(
    Pairs, PairTest,
    Values<std::pair<int, int>>({{1, 2}, {3, 4}, {5, 6}, {7, 8}}));

TEST_P(PairTest, SecondIsGreaterThanFirst) {
    const auto& pair = GetParam();
    expect_gt(pair.second, pair.first);
}

TEST_P(PairTest, SumIsPositive) {
    const auto& pair = GetParam();
    expect_gt(pair.first + pair.second, 0);
}

// ============================================================================
// Named Parameters Tests
// ============================================================================

class NamedParamTest : public ParameterizedTest<int> {};

INSTANTIATE_TEST_SUITE_P(
    NamedParams, NamedParamTest,
    ValuesIn<int>({{"zero", 0}, {"one", 1}, {"two", 2}, {"ten", 10}}));

TEST_P(NamedParamTest, IsNonNegative) { expect_ge(GetParam(), 0); }

// ============================================================================
// Fibonacci Test
// ============================================================================

class FibonacciTest : public ParameterizedTest<std::pair<int, int>> {};

INSTANTIATE_TEST_SUITE_P(
    Fibonacci, FibonacciTest,
    Values<std::pair<int, int>>(
        {{0, 0}, {1, 1}, {2, 1}, {3, 2}, {4, 3}, {5, 5}, {6, 8}, {7, 13}}));

int fibonacci(int n) {
    if (n <= 1)
        return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; ++i) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

TEST_P(FibonacciTest, ComputesCorrectly) {
    const auto& [n, expected] = GetParam();
    expect_eq(fibonacci(n), expected);
}

// ============================================================================
// Prime Number Test
// ============================================================================

class PrimeTest : public ParameterizedTest<std::pair<int, bool>> {};

bool isPrime(int n) {
    if (n < 2)
        return false;
    for (int i = 2; i * i <= n; ++i) {
        if (n % i == 0)
            return false;
    }
    return true;
}

INSTANTIATE_TEST_SUITE_P(Primes, PrimeTest,
                         Values<std::pair<int, bool>>({{2, true},
                                                       {3, true},
                                                       {4, false},
                                                       {5, true},
                                                       {6, false},
                                                       {7, true},
                                                       {8, false},
                                                       {9, false},
                                                       {10, false},
                                                       {11, true}}));

TEST_P(PrimeTest, IsPrimeCorrect) {
    const auto& [n, expected] = GetParam();
    expect_eq(isPrime(n), expected);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) { return runAllTests(argc, argv); }
