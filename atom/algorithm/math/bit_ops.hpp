/*
 * bit_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Bit manipulation operations for 64-bit integers

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_BIT_OPS_HPP
#define ATOM_ALGORITHM_MATH_BIT_OPS_HPP

#ifdef __has_include
#if __has_include(<bit>)
#include <bit>
#define HAS_STD_BIT 1
#endif
#endif

#ifndef HAS_STD_BIT
#define HAS_STD_BIT 0
#endif

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Rotates a 64-bit integer to the left.
 *
 * This function rotates a 64-bit integer to the left by a specified number of
 * bits. Uses std::rotl from C++20 or fallback implementation.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] constexpr auto rotl64(u64 n, u32 c) noexcept -> u64 {
#if HAS_STD_BIT
    return std::rotl(n, static_cast<int>(c));
#else
    c &= 63;
    return (n << c) | (n >> (64 - c));
#endif
}

/**
 * @brief Rotates a 64-bit integer to the right.
 *
 * This function rotates a 64-bit integer to the right by a specified number of
 * bits. Uses std::rotr from C++20 or fallback implementation.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] constexpr auto rotr64(u64 n, u32 c) noexcept -> u64 {
#if HAS_STD_BIT
    return std::rotr(n, static_cast<int>(c));
#else
    c &= 63;
    return (n >> c) | (n << (64 - c));
#endif
}

/**
 * @brief Counts the leading zeros in a 64-bit integer.
 *
 * This function counts the number of leading zeros in a 64-bit integer.
 * Uses std::countl_zero from C++20 or fallback implementation.
 *
 * @param x The 64-bit integer to count leading zeros in.
 * @return The number of leading zeros in the 64-bit integer.
 */
[[nodiscard]] constexpr auto clz64(u64 x) noexcept -> i32 {
#if HAS_STD_BIT
    return std::countl_zero(x);
#else
    if (x == 0)
        return 64;
    i32 n = 0;
    if (x <= 0x00000000FFFFFFFF) {
        n += 32;
        x <<= 32;
    }
    if (x <= 0x0000FFFFFFFFFFFF) {
        n += 16;
        x <<= 16;
    }
    if (x <= 0x00FFFFFFFFFFFFFF) {
        n += 8;
        x <<= 8;
    }
    if (x <= 0x0FFFFFFFFFFFFFFF) {
        n += 4;
        x <<= 4;
    }
    if (x <= 0x3FFFFFFFFFFFFFFF) {
        n += 2;
        x <<= 2;
    }
    if (x <= 0x7FFFFFFFFFFFFFFF) {
        n += 1;
    }
    return n;
#endif
}

/**
 * @brief Normalizes a 64-bit integer.
 *
 * This function normalizes a 64-bit integer by shifting it to the left until
 * the most significant bit is set.
 *
 * @param x The 64-bit integer to normalize.
 * @return The normalized 64-bit integer.
 */
[[nodiscard]] constexpr auto normalize(u64 x) noexcept -> u64 {
    if (x == 0) {
        return 0;
    }
    i32 n = clz64(x);
    return x << n;
}

/**
 * @brief Checks if a 64-bit integer is a power of two.
 *
 * This function checks if a 64-bit integer is a power of two.
 * Uses std::has_single_bit from C++20 or fallback implementation.
 *
 * @param n The 64-bit integer to check.
 * @return True if the 64-bit integer is a power of two, false otherwise.
 */
[[nodiscard]] constexpr auto isPowerOfTwo(u64 n) noexcept -> bool {
#if HAS_STD_BIT
    return n != 0 && std::has_single_bit(n);
#else
    return n != 0 && (n & (n - 1)) == 0;
#endif
}

/**
 * @brief Calculates the next power of two for a 64-bit integer.
 *
 * This function calculates the next power of two for a 64-bit integer.
 * Uses std::bit_ceil from C++20 when available or fallback implementation.
 *
 * @param n The 64-bit integer for which to calculate the next power of two.
 * @return The next power of two for the 64-bit integer.
 */
[[nodiscard]] constexpr auto nextPowerOfTwo(u64 n) noexcept -> u64 {
    if (n == 0) {
        return 1;
    }

    // Fast path for powers of two
    if (isPowerOfTwo(n)) {
        return n;
    }

#if HAS_STD_BIT
    return std::bit_ceil(n);
#else
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
#endif
}

/**
 * @brief Calculates the bitwise reverse of a 64-bit integer.
 *
 * This function calculates the bitwise reverse of a 64-bit integer.
 * Uses optimized SIMD implementation when available.
 *
 * @param n The 64-bit integer to reverse.
 * @return The bitwise reverse of the 64-bit integer.
 */
[[nodiscard]] auto bitReverse64(u64 n) noexcept -> u64;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_BIT_OPS_HPP
