/*
 * math.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Extra Math Library

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_HPP
#define ATOM_ALGORITHM_MATH_HPP

#include <concepts>
#include <cstdint>
#include <span>
#include <vector>

namespace atom::algorithm {

// Concepts for numeric types
template <typename T>
concept UnsignedIntegral = std::unsigned_integral<T>;

template <typename T>
concept Arithmetic = std::integral<T> || std::floating_point<T>;

/**
 * @brief Performs a 64-bit multiplication followed by division.
 *
 * This function calculates the result of (operant * multiplier) / divider.
 *
 * @param operant The first operand for multiplication.
 * @param multiplier The second operand for multiplication.
 * @param divider The divisor for the division operation.
 * @return The result of (operant * multiplier) / divider.
 * @throws atom::error::InvalidArgumentException if divider is zero.
 */
[[nodiscard]] auto mulDiv64(uint64_t operant, uint64_t multiplier,
                            uint64_t divider) -> uint64_t;

/**
 * @brief Performs a safe addition operation.
 *
 * This function adds two unsigned 64-bit integers, handling potential overflow.
 *
 * @param a The first operand for addition.
 * @param b The second operand for addition.
 * @return The result of a + b.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] auto safeAdd(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Performs a safe multiplication operation.
 *
 * This function multiplies two unsigned 64-bit integers, handling potential
 * overflow.
 *
 * @param a The first operand for multiplication.
 * @param b The second operand for multiplication.
 * @return The result of a * b.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] auto safeMul(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Rotates a 64-bit integer to the left.
 *
 * This function rotates a 64-bit integer to the left by a specified number of
 * bits.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] auto rotl64(uint64_t n, unsigned int c) noexcept -> uint64_t;

/**
 * @brief Rotates a 64-bit integer to the right.
 *
 * This function rotates a 64-bit integer to the right by a specified number of
 * bits.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] auto rotr64(uint64_t n, unsigned int c) noexcept -> uint64_t;

/**
 * @brief Counts the leading zeros in a 64-bit integer.
 *
 * This function counts the number of leading zeros in a 64-bit integer.
 *
 * @param x The 64-bit integer to count leading zeros in.
 * @return The number of leading zeros in the 64-bit integer.
 */
[[nodiscard]] auto clz64(uint64_t x) noexcept -> int;

/**
 * @brief Normalizes a 64-bit integer.
 *
 * This function normalizes a 64-bit integer by shifting it to the left until
 * the most significant bit is set.
 *
 * @param x The 64-bit integer to normalize.
 * @return The normalized 64-bit integer.
 */
[[nodiscard]] auto normalize(uint64_t x) noexcept -> uint64_t;

/**
 * @brief Performs a safe subtraction operation.
 *
 * This function subtracts two unsigned 64-bit integers, handling potential
 * underflow.
 *
 * @param a The first operand for subtraction.
 * @param b The second operand for subtraction.
 * @return The result of a - b.
 * @throws atom::error::UnderflowException if the operation would underflow.
 */
[[nodiscard]] auto safeSub(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Performs a safe division operation.
 *
 * This function divides two unsigned 64-bit integers, handling potential
 * division by zero.
 *
 * @param a The numerator for division.
 * @param b The denominator for division.
 * @return The result of a / b.
 * @throws atom::error::InvalidArgumentException if there is a division by zero.
 */
[[nodiscard]] auto safeDiv(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Calculates the bitwise reverse of a 64-bit integer.
 *
 * This function calculates the bitwise reverse of a 64-bit integer.
 *
 * @param n The 64-bit integer to reverse.
 * @return The bitwise reverse of the 64-bit integer.
 */
[[nodiscard]] auto bitReverse64(uint64_t n) noexcept -> uint64_t;

/**
 * @brief Approximates the square root of a 64-bit integer.
 *
 * This function approximates the square root of a 64-bit integer using a fast
 * algorithm.
 *
 * @param n The 64-bit integer for which to approximate the square root.
 * @return The approximate square root of the 64-bit integer.
 */
[[nodiscard]] auto approximateSqrt(uint64_t n) noexcept -> uint64_t;

/**
 * @brief Calculates the greatest common divisor (GCD) of two 64-bit integers.
 *
 * This function calculates the greatest common divisor (GCD) of two 64-bit
 * integers.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The greatest common divisor of the two 64-bit integers.
 */
[[nodiscard]] auto gcd64(uint64_t a, uint64_t b) noexcept -> uint64_t;

/**
 * @brief Calculates the least common multiple (LCM) of two 64-bit integers.
 *
 * This function calculates the least common multiple (LCM) of two 64-bit
 * integers.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The least common multiple of the two 64-bit integers.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] auto lcm64(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Checks if a 64-bit integer is a power of two.
 *
 * This function checks if a 64-bit integer is a power of two.
 *
 * @param n The 64-bit integer to check.
 * @return True if the 64-bit integer is a power of two, false otherwise.
 */
[[nodiscard]] auto isPowerOfTwo(uint64_t n) noexcept -> bool;

/**
 * @brief Calculates the next power of two for a 64-bit integer.
 *
 * This function calculates the next power of two for a 64-bit integer.
 *
 * @param n The 64-bit integer for which to calculate the next power of two.
 * @return The next power of two for the 64-bit integer.
 */
[[nodiscard]] auto nextPowerOfTwo(uint64_t n) noexcept -> uint64_t;

/**
 * @brief Parallel addition of two vectors using SIMD
 *
 * @tparam T Arithmetic type
 * @param a First vector
 * @param b Second vector
 * @return std::vector<T> Result of addition
 */
template <Arithmetic T>
[[nodiscard]] auto parallelVectorAdd(std::span<const T> a,
                                     std::span<const T> b) -> std::vector<T>;

/**
 * @brief Parallel multiplication of two vectors using SIMD
 *
 * @tparam T Arithmetic type
 * @param a First vector
 * @param b Second vector
 * @return std::vector<T> Result of multiplication
 */
template <Arithmetic T>
[[nodiscard]] auto parallelVectorMul(std::span<const T> a,
                                     std::span<const T> b) -> std::vector<T>;

/**
 * @brief Fast exponentiation for integral types
 *
 * @tparam T Integral type
 * @param base The base value
 * @param exponent The exponent value
 * @return T The result of base^exponent
 */
template <std::integral T>
[[nodiscard]] auto fastPow(T base, T exponent) noexcept -> T;

/**
 * @brief Prime number checker using optimized trial division
 *
 * @param n Number to check
 * @return true If n is prime
 * @return false If n is not prime
 */
[[nodiscard]] auto isPrime(uint64_t n) noexcept -> bool;

/**
 * @brief Generates prime numbers up to a limit using the Sieve of Eratosthenes
 *
 * @param limit Upper limit for prime generation
 * @return std::vector<uint64_t> Vector of primes up to limit
 */
[[nodiscard]] auto generatePrimes(uint64_t limit) -> std::vector<uint64_t>;

/**
 * @brief Montgomery modular multiplication
 *
 * @param a First operand
 * @param b Second operand
 * @param n Modulus
 * @return uint64_t (a * b) mod n
 */
[[nodiscard]] auto montgomeryMultiply(uint64_t a, uint64_t b,
                                      uint64_t n) -> uint64_t;

/**
 * @brief Modular exponentiation using Montgomery reduction
 *
 * @param base Base value
 * @param exponent Exponent value
 * @param modulus Modulus
 * @return uint64_t (base^exponent) mod modulus
 */
[[nodiscard]] auto modPow(uint64_t base, uint64_t exponent,
                          uint64_t modulus) -> uint64_t;

}  // namespace atom::algorithm

#endif
