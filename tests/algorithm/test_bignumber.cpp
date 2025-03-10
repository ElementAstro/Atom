#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <random>
#include <string>

#include "atom/algorithm/bignumber.hpp"
#include "atom/macro.hpp"

using namespace atom::algorithm;

class BigNumberTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test values
        zero = BigNumber("0");
        one = BigNumber("1");
        minusOne = BigNumber("-1");
        ten = BigNumber("10");
        hundred = BigNumber("100");
        large1 = BigNumber("12345678901234567890");
        large2 = BigNumber("98765432109876543210");
        negative = BigNumber("-42");
        positive = BigNumber("42");

        // Test values for edge cases
        maxInt = BigNumber(std::to_string(std::numeric_limits<int>::max()));
        minInt = BigNumber(std::to_string(std::numeric_limits<int>::min()));
    }

    // Helper method to generate a random big number
    BigNumber generateRandomBigNumber(size_t digits,
                                      bool allowNegative = true) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> digitDist(0, 9);
        std::uniform_int_distribution<> signDist(0, 1);

        std::string number;
        if (allowNegative && signDist(gen)) {
            number += "-";
        }

        // First digit shouldn't be zero
        number += std::to_string(1 + digitDist(gen) % 9);

        for (size_t i = 1; i < digits; i++) {
            number += std::to_string(digitDist(gen));
        }

        return BigNumber(number);
    }

    BigNumber two();

    BigNumber zero;
    BigNumber one;
    BigNumber minusOne;
    BigNumber ten;
    BigNumber hundred;
    BigNumber large1;
    BigNumber large2;
    BigNumber negative;
    BigNumber positive;
    BigNumber maxInt;
    BigNumber minInt;
};

// Constructor tests
TEST_F(BigNumberTest, DefaultConstructor) {
    BigNumber num;
    EXPECT_EQ(num.toString(), "0");
    EXPECT_FALSE(num.isNegative());
}

TEST_F(BigNumberTest, StringConstructor) {
    BigNumber num1("12345");
    EXPECT_EQ(num1.toString(), "12345");
    EXPECT_FALSE(num1.isNegative());

    BigNumber num2("-54321");
    EXPECT_EQ(num2.toString(), "-54321");
    EXPECT_TRUE(num2.isNegative());

    BigNumber num3("0");
    EXPECT_EQ(num3.toString(), "0");
    EXPECT_FALSE(num3.isNegative());

    BigNumber num4("-0");
    EXPECT_EQ(num4.toString(), "0");
    EXPECT_FALSE(num4.isNegative());  // -0 should normalize to 0
}

TEST_F(BigNumberTest, IntegerConstructor) {
    BigNumber num1(12345);
    EXPECT_EQ(num1.toString(), "12345");
    EXPECT_FALSE(num1.isNegative());

    BigNumber num2(-54321);
    EXPECT_EQ(num2.toString(), "-54321");
    EXPECT_TRUE(num2.isNegative());

    BigNumber num3(0);
    EXPECT_EQ(num3.toString(), "0");
    EXPECT_FALSE(num3.isNegative());
}

TEST_F(BigNumberTest, ConstructorInvalidInputs) {
    EXPECT_THROW(BigNumber(""), std::invalid_argument);
    EXPECT_THROW(BigNumber("-"), std::invalid_argument);
    EXPECT_THROW(BigNumber("123a456"), std::invalid_argument);
    EXPECT_THROW(BigNumber("12.34"), std::invalid_argument);
    EXPECT_THROW(BigNumber("-12a"), std::invalid_argument);
}

// String trimming tests
TEST_F(BigNumberTest, LeadingZeros) {
    BigNumber num1("00123");
    EXPECT_EQ(num1.toString(), "123");

    BigNumber num2("-00123");
    EXPECT_EQ(num2.toString(), "-123");

    BigNumber num3("000");
    EXPECT_EQ(num3.toString(), "0");

    BigNumber num4("-000");
    EXPECT_EQ(num4.toString(), "0");
}

TEST_F(BigNumberTest, TrimLeadingZeros) {
    BigNumber original("00123");
    BigNumber trimmed = original.trimLeadingZeros();
    EXPECT_EQ(trimmed.toString(), "123");

    original = BigNumber("-00123");
    trimmed = original.trimLeadingZeros();
    EXPECT_EQ(trimmed.toString(), "-123");

    original = BigNumber("000");
    trimmed = original.trimLeadingZeros();
    EXPECT_EQ(trimmed.toString(), "0");
}

// String manipulation tests
TEST_F(BigNumberTest, ToString) {
    EXPECT_EQ(zero.toString(), "0");
    EXPECT_EQ(one.toString(), "1");
    EXPECT_EQ(minusOne.toString(), "-1");
    EXPECT_EQ(large1.toString(), "12345678901234567890");
    EXPECT_EQ(negative.toString(), "-42");
}

TEST_F(BigNumberTest, SetString) {
    BigNumber num("123");
    EXPECT_EQ(num.toString(), "123");

    num.setString("456");
    EXPECT_EQ(num.toString(), "456");

    num.setString("-789");
    EXPECT_EQ(num.toString(), "-789");

    num.setString("0");
    EXPECT_EQ(num.toString(), "0");

    EXPECT_THROW(num.setString(""), std::invalid_argument);
    EXPECT_THROW(num.setString("abc"), std::invalid_argument);
}

// Equality tests
TEST_F(BigNumberTest, Equals) {
    EXPECT_TRUE(zero.equals(zero));
    EXPECT_TRUE(one.equals(one));
    EXPECT_TRUE(large1.equals(large1));

    EXPECT_FALSE(one.equals(zero));
    EXPECT_FALSE(zero.equals(one));
    EXPECT_FALSE(large1.equals(large2));

    // Test with integers
    EXPECT_TRUE(zero.equals(0));
    EXPECT_TRUE(one.equals(1));
    EXPECT_TRUE(negative.equals(-42));

    // Test with strings
    EXPECT_TRUE(zero.equals("0"));
    EXPECT_TRUE(one.equals("1"));
    EXPECT_TRUE(negative.equals("-42"));
}

TEST_F(BigNumberTest, EqualsOperator) {
    EXPECT_TRUE(zero == zero);
    EXPECT_TRUE(one == one);
    EXPECT_TRUE(large1 == large1);

    EXPECT_FALSE(one == zero);
    EXPECT_FALSE(zero == one);
    EXPECT_FALSE(large1 == large2);

    BigNumber sameAsOne("1");
    EXPECT_TRUE(one == sameAsOne);

    BigNumber sameAsLarge1("12345678901234567890");
    EXPECT_TRUE(large1 == sameAsLarge1);
}

// Comparison tests
TEST_F(BigNumberTest, GreaterThan) {
    EXPECT_TRUE(one > zero);
    EXPECT_TRUE(ten > one);
    EXPECT_TRUE(hundred > ten);
    EXPECT_TRUE(large2 > large1);
    EXPECT_TRUE(zero > minusOne);
    EXPECT_TRUE(positive > negative);

    EXPECT_FALSE(zero > one);
    EXPECT_FALSE(one > ten);
    EXPECT_FALSE(minusOne > zero);
    EXPECT_FALSE(negative > positive);

    // Same number should not be greater than itself
    EXPECT_FALSE(zero > zero);
    EXPECT_FALSE(one > one);

    // Negative number comparisons
    EXPECT_TRUE(minusOne > BigNumber("-2"));
    EXPECT_FALSE(minusOne > zero);
}

TEST_F(BigNumberTest, LessThan) {
    EXPECT_TRUE(zero < one);
    EXPECT_TRUE(one < ten);
    EXPECT_TRUE(ten < hundred);
    EXPECT_TRUE(large1 < large2);
    EXPECT_TRUE(minusOne < zero);
    EXPECT_TRUE(negative < positive);

    EXPECT_FALSE(one < zero);
    EXPECT_FALSE(ten < one);
    EXPECT_FALSE(zero < minusOne);
    EXPECT_FALSE(positive < negative);

    // Same number should not be less than itself
    EXPECT_FALSE(zero < zero);
    EXPECT_FALSE(one < one);

    // Negative number comparisons
    EXPECT_TRUE(BigNumber("-2") < minusOne);
    EXPECT_TRUE(minusOne < zero);
}

TEST_F(BigNumberTest, GreaterThanOrEqual) {
    EXPECT_TRUE(one >= zero);
    EXPECT_TRUE(ten >= one);

    EXPECT_TRUE(zero >= zero);  // Equal case
    EXPECT_TRUE(one >= one);    // Equal case

    EXPECT_FALSE(zero >= one);
    EXPECT_FALSE(minusOne >= zero);

    // Negative number comparisons
    EXPECT_TRUE(minusOne >= BigNumber("-2"));
    EXPECT_TRUE(minusOne >= minusOne);  // Equal case
    EXPECT_FALSE(minusOne >= zero);
}

TEST_F(BigNumberTest, LessThanOrEqual) {
    EXPECT_TRUE(zero <= one);
    EXPECT_TRUE(one <= ten);

    EXPECT_TRUE(zero <= zero);  // Equal case
    EXPECT_TRUE(one <= one);    // Equal case

    EXPECT_FALSE(one <= zero);
    EXPECT_FALSE(zero <= minusOne);

    // Negative number comparisons
    EXPECT_TRUE(BigNumber("-2") <= minusOne);
    EXPECT_TRUE(minusOne <= minusOne);  // Equal case
    EXPECT_TRUE(minusOne <= zero);
}

// Addition tests
TEST_F(BigNumberTest, Add) {
    EXPECT_EQ(zero.add(zero).toString(), "0");
    EXPECT_EQ(zero.add(one).toString(), "1");
    EXPECT_EQ(one.add(zero).toString(), "1");
    EXPECT_EQ(one.add(one).toString(), "2");
    EXPECT_EQ(one.add(minusOne).toString(), "0");

    // Test large number addition
    EXPECT_EQ(large1.add(large2).toString(), "111111111011111111100");

    // Test adding negative numbers
    EXPECT_EQ(negative.add(positive).toString(), "0");
    EXPECT_EQ(negative.add(negative).toString(), "-84");
    EXPECT_EQ(positive.add(positive).toString(), "84");
    EXPECT_EQ(positive.add(negative).toString(), "0");

    // Test carrying
    BigNumber num1("999");
    BigNumber num2("1");
    EXPECT_EQ(num1.add(num2).toString(), "1000");

    BigNumber num3("999999999999999999999");
    BigNumber num4("1");
    EXPECT_EQ(num3.add(num4).toString(), "1000000000000000000000");
}

TEST_F(BigNumberTest, AdditionOperator) {
    EXPECT_EQ((zero + zero).toString(), "0");
    EXPECT_EQ((zero + one).toString(), "1");
    EXPECT_EQ((one + zero).toString(), "1");
    EXPECT_EQ((one + one).toString(), "2");
    EXPECT_EQ((one + minusOne).toString(), "0");

    // Test large number addition
    EXPECT_EQ((large1 + large2).toString(), "111111111011111111100");

    // Test adding negative numbers
    EXPECT_EQ((negative + positive).toString(), "0");
    EXPECT_EQ((negative + negative).toString(), "-84");
    EXPECT_EQ((positive + positive).toString(), "84");
    EXPECT_EQ((positive + negative).toString(), "0");
}

TEST_F(BigNumberTest, AddAssignmentOperator) {
    BigNumber num1("123");
    num1 += BigNumber("456");
    EXPECT_EQ(num1.toString(), "579");

    BigNumber num2("999");
    num2 += BigNumber("1");
    EXPECT_EQ(num2.toString(), "1000");

    BigNumber num3("100");
    num3 += BigNumber("-50");
    EXPECT_EQ(num3.toString(), "50");

    BigNumber num4("-100");
    num4 += BigNumber("50");
    EXPECT_EQ(num4.toString(), "-50");

    BigNumber num5("-100");
    num5 += BigNumber("-50");
    EXPECT_EQ(num5.toString(), "-150");
}

// Subtraction tests
TEST_F(BigNumberTest, Subtract) {
    EXPECT_EQ(zero.subtract(zero).toString(), "0");
    EXPECT_EQ(one.subtract(one).toString(), "0");
    EXPECT_EQ(one.subtract(zero).toString(), "1");
    EXPECT_EQ(zero.subtract(one).toString(), "-1");

    // Test large number subtraction
    EXPECT_EQ(large2.subtract(large1).toString(), "86419753208641975320");
    EXPECT_EQ(large1.subtract(large2).toString(), "-86419753208641975320");

    // Test subtracting negative numbers
    EXPECT_EQ(positive.subtract(negative).toString(), "84");
    EXPECT_EQ(negative.subtract(positive).toString(), "-84");

    // Test borrowing
    BigNumber num1("1000");
    BigNumber num2("1");
    EXPECT_EQ(num1.subtract(num2).toString(), "999");

    BigNumber num3("1000000000000000000000");
    BigNumber num4("1");
    EXPECT_EQ(num3.subtract(num4).toString(), "999999999999999999999");
}

TEST_F(BigNumberTest, SubtractionOperator) {
    EXPECT_EQ((zero - zero).toString(), "0");
    EXPECT_EQ((one - one).toString(), "0");
    EXPECT_EQ((one - zero).toString(), "1");
    EXPECT_EQ((zero - one).toString(), "-1");

    // Test large number subtraction
    EXPECT_EQ((large2 - large1).toString(), "86419753208641975320");
    EXPECT_EQ((large1 - large2).toString(), "-86419753208641975320");

    // Test subtracting negative numbers
    EXPECT_EQ((positive - negative).toString(), "84");
    EXPECT_EQ((negative - positive).toString(), "-84");
}

TEST_F(BigNumberTest, SubtractAssignmentOperator) {
    BigNumber num1("579");
    num1 -= BigNumber("456");
    EXPECT_EQ(num1.toString(), "123");

    BigNumber num2("1000");
    num2 -= BigNumber("1");
    EXPECT_EQ(num2.toString(), "999");

    BigNumber num3("50");
    num3 -= BigNumber("100");
    EXPECT_EQ(num3.toString(), "-50");

    BigNumber num4("-50");
    num4 -= BigNumber("50");
    EXPECT_EQ(num4.toString(), "-100");

    BigNumber num5("-100");
    num5 -= BigNumber("-150");
    EXPECT_EQ(num5.toString(), "50");
}

// Multiplication tests
TEST_F(BigNumberTest, Multiply) {
    EXPECT_EQ(zero.multiply(zero).toString(), "0");
    EXPECT_EQ(zero.multiply(one).toString(), "0");
    EXPECT_EQ(one.multiply(zero).toString(), "0");
    EXPECT_EQ(one.multiply(one).toString(), "1");
    EXPECT_EQ(ten.multiply(ten).toString(), "100");

    // Test large number multiplication
    BigNumber num1("12345");
    BigNumber num2("67890");
    EXPECT_EQ(num1.multiply(num2).toString(), "838102050");

    // Test sign handling in multiplication
    EXPECT_EQ(positive.multiply(negative).toString(), "-1764");
    EXPECT_EQ(negative.multiply(positive).toString(), "-1764");
    EXPECT_EQ(negative.multiply(negative).toString(), "1764");
    EXPECT_EQ(positive.multiply(positive).toString(), "1764");

    // Test very large multiplication (should use Karatsuba)
    BigNumber largeA(std::string(200, '9'));  // 200 nines
    BigNumber largeB(std::string(200, '9'));  // 200 nines
    std::string expected =
        "9" + std::string(399, '8') + "1";  // 9 followed by 399 eights and a 1
    EXPECT_EQ(largeA.multiply(largeB).toString(), expected);
}

TEST_F(BigNumberTest, MultiplicationOperator) {
    EXPECT_EQ((zero * zero).toString(), "0");
    EXPECT_EQ((zero * one).toString(), "0");
    EXPECT_EQ((one * zero).toString(), "0");
    EXPECT_EQ((one * one).toString(), "1");
    EXPECT_EQ((ten * ten).toString(), "100");

    // Test large number multiplication
    BigNumber num1("12345");
    BigNumber num2("67890");
    EXPECT_EQ((num1 * num2).toString(), "838102050");

    // Test sign handling in multiplication
    EXPECT_EQ((positive * negative).toString(), "-1764");
    EXPECT_EQ((negative * positive).toString(), "-1764");
    EXPECT_EQ((negative * negative).toString(), "1764");
    EXPECT_EQ((positive * positive).toString(), "1764");
}

TEST_F(BigNumberTest, MultiplyAssignmentOperator) {
    BigNumber num1("123");
    num1 *= BigNumber("2");
    EXPECT_EQ(num1.toString(), "246");

    BigNumber num2("100");
    num2 *= BigNumber("0");
    EXPECT_EQ(num2.toString(), "0");

    BigNumber num3("50");
    num3 *= BigNumber("-2");
    EXPECT_EQ(num3.toString(), "-100");

    BigNumber num4("-5");
    num4 *= BigNumber("-10");
    EXPECT_EQ(num4.toString(), "50");
}

// Division tests
TEST_F(BigNumberTest, Divide) {
    EXPECT_EQ(zero.divide(one).toString(), "0");
    EXPECT_EQ(one.divide(one).toString(), "1");
    EXPECT_EQ(ten.divide(one).toString(), "10");
    EXPECT_EQ(ten.divide(two()).toString(), "5");
    EXPECT_EQ(hundred.divide(ten).toString(), "10");

    // Test division with remainders
    BigNumber num1("100");
    BigNumber num2("3");
    EXPECT_EQ(num1.divide(num2).toString(), "33");  // Integer division

    // Test sign handling in division
    EXPECT_EQ(positive.divide(negative).toString(), "-1");
    EXPECT_EQ(negative.divide(positive).toString(), "-1");
    EXPECT_EQ(negative.divide(negative).toString(), "1");

    // Test division by zero
    EXPECT_THROW(ATOM_UNUSED_RESULT(one.divide(zero)),
                 std::invalid_argument);  // Removed [[maybe_unused]]
}

TEST_F(BigNumberTest, DivisionOperator) {
    EXPECT_EQ((zero / one).toString(), "0");
    EXPECT_EQ((one / one).toString(), "1");
    EXPECT_EQ((ten / one).toString(), "10");
    EXPECT_EQ((ten / two()).toString(), "5");
    EXPECT_EQ((hundred / ten).toString(), "10");

    // Test division with remainders
    BigNumber num1("100");
    BigNumber num2("3");
    EXPECT_EQ((num1 / num2).toString(), "33");  // Integer division

    // Test sign handling in division
    EXPECT_EQ((positive / negative).toString(), "-1");
    EXPECT_EQ((negative / positive).toString(), "-1");
    EXPECT_EQ((negative / negative).toString(), "1");

    // Test division by zero
    EXPECT_THROW(one / zero, std::invalid_argument);
}

TEST_F(BigNumberTest, DivideAssignmentOperator) {
    BigNumber num1("246");
    num1 /= BigNumber("2");
    EXPECT_EQ(num1.toString(), "123");

    BigNumber num2("100");
    num2 /= BigNumber("3");
    EXPECT_EQ(num2.toString(), "33");  // Integer division

    BigNumber num3("100");
    num3 /= BigNumber("-2");
    EXPECT_EQ(num3.toString(), "-50");

    BigNumber num4("-100");
    num4 /= BigNumber("-2");
    EXPECT_EQ(num4.toString(), "50");

    // Test division by zero
    BigNumber num5("100");
    EXPECT_THROW(num5 /= BigNumber("0"), std::invalid_argument);
}

// Power tests
TEST_F(BigNumberTest, Pow) {
    EXPECT_EQ(zero.pow(0).toString(), "1");  // 0^0 = 1 by convention
    EXPECT_EQ(zero.pow(1).toString(), "0");
    EXPECT_EQ(zero.pow(10).toString(), "0");

    EXPECT_EQ(one.pow(0).toString(), "1");
    EXPECT_EQ(one.pow(1).toString(), "1");
    EXPECT_EQ(one.pow(10).toString(), "1");

    EXPECT_EQ(two().pow(0).toString(), "1");
    EXPECT_EQ(two().pow(1).toString(), "2");
    EXPECT_EQ(two().pow(3).toString(), "8");
    EXPECT_EQ(two().pow(10).toString(), "1024");

    EXPECT_EQ(ten.pow(0).toString(), "1");
    EXPECT_EQ(ten.pow(1).toString(), "10");
    EXPECT_EQ(ten.pow(3).toString(), "1000");

    // Test negative base
    EXPECT_EQ(minusOne.pow(0).toString(), "1");
    EXPECT_EQ(minusOne.pow(1).toString(), "-1");
    EXPECT_EQ(minusOne.pow(2).toString(), "1");
    EXPECT_EQ(minusOne.pow(3).toString(), "-1");

    // Test negative exponent (should throw)
    EXPECT_THROW(ATOM_UNUSED_RESULT(one.pow(-1)), std::invalid_argument);
}

TEST_F(BigNumberTest, PowerOperator) {
    EXPECT_EQ((zero ^ 0).toString(), "1");  // 0^0 = 1 by convention
    EXPECT_EQ((zero ^ 1).toString(), "0");
    EXPECT_EQ((one ^ 0).toString(), "1");
    EXPECT_EQ((one ^ 10).toString(), "1");
    EXPECT_EQ((two() ^ 3).toString(), "8");
    EXPECT_EQ((ten ^ 3).toString(), "1000");
}

// Increment and decrement tests
TEST_F(BigNumberTest, Increment) {
    BigNumber num("42");

    // Pre-increment
    BigNumber& ref = ++num;
    EXPECT_EQ(num.toString(), "43");
    EXPECT_EQ(ref.toString(),
              "43");  // Reference should point to incremented value

    // Post-increment
    BigNumber copy = num++;
    EXPECT_EQ(copy.toString(), "43");  // Copy should be the original value
    EXPECT_EQ(num.toString(), "44");   // Original should be incremented
}

TEST_F(BigNumberTest, Decrement) {
    BigNumber num("42");

    // Pre-decrement
    BigNumber& ref = --num;
    EXPECT_EQ(num.toString(), "41");
    EXPECT_EQ(ref.toString(),
              "41");  // Reference should point to decremented value

    // Post-decrement
    BigNumber copy = num--;
    EXPECT_EQ(copy.toString(), "41");  // Copy should be the original value
    EXPECT_EQ(num.toString(), "40");   // Original should be decremented

    // Test decrementing to zero and below
    BigNumber one("1");
    EXPECT_EQ((--one).toString(), "0");
    EXPECT_EQ((--one).toString(), "-1");
}

// Misc operations
TEST_F(BigNumberTest, Negate) {
    EXPECT_EQ(zero.negate().toString(), "0");        // 0 remains 0
    EXPECT_EQ(one.negate().toString(), "-1");        // 1 becomes -1
    EXPECT_EQ(minusOne.negate().toString(), "1");    // -1 becomes 1
    EXPECT_EQ(positive.negate().toString(), "-42");  // 42 becomes -42
    EXPECT_EQ(negative.negate().toString(), "42");   // -42 becomes 42
}

TEST_F(BigNumberTest, Abs) {
    EXPECT_EQ(zero.abs().toString(), "0");
    EXPECT_EQ(one.abs().toString(), "1");
    EXPECT_EQ(minusOne.abs().toString(), "1");
    EXPECT_EQ(positive.abs().toString(), "42");
    EXPECT_EQ(negative.abs().toString(), "42");
}

TEST_F(BigNumberTest, IsOddEven) {
    EXPECT_TRUE(zero.isEven());
    EXPECT_FALSE(zero.isOdd());

    EXPECT_FALSE(one.isEven());
    EXPECT_TRUE(one.isOdd());

    EXPECT_TRUE(two().isEven());
    EXPECT_FALSE(two().isEven());

    EXPECT_FALSE(BigNumber("123").isEven());
    EXPECT_TRUE(BigNumber("123").isOdd());

    EXPECT_TRUE(BigNumber("456").isEven());
    EXPECT_FALSE(BigNumber("456").isOdd());

    // Test negative numbers
    EXPECT_FALSE(minusOne.isEven());
    EXPECT_TRUE(minusOne.isOdd());

    EXPECT_TRUE(BigNumber("-2").isEven());
    EXPECT_FALSE(BigNumber("-2").isOdd());
}

TEST_F(BigNumberTest, IsPositiveNegative) {
    EXPECT_TRUE(zero.isPositive());
    EXPECT_FALSE(zero.isNegative());

    EXPECT_TRUE(one.isPositive());
    EXPECT_FALSE(one.isNegative());

    EXPECT_FALSE(minusOne.isPositive());
    EXPECT_TRUE(minusOne.isNegative());

    EXPECT_TRUE(positive.isPositive());
    EXPECT_FALSE(positive.isNegative());

    EXPECT_FALSE(negative.isPositive());
    EXPECT_TRUE(negative.isNegative());
}

TEST_F(BigNumberTest, At) {
    BigNumber num("12345");

    // Remember digits are stored least significant first
    EXPECT_EQ(num.at(0), 5);
    EXPECT_EQ(num.at(1), 4);
    EXPECT_EQ(num.at(2), 3);
    EXPECT_EQ(num.at(3), 2);
    EXPECT_EQ(num.at(4), 1);

    // Out of range access should throw
    EXPECT_THROW(ATOM_UNUSED_RESULT(num.at(5)), std::out_of_range);
}

TEST_F(BigNumberTest, IndexOperator) {
    BigNumber num("12345");

    // Remember digits are stored least significant first
    EXPECT_EQ(num[0], 5);
    EXPECT_EQ(num[1], 4);
    EXPECT_EQ(num[2], 3);
    EXPECT_EQ(num[3], 2);
    EXPECT_EQ(num[4], 1);

    // Out of range access should throw
    EXPECT_THROW(num[5], std::out_of_range);
}

// Performance test (optional)
TEST_F(BigNumberTest, DISABLED_PerformanceTest) {
    // Generate large numbers for performance testing
    BigNumber large1 = generateRandomBigNumber(1000, false);
    BigNumber large2 = generateRandomBigNumber(1000, false);

    // Test addition performance
    auto startAdd = std::chrono::high_resolution_clock::now();
    BigNumber addResult = large1 + large2;
    auto endAdd = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> addTime = endAdd - startAdd;

    // Test multiplication performance
    auto startMul = std::chrono::high_resolution_clock::now();
    BigNumber mulResult = large1 * large2;
    auto endMul = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> mulTime = endMul - startMul;

    std::cout << "Addition of 1000-digit numbers: " << addTime.count() << " ms"
              << std::endl;
    std::cout << "Multiplication of 1000-digit numbers: " << mulTime.count()
              << " ms" << std::endl;
}
