/*
 * number_theory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Number theory functions - GCD, LCM, primes, modular arithmetic

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_NUMBER_THEORY_HPP
#define ATOM_ALGORITHM_MATH_NUMBER_THEORY_HPP

#include <concepts>
#include <memory>
#include <numeric>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

/**
 * @brief Thread-safe cache for math computations
 *
 * A singleton class that provides thread-safe caching for expensive
 * mathematical operations.
 */
class MathCache {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the singleton instance
     */
    static MathCache& getInstance() noexcept;

    /**
     * @brief Get a cached prime number vector up to the specified limit
     *
     * @param limit Upper bound for prime generation
     * @return std::shared_ptr<const std::vector<u64>> Thread-safe shared
     * pointer to prime vector
     */
    [[nodiscard]] std::shared_ptr<const std::vector<u64>> getCachedPrimes(
        u64 limit);

    /**
     * @brief Clear all cached values
     */
    void clear() noexcept;

private:
    MathCache() = default;
    ~MathCache() = default;
    MathCache(const MathCache&) = delete;
    MathCache& operator=(const MathCache&) = delete;
    MathCache(MathCache&&) = delete;
    MathCache& operator=(MathCache&&) = delete;

    std::shared_mutex mutex_;
    std::unordered_map<u64, std::shared_ptr<std::vector<u64>>> primeCache_;
};

/**
 * @brief Calculates the greatest common divisor (GCD) of two 64-bit integers.
 *
 * This function calculates the greatest common divisor (GCD) of two 64-bit
 * integers using std::gcd.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The greatest common divisor of the two 64-bit integers.
 */
[[nodiscard]] constexpr auto gcd64(u64 a, u64 b) noexcept -> u64 {
    // Using std::gcd from C++17, which is constexpr in C++20
    return std::gcd(a, b);
}

/**
 * @brief Calculates the least common multiple (LCM) of two 64-bit integers.
 *
 * This function calculates the least common multiple (LCM) of two 64-bit
 * integers using std::lcm with overflow checking.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The least common multiple of the two 64-bit integers.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] auto lcm64(u64 a, u64 b) -> u64;

/**
 * @brief Approximates the square root of a 64-bit integer.
 *
 * This function approximates the square root of a 64-bit integer using a fast
 * algorithm. Uses SIMD optimization when available.
 *
 * @param n The 64-bit integer for which to approximate the square root.
 * @return The approximate square root of the 64-bit integer.
 */
[[nodiscard]] auto approximateSqrt(u64 n) noexcept -> u64;

/**
 * @brief Fast exponentiation for integral types
 *
 * @tparam T Integral type
 * @param base The base value
 * @param exponent The exponent value
 * @return T The result of base^exponent
 */
template <std::integral T>
[[nodiscard]] constexpr auto fastPow(T base, T exponent) noexcept -> T {
    T result = 1;

    // Handle edge cases
    if (exponent < 0) {
        return (base == 1) ? 1 : 0;
    }

    // Binary exponentiation algorithm
    while (exponent > 0) {
        if (exponent & 1) {
            result *= base;
        }
        exponent >>= 1;
        base *= base;
    }

    return result;
}

/**
 * @brief Prime number checker using optimized trial division
 *
 * Uses cache for repeated checks of the same value.
 *
 * @param n Number to check
 * @return true If n is prime
 * @return false If n is not prime
 */
[[nodiscard]] auto isPrime(u64 n) noexcept -> bool;

/**
 * @brief Generates prime numbers up to a limit using the Sieve of Eratosthenes
 *
 * Uses thread-safe caching for repeated calls with the same limit.
 *
 * @param limit Upper limit for prime generation
 * @return std::vector<u64> Vector of primes up to limit
 */
[[nodiscard]] auto generatePrimes(u64 limit) -> std::vector<u64>;

/**
 * @brief Montgomery modular multiplication
 *
 * Uses optimized implementation for different platforms.
 *
 * @param a First operand
 * @param b Second operand
 * @param n Modulus
 * @return u64 (a * b) mod n
 */
[[nodiscard]] auto montgomeryMultiply(u64 a, u64 b, u64 n) -> u64;

/**
 * @brief Modular exponentiation using Montgomery reduction
 *
 * Uses optimized implementation with compile-time selection
 * between regular and Montgomery algorithms.
 *
 * @param base Base value
 * @param exponent Exponent value
 * @param modulus Modulus
 * @return u64 (base^exponent) mod modulus
 */
[[nodiscard]] auto modPow(u64 base, u64 exponent, u64 modulus) -> u64;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_NUMBER_THEORY_HPP
