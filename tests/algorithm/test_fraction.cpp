#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <sstream>
#include "atom/algorithm/fraction.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;

// Test fixture for fraction tests
class FractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing if needed
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }
};

// Construction Tests
TEST_F(FractionTest, DefaultConstructor) {
    Fraction f;
    EXPECT_EQ(f.toString(), "0/1");
    EXPECT_DOUBLE_EQ(static_cast<double>(f), 0.0);
}

TEST_F(FractionTest, IntValueConstructor) {
    Fraction f(42);
    EXPECT_EQ(f.toString(), "42/1");
    EXPECT_DOUBLE_EQ(static_cast<double>(f), 42.0);
}

TEST_F(FractionTest, FractionConstructor) {
    Fraction f(3, 4);
    EXPECT_EQ(f.toString(), "3/4");
    EXPECT_DOUBLE_EQ(static_cast<double>(f), 0.75);
}

TEST_F(FractionTest, ConstructorReducesFraction) {
    Fraction f(4, 8);
    EXPECT_EQ(f.toString(), "1/2");
}

TEST_F(FractionTest, ConstructorHandlesNegativeValues) {
    Fraction f1(-3, 4);
    EXPECT_EQ(f1.toString(), "-3/4");

    Fraction f2(3, -4);
    EXPECT_EQ(f2.toString(), "-3/4");

    Fraction f3(-3, -4);
    EXPECT_EQ(f3.toString(), "3/4");
}

TEST_F(FractionTest, ConstructorThrowsOnZeroDenominator) {
    EXPECT_THROW((void)Fraction(1, 0), FractionException);
}

// Arithmetic Tests
TEST_F(FractionTest, AdditionSameDenominator) {
    Fraction f1(1, 4);
    Fraction f2(2, 4);
    Fraction result = f1 + f2;
    EXPECT_EQ(result.toString(), "3/4");
}

TEST_F(FractionTest, AdditionDifferentDenominator) {
    Fraction f1(1, 4);
    Fraction f2(1, 3);
    Fraction result = f1 + f2;
    EXPECT_EQ(result.toString(), "7/12");
}

TEST_F(FractionTest, AdditionResultsInWholeNumber) {
    Fraction f1(1, 4);
    Fraction f2(3, 4);
    Fraction result = f1 + f2;
    EXPECT_EQ(result.toString(), "1/1");
}

TEST_F(FractionTest, AdditionWithNegative) {
    Fraction f1(1, 2);
    Fraction f2(-1, 4);
    Fraction result = f1 + f2;
    EXPECT_EQ(result.toString(), "1/4");
}

TEST_F(FractionTest, SubtractionSameDenominator) {
    Fraction f1(3, 4);
    Fraction f2(1, 4);
    Fraction result = f1 - f2;
    EXPECT_EQ(result.toString(), "1/2");
}

TEST_F(FractionTest, SubtractionDifferentDenominator) {
    Fraction f1(3, 4);
    Fraction f2(1, 3);
    Fraction result = f1 - f2;
    EXPECT_EQ(result.toString(), "5/12");
}

TEST_F(FractionTest, SubtractionResultsInNegative) {
    Fraction f1(1, 4);
    Fraction f2(3, 4);
    Fraction result = f1 - f2;
    EXPECT_EQ(result.toString(), "-1/2");
}

TEST_F(FractionTest, MultiplicationBasic) {
    Fraction f1(2, 3);
    Fraction f2(3, 4);
    Fraction result = f1 * f2;
    EXPECT_EQ(result.toString(), "1/2");
}

TEST_F(FractionTest, MultiplicationWithNegative) {
    Fraction f1(2, 3);
    Fraction f2(-3, 4);
    Fraction result = f1 * f2;
    EXPECT_EQ(result.toString(), "-1/2");
}

TEST_F(FractionTest, MultiplicationByZero) {
    Fraction f1(2, 3);
    Fraction f2(0, 5);
    Fraction result = f1 * f2;
    EXPECT_EQ(result.toString(), "0/1");
}

TEST_F(FractionTest, DivisionBasic) {
    Fraction f1(2, 3);
    Fraction f2(3, 4);
    Fraction result = f1 / f2;
    EXPECT_EQ(result.toString(), "8/9");
}

TEST_F(FractionTest, DivisionWithNegative) {
    Fraction f1(2, 3);
    Fraction f2(-3, 4);
    Fraction result = f1 / f2;
    EXPECT_EQ(result.toString(), "-8/9");
}

TEST_F(FractionTest, DivisionThrowsOnZero) {
    Fraction f1(2, 3);
    Fraction f2(0, 1);
    EXPECT_THROW((void)(f1 / f2), FractionException);
}

// Compound Assignment Tests
TEST_F(FractionTest, AdditionAssignment) {
    Fraction f1(1, 4);
    Fraction f2(1, 3);
    f1 += f2;
    EXPECT_EQ(f1.toString(), "7/12");
}

TEST_F(FractionTest, SubtractionAssignment) {
    Fraction f1(3, 4);
    Fraction f2(1, 3);
    f1 -= f2;
    EXPECT_EQ(f1.toString(), "5/12");
}

TEST_F(FractionTest, MultiplicationAssignment) {
    Fraction f1(2, 3);
    Fraction f2(3, 4);
    f1 *= f2;
    EXPECT_EQ(f1.toString(), "1/2");
}

TEST_F(FractionTest, DivisionAssignment) {
    Fraction f1(2, 3);
    Fraction f2(3, 4);
    f1 /= f2;
    EXPECT_EQ(f1.toString(), "8/9");
}

// Comparison Tests
TEST_F(FractionTest, EqualityOperator) {
    Fraction f1(1, 2);
    Fraction f2(2, 4);
    EXPECT_TRUE(f1 == f2);

    Fraction f3(3, 4);
    EXPECT_FALSE(f1 == f3);
}

#if __cplusplus >= 202002L
TEST_F(FractionTest, SpaceshipOperator) {
    Fraction f1(1, 2);
    Fraction f2(1, 3);
    Fraction f3(1, 2);
    Fraction f4(2, 3);

    EXPECT_TRUE(f1 > f2);   // 1/2 > 1/3
    EXPECT_TRUE(f1 >= f2);  // 1/2 >= 1/3
    EXPECT_TRUE(f2 < f1);   // 1/3 < 1/2
    EXPECT_TRUE(f2 <= f1);  // 1/3 <= 1/2
    EXPECT_TRUE(f1 >= f3);  // 1/2 >= 1/2
    EXPECT_TRUE(f1 <= f3);  // 1/2 <= 1/2
    EXPECT_TRUE(f4 > f1);   // 2/3 > 1/2
}
#endif

// Type Conversion Tests
TEST_F(FractionTest, ToDouble) {
    Fraction f(3, 4);
    double d = static_cast<double>(f);
    EXPECT_DOUBLE_EQ(d, 0.75);
    EXPECT_DOUBLE_EQ(f.toDouble(), 0.75);
}

TEST_F(FractionTest, ToFloat) {
    Fraction f(3, 4);
    float fl = static_cast<float>(f);
    EXPECT_FLOAT_EQ(fl, 0.75f);
}

TEST_F(FractionTest, ToInt) {
    Fraction f1(3, 2);
    int i1 = static_cast<int>(f1);
    EXPECT_EQ(i1, 1);  // 3/2 = 1.5, truncated to 1

    Fraction f2(7, 3);
    int i2 = static_cast<int>(f2);
    EXPECT_EQ(i2, 2);  // 7/3 = 2.33..., truncated to 2

    Fraction f3(5, 5);
    int i3 = static_cast<int>(f3);
    EXPECT_EQ(i3, 1);  // 5/5 = 1
}

// Utility Methods Tests
TEST_F(FractionTest, ToString) {
    Fraction f(3, 4);
    EXPECT_EQ(f.toString(), "3/4");

    Fraction f2(-5, 8);
    EXPECT_EQ(f2.toString(), "-5/8");
}

TEST_F(FractionTest, Invert) {
    Fraction f(3, 4);
    f.invert();
    EXPECT_EQ(f.toString(), "4/3");

    Fraction f2(-5, 8);
    f2.invert();
    EXPECT_EQ(f2.toString(), "-8/5");
}

TEST_F(FractionTest, InvertThrowsOnZeroNumerator) {
    Fraction f(0, 1);
    EXPECT_THROW(f.invert(), FractionException);
}

TEST_F(FractionTest, AbsValue) {
    Fraction f1(3, 4);
    Fraction abs1 = f1.abs();
    EXPECT_EQ(abs1.toString(), "3/4");

    Fraction f2(-3, 4);
    Fraction abs2 = f2.abs();
    EXPECT_EQ(abs2.toString(), "3/4");
}

TEST_F(FractionTest, IsZero) {
    Fraction f1(0, 1);
    EXPECT_TRUE(f1.isZero());

    Fraction f2(1, 2);
    EXPECT_FALSE(f2.isZero());
}

TEST_F(FractionTest, IsPositive) {
    Fraction f1(1, 2);
    EXPECT_TRUE(f1.isPositive());

    Fraction f2(-1, 2);
    EXPECT_FALSE(f2.isPositive());

    Fraction f3(0, 1);
    EXPECT_FALSE(f3.isPositive());
}

TEST_F(FractionTest, IsNegative) {
    Fraction f1(-1, 2);
    EXPECT_TRUE(f1.isNegative());

    Fraction f2(1, 2);
    EXPECT_FALSE(f2.isNegative());

    Fraction f3(0, 1);
    EXPECT_FALSE(f3.isNegative());
}

// Stream I/O Tests
TEST_F(FractionTest, OutputStreamOperator) {
    Fraction f(3, 4);
    std::ostringstream oss;
    oss << f;
    EXPECT_EQ(oss.str(), "3/4");
}

TEST_F(FractionTest, InputStreamOperator) {
    Fraction f;
    std::istringstream iss("5/8");
    iss >> f;
    EXPECT_EQ(f.toString(), "5/8");
}

TEST_F(FractionTest, InputStreamOperatorThrowsOnInvalidFormat) {
    Fraction f;
    std::istringstream iss("5:8");
    EXPECT_THROW(iss >> f, FractionException);
}

TEST_F(FractionTest, InputStreamOperatorThrowsOnZeroDenominator) {
    Fraction f;
    std::istringstream iss("5/0");
    EXPECT_THROW(iss >> f, FractionException);
}

// Utility Function Tests
TEST_F(FractionTest, MakeFractionFromInt) {
    Fraction f = makeFraction(42);
    EXPECT_EQ(f.toString(), "42/1");
}

TEST_F(FractionTest, MakeFractionFromDouble) {
    Fraction f1 = makeFraction(0.5, 100);
    EXPECT_EQ(f1.toString(), "1/2");

    Fraction f2 = makeFraction(0.333333, 100);
    EXPECT_EQ(f2.toString(), "1/3");

    Fraction f3 = makeFraction(0.25, 100);
    EXPECT_EQ(f3.toString(), "1/4");

    Fraction f4 = makeFraction(3.14159, 1000);
    EXPECT_EQ(f4.toString(), "355/113");  // Approximation of π

    Fraction f5 = makeFraction(-0.5, 100);
    EXPECT_EQ(f5.toString(), "-1/2");
}

TEST_F(FractionTest, MakeFractionThrowsOnNanInf) {
    EXPECT_THROW((void)makeFraction(std::nan("")), FractionException);
    EXPECT_THROW((void)makeFraction(std::numeric_limits<double>::infinity()),
                 FractionException);
}

// Edge Cases Tests
TEST_F(FractionTest, LargeNumbersAddition) {
    Fraction f1(std::numeric_limits<int>::max() / 2, 1);
    Fraction f2(std::numeric_limits<int>::max() / 2 + 1, 1);
    EXPECT_THROW((void)(f1 + f2), FractionException);
}

TEST_F(FractionTest, LargeNumbersSubtraction) {
    Fraction f1(std::numeric_limits<int>::min() / 2, 1);
    Fraction f2(std::numeric_limits<int>::max() / 2, 1);
    EXPECT_THROW((void)(f1 - f2), FractionException);
}

TEST_F(FractionTest, LargeNumbersMultiplication) {
    Fraction f1(46340, 1);  // sqrt(INT_MAX) ~ 46340
    Fraction f2(46341, 1);
    EXPECT_THROW((void)(f1 * f2), FractionException);
}

TEST_F(FractionTest, LargeNumbersDivision) {
    int large_num = std::numeric_limits<int>::max();
    Fraction f1(large_num, 1);
    Fraction f2(1, large_num);
    EXPECT_THROW((void)(f1 / f2), FractionException);
}

TEST_F(FractionTest, MinIntHandling) {
    int min_int = std::numeric_limits<int>::min();
    Fraction f1(min_int, 1);
    Fraction f2(1, 2);
    Fraction result = f1 * f2;
    EXPECT_TRUE(result.isNegative());
}

// Reduction Tests
TEST_F(FractionTest, ProperReductionWithGCD) {
    Fraction f1(15, 25);
    EXPECT_EQ(f1.toString(), "3/5");

    Fraction f2(48, 180);
    EXPECT_EQ(f2.toString(), "4/15");

    Fraction f3(0, 5);
    EXPECT_EQ(f3.toString(), "0/1");
}

TEST_F(FractionTest, ReductionWithNegativeValues) {
    Fraction f1(15, -25);
    EXPECT_EQ(f1.toString(), "-3/5");

    Fraction f2(-48, -180);
    EXPECT_EQ(f2.toString(), "4/15");
}

// Multiple Operations Tests
TEST_F(FractionTest, ComplexSequenceOfOperations) {
    // Test a complex sequence: (1/2 + 1/3) * (3/4 - 1/6) / (5/8)
    Fraction a(1, 2);
    Fraction b(1, 3);
    Fraction c(3, 4);
    Fraction d(1, 6);
    Fraction e(5, 8);

    Fraction result = ((a + b) * (c - d)) / e;
    // (1/2 + 1/3) = 5/6
    // (3/4 - 1/6) = 7/12
    // 5/6 * 7/12 = 35/72
    // 35/72 / (5/8) = 35/72 * 8/5 = 280/360 = 7/9
    EXPECT_EQ(result.toString(), "7/9");
}

TEST_F(FractionTest, ChainedOperations) {
    // Test chained operations: 1/2 + 1/3 - 1/4 * 1/5
    Fraction a(1, 2);
    Fraction b(1, 3);
    Fraction c(1, 4);
    Fraction d(1, 5);

    // Note: without parentheses, operations are performed in left-to-right
    // order after applying operator precedence rules.
    Fraction result = a + b - c * d;
    // 1/4 * 1/5 = 1/20
    // 1/2 + 1/3 = 5/6
    // 5/6 - 1/20 = 100/120 - 6/120 = 94/120 = 47/60
    EXPECT_EQ(result.toString(), "47/60");
}