#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "atom/algorithm/math.hpp"
#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

// Test for mulDiv64: (operant * multiplier) / divider
TEST(MathTest, MulDiv64_Normal) {
    uint64_t operant = 10, multiplier = 20, divider = 5;
    uint64_t expected = (10 * 20) / 5;
    EXPECT_EQ(atom::algorithm::mulDiv64(operant, multiplier, divider),
              expected);
}

// Test for mulDiv64 division by zero exception - assuming
// InvalidArgument thrown.
TEST(MathTest, MulDiv64_DivideByZero) {
    uint64_t operant = 10, multiplier = 20, divider = 0;
    EXPECT_THROW(ATOM_UNUSED_RESULT(
                     atom::algorithm::mulDiv64(operant, multiplier, divider)),
                 atom::error::InvalidArgument);
}

// Test safeAdd normal addition
TEST(MathTest, SafeAdd_Normal) {
    uint64_t a = 100, b = 200;
    EXPECT_EQ(atom::algorithm::safeAdd(a, b), 300);
}

// Test safeAdd overflow
TEST(MathTest, SafeAdd_Overflow) {
    uint64_t a = std::numeric_limits<uint64_t>::max();
    uint64_t b = 1;
    EXPECT_THROW(ATOM_UNUSED_RESULT(atom::algorithm::safeAdd(a, b)),
                 atom::error::OverflowException);
}

// Test safeMul normal multiplication
TEST(MathTest, SafeMul_Normal) {
    uint64_t a = 20, b = 30;
    EXPECT_EQ(atom::algorithm::safeMul(a, b), 600);
}

// Test safeMul overflow
TEST(MathTest, SafeMul_Overflow) {
    uint64_t a = std::numeric_limits<uint64_t>::max(), b = 2;
    EXPECT_THROW(ATOM_UNUSED_RESULT(atom::algorithm::safeMul(a, b)),
                 atom::error::OverflowException);
}

// Test rotl64 and rotr64
TEST(MathTest, RotateLeftRight64) {
    uint64_t value = 0x0123456789ABCDEF;
    unsigned int shift = 8;
    uint64_t left = atom::algorithm::rotl64(value, shift);
    uint64_t right = atom::algorithm::rotr64(left, shift);
    EXPECT_EQ(right, value);
}

// Test clz64 with known value
TEST(MathTest, Clz64) {
    // The MSB is set at bit 63, so clz64(0x8000000000000000) should return 0.
    EXPECT_EQ(atom::algorithm::clz64(0x8000000000000000ULL), 0);
    // For value 1, there are 63 leading zeros.
    EXPECT_EQ(atom::algorithm::clz64(1ULL), 63);
}

// Test normalize: shifting left until MSB is set. For example, normalize(1)
// should return 0x8000000000000000.
TEST(MathTest, Normalize) {
    uint64_t normalized = atom::algorithm::normalize(1);
    EXPECT_EQ(normalized, 0x8000000000000000ULL);
}

// Test safeSub normal subtraction
TEST(MathTest, SafeSub_Normal) {
    uint64_t a = 500, b = 300;
    EXPECT_EQ(atom::algorithm::safeSub(a, b), 200);
}

// Test safeSub underflow
TEST(MathTest, SafeSub_Underflow) {
    uint64_t a = 300, b = 500;
    EXPECT_THROW(ATOM_UNUSED_RESULT(atom::algorithm::safeSub(a, b)),
                 atom::error::UnderflowException);
}

// Test safeDiv normal division
TEST(MathTest, SafeDiv_Normal) {
    uint64_t a = 100, b = 5;
    EXPECT_EQ(atom::algorithm::safeDiv(a, b), 20);
}

// Test safeDiv division by zero
TEST(MathTest, SafeDiv_DivideByZero) {
    uint64_t a = 100, b = 0;
    EXPECT_THROW(ATOM_UNUSED_RESULT(atom::algorithm::safeDiv(a, b)),
                 atom::error::InvalidArgument);
}

// Test bitReverse64 with known value (example: reverse of 0x0123456789ABCDEF)
TEST(MathTest, BitReverse64) {
    uint64_t value = 0x0123456789ABCDEFULL;
    uint64_t reversed = atom::algorithm::bitReverse64(value);
    // Reverse the bits by comparing with a hand computed value if possible.
    // For testing, reverse twice should yield the original.
    EXPECT_EQ(atom::algorithm::bitReverse64(reversed), value);
}

// Test approximateSqrt with perfect square and non-perfect square
TEST(MathTest, ApproximateSqrt) {
    uint64_t square = 144;
    uint64_t approx = atom::algorithm::approximateSqrt(square);
    EXPECT_NEAR(approx, 12, 1);  // tolerance 1 unit

    uint64_t nonSquare = 150;
    uint64_t approxNS = atom::algorithm::approximateSqrt(nonSquare);
    // The approximate sqrt may not be exact, check within tolerance.
    double expected = std::sqrt(150.0);
    EXPECT_NEAR(static_cast<double>(approxNS), expected, 2);
}

// Test gcd64 with known pairs
TEST(MathTest, Gcd64) {
    EXPECT_EQ(atom::algorithm::gcd64(54, 24), 6);
    EXPECT_EQ(atom::algorithm::gcd64(17, 13), 1);
}

// Test lcm64 with known pairs and potential overflow
TEST(MathTest, Lcm64) {
    EXPECT_EQ(atom::algorithm::lcm64(4, 6), 12);
    EXPECT_EQ(atom::algorithm::lcm64(21, 6), 42);
    // Optionally, test overflow scenario if applicable (expect exception)
}

// Test isPowerOfTwo and nextPowerOfTwo
TEST(MathTest, PowerOfTwoFunctions) {
    EXPECT_TRUE(atom::algorithm::isPowerOfTwo(256));
    EXPECT_FALSE(atom::algorithm::isPowerOfTwo(300));
    EXPECT_EQ(atom::algorithm::nextPowerOfTwo(300), 512);
    EXPECT_EQ(atom::algorithm::nextPowerOfTwo(512), 512);
}

// Test parallelVectorAdd
TEST(MathTest, ParallelVectorAdd) {
    std::vector<uint64_t> a = {1, 2, 3, 4};
    std::vector<uint64_t> b = {10, 20, 30, 40};
    auto result = atom::algorithm::parallelVectorAdd<uint64_t>(a, b);
    std::vector<uint64_t> expected = {11, 22, 33, 44};
    EXPECT_EQ(result, expected);
}

// Test parallelVectorMul
TEST(MathTest, ParallelVectorMul) {
    std::vector<uint64_t> a = {2, 3, 4};
    std::vector<uint64_t> b = {5, 6, 7};
    auto result = atom::algorithm::parallelVectorMul<uint64_t>(a, b);
    std::vector<uint64_t> expected = {10, 18, 28};
    EXPECT_EQ(result, expected);
}

// Test fastPow for integral types
TEST(MathTest, FastPow) {
    EXPECT_EQ(atom::algorithm::fastPow(2, 10), 1024);
    EXPECT_EQ(atom::algorithm::fastPow(3, 0), 1);
    EXPECT_EQ(atom::algorithm::fastPow(5, 3), 125);
}

// Test isPrime
TEST(MathTest, IsPrime) {
    EXPECT_TRUE(atom::algorithm::isPrime(2));
    EXPECT_TRUE(atom::algorithm::isPrime(13));
    EXPECT_FALSE(atom::algorithm::isPrime(1));
    EXPECT_FALSE(atom::algorithm::isPrime(100));
}

// Test generatePrimes
TEST(MathTest, GeneratePrimes) {
    auto primes = atom::algorithm::generatePrimes(50);
    std::vector<uint64_t> expected = {2,  3,  5,  7,  11, 13, 17, 19,
                                      23, 29, 31, 37, 41, 43, 47};
    EXPECT_EQ(primes, expected);
}

// Test montgomeryMultiply against standard modulo multiplication
TEST(MathTest, MontgomeryMultiply) {
    uint64_t a = 123456789, b = 987654321, n = 1000000007;
    uint64_t expected = (a % n * b % n) % n;
    uint64_t result = atom::algorithm::montgomeryMultiply(a, b, n);
    EXPECT_EQ(result, expected);
}

// Test modPow (modular exponentiation)
TEST(MathTest, ModPow) {
    // 2^10 mod 1000 = 1024 mod 1000 = 24
    EXPECT_EQ(atom::algorithm::modPow(2, 10, 1000), 24);
    // 3^20 mod 50
    uint64_t res = atom::algorithm::modPow(3, 20, 50);
    // Calculate expected using std::pow in double then mod; alternatively,
    // precomputed value.
    EXPECT_EQ(res, 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}