/*
 * bit.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: Validate aligned storage with optional Boost support

**************************************************/

#ifndef ATOM_UTILS_BIT_HPP
#define ATOM_UTILS_BIT_HPP

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <future>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || \
    defined(_M_IX86)
#include <immintrin.h>
#define ATOM_SIMD_SUPPORT
#endif

#include "atom/macro.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/integer.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits.hpp>
#endif

namespace atom::utils {

/**
 * @brief Concept for unsigned integral types.
 */
template <typename T>
concept UnsignedIntegral = std::unsigned_integral<T>;

/**
 * @brief Exception class for bit manipulation errors
 */
class BitManipulationException : public std::runtime_error {
public:
    explicit BitManipulationException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Creates a bitmask with the specified number of bits set to 1.
 *
 * This function generates a bitmask of type `T` where the lower `bits` number
 * of bits are set to 1. If the `bits` parameter is greater than or equal to
 * the number of bits in type `T`, the function returns the maximum value that
 * type `T` can represent.
 *
 * @tparam T The unsigned integral type of the bitmask.
 * @param bits The number of bits to set to 1.
 * @return T The bitmask with `bits` number of bits set to 1.
 * @throws BitManipulationException if bits is negative
 */
template <UnsignedIntegral T>
constexpr auto createMask(int32_t bits) -> T {
    if (bits < 0) {
        throw BitManipulationException("Number of bits cannot be negative");
    }

    if (bits >= std::numeric_limits<T>::digits) {
        return std::numeric_limits<T>::max();
    }
    return static_cast<T>((T{1} << bits) - 1);
}

/**
 * @brief Counts the number of set bits (1s) in the given value.
 *
 * This function counts and returns the number of set bits in the unsigned
 * integral value `value`. The number of bits is counted using the
 * `std::popcount` function, which is available in C++20 and later.
 *
 * @tparam T The unsigned integral type of the value.
 * @param value The value whose set bits are to be counted.
 * @return uint32_t The number of set bits in the value.
 */
template <UnsignedIntegral T>
constexpr auto countBytes(T value) ATOM_NOEXCEPT -> uint32_t {
#ifdef ATOM_USE_BOOST
    return boost::popcount(value);
#else
    return static_cast<uint32_t>(std::popcount(value));
#endif
}

/**
 * @brief Reverses the bits in the given value.
 *
 * This function reverses the bits of the unsigned integral value `value`. For
 * instance, if the value is `0b00000001` (1 in decimal) and the type has 8
 * bits, the function will return `0b10000000` (128 in decimal).
 *
 * @tparam T The unsigned integral type of the value.
 * @param value The value whose bits are to be reversed.
 * @return T The value with its bits reversed.
 */
template <UnsignedIntegral T>
constexpr auto reverseBits(T value) noexcept -> T {
#ifdef ATOM_USE_BOOST
    return boost::integer::reverse_bits(value);
#else
    // Optimized bit reversal algorithm for better performance
    constexpr int bits = std::numeric_limits<T>::digits;
    T result = 0;

    for (int i = 0; i < bits; ++i) {
        // Extract bit and position it at the reversed position
        result |= ((value >> i) & 1) << (bits - 1 - i);
    }

    return result;
#endif
}

/**
 * @brief Performs a left rotation on the bits of the given value.
 *
 * This function performs a bitwise left rotation on the unsigned integral value
 * `value` by the specified number of `shift` positions. The rotation is
 * circular, meaning that bits shifted out on the left will reappear on the
 * right.
 *
 * @tparam T The unsigned integral type of the value.
 * @param value The value to rotate.
 * @param shift The number of positions to rotate left.
 * @return T The value after left rotation.
 * @throws BitManipulationException if shift is negative (C++20 std::rotl
 * doesn't check this)
 */
template <UnsignedIntegral T>
constexpr auto rotateLeft(T value, int shift) -> T {
    if (shift < 0) {
        throw BitManipulationException(
            "Left rotation shift value cannot be negative");
    }

    try {
#ifdef ATOM_USE_BOOST
        return boost::integer::rotl(value, shift);
#else
        return std::rotl(value, shift);
#endif
    } catch (const std::exception& e) {
        throw BitManipulationException(std::string("Left rotation failed: ") +
                                       e.what());
    }
}

/**
 * @brief Performs a right rotation on the bits of the given value.
 *
 * This function performs a bitwise right rotation on the unsigned integral
 * value `value` by the specified number of `shift` positions. The rotation is
 * circular, meaning that bits shifted out on the right will reappear on the
 * left.
 *
 * @tparam T The unsigned integral type of the value.
 * @param value The value to rotate.
 * @param shift The number of positions to rotate right.
 * @return T The value after right rotation.
 * @throws BitManipulationException if shift is negative (C++20 std::rotr
 * doesn't check this)
 */
template <UnsignedIntegral T>
constexpr auto rotateRight(T value, int shift) -> T {
    if (shift < 0) {
        throw BitManipulationException(
            "Right rotation shift value cannot be negative");
    }

    try {
#ifdef ATOM_USE_BOOST
        return boost::integer::rotr(value, shift);
#else
        return std::rotr(value, shift);
#endif
    } catch (const std::exception& e) {
        throw BitManipulationException(std::string("Right rotation failed: ") +
                                       e.what());
    }
}

/**
 * @brief Merges two bitmasks into one.
 *
 * This function merges two bitmasks of type `T` into one by performing a
 * bitwise OR operation.
 *
 * @tparam T The unsigned integral type of the bitmasks.
 * @param mask1 The first bitmask.
 * @param mask2 The second bitmask.
 * @return T The merged bitmask.
 */
template <UnsignedIntegral T>
constexpr auto mergeMasks(T mask1, T mask2) ATOM_NOEXCEPT -> T {
#ifdef ATOM_USE_BOOST
    return boost::integer::bitwise_or(mask1, mask2);
#else
    return mask1 | mask2;
#endif
}

/**
 * @brief Splits a bitmask into two parts.
 *
 * This function splits a bitmask of type `T` into two parts at the specified
 * position.
 *
 * @tparam T The unsigned integral type of the bitmask.
 * @param mask The bitmask to split.
 * @param position The position to split the bitmask.
 * @return std::pair<T, T> A pair containing the two parts of the split bitmask.
 * @throws BitManipulationException if position is negative or exceeds bit width
 */
template <UnsignedIntegral T>
constexpr auto splitMask(T mask, int32_t position) -> std::pair<T, T> {
    constexpr int max_bits = std::numeric_limits<T>::digits;

    if (position < 0 || position > max_bits) {
        throw BitManipulationException("Split position must be between 0 and " +
                                       std::to_string(max_bits));
    }

    try {
#ifdef ATOM_USE_BOOST
        T lowerPart =
            boost::integer::bitwise_and(mask, createMask<T>(position));
        T upperPart =
            boost::integer::bitwise_and(mask, ~createMask<T>(position));
#else
        T lowerPart = mask & createMask<T>(position);
        T upperPart = mask & ~createMask<T>(position);
#endif
        return {lowerPart, upperPart};
    } catch (const std::exception& e) {
        throw BitManipulationException(std::string("Split mask failed: ") +
                                       e.what());
    }
}

/**
 * @brief Checks if a bit at the specified position is set.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to check.
 * @param position The bit position to check.
 * @return true If the bit is set.
 * @return false If the bit is not set.
 * @throws BitManipulationException if position is out of range
 */
template <UnsignedIntegral T>
constexpr auto isBitSet(T value, int position) -> bool {
    if (position < 0 || position >= std::numeric_limits<T>::digits) {
        throw BitManipulationException("Bit position out of range");
    }

    return (value & (T{1} << position)) != 0;
}

/**
 * @brief Sets a bit at the specified position.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to modify.
 * @param position The bit position to set.
 * @return T The modified value with the bit set.
 * @throws BitManipulationException if position is out of range
 */
template <UnsignedIntegral T>
constexpr auto setBit(T value, int position) -> T {
    if (position < 0 || position >= std::numeric_limits<T>::digits) {
        throw BitManipulationException("Bit position out of range");
    }

    return value | (T{1} << position);
}

/**
 * @brief Clears a bit at the specified position.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to modify.
 * @param position The bit position to clear.
 * @return T The modified value with the bit cleared.
 * @throws BitManipulationException if position is out of range
 */
template <UnsignedIntegral T>
constexpr auto clearBit(T value, int position) -> T {
    if (position < 0 || position >= std::numeric_limits<T>::digits) {
        throw BitManipulationException("Bit position out of range");
    }

    return value & ~(T{1} << position);
}

/**
 * @brief Toggles a bit at the specified position.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to modify.
 * @param position The bit position to toggle.
 * @return T The modified value with the bit toggled.
 * @throws BitManipulationException if position is out of range
 */
template <UnsignedIntegral T>
constexpr auto toggleBit(T value, int position) -> T {
    if (position < 0 || position >= std::numeric_limits<T>::digits) {
        throw BitManipulationException("Bit position out of range");
    }

    return value ^ (T{1} << position);
}

#ifdef ATOM_SIMD_SUPPORT
/**
 * @brief Counts set bits in a large array using SIMD instructions for
 * performance.
 *
 * @param data Pointer to the data array.
 * @param size Size of the array in bytes.
 * @return uint64_t Total count of set bits.
 */
inline auto countBitsParallel(const uint8_t* data, size_t size) -> uint64_t {
    constexpr size_t PARALLEL_THRESHOLD =
        1024;  // Threshold for parallel execution

    if (size < PARALLEL_THRESHOLD) {
        // For small arrays, use sequential processing
        uint64_t count = 0;
        for (size_t i = 0; i < size; ++i) {
            count += std::popcount(data[i]);
        }
        return count;
    }

    // For larger arrays, use parallel processing with std::async
    const size_t num_threads = std::min(std::thread::hardware_concurrency(),
                                        static_cast<unsigned>(16));
    const size_t chunk_size = (size + num_threads - 1) / num_threads;

    std::vector<std::future<uint64_t>> futures;
    futures.reserve(num_threads);

    // Launch parallel tasks
    for (size_t t = 0; t < num_threads; ++t) {
        size_t begin = t * chunk_size;
        size_t end = std::min(begin + chunk_size, size);

        if (begin >= size)
            break;

        futures.push_back(
            std::async(std::launch::async, [data, begin, end]() -> uint64_t {
                uint64_t count = 0;

#ifdef __AVX2__
                // Use AVX2 instructions for faster bit counting
                for (size_t i = begin; i + 32 <= end; i += 32) {
                    __m256i chunk = _mm256_loadu_si256(
                        reinterpret_cast<const __m256i*>(data + i));

                    // Count bits in each byte
                    for (int j = 0; j < 32; j++) {
                        count += std::popcount(static_cast<uint8_t>(
                            _mm256_extract_epi8(chunk, j)));
                    }
                }

                // Handle remaining bytes
                for (size_t i = begin + ((end - begin) / 32) * 32; i < end;
                     ++i) {
                    count += std::popcount(data[i]);
                }
#else
                // Fallback to standard approach
                for (size_t i = begin; i < end; ++i) {
                    count += std::popcount(data[i]);
                }
#endif

                return count;
            }));
    }

    // Collect results
    uint64_t total_count = 0;
    for (auto& future : futures) {
        try {
            total_count += future.get();
        } catch (const std::exception& e) {
            throw BitManipulationException(
                std::string("Parallel bit counting failed: ") + e.what());
        }
    }

    return total_count;
}
#endif

/**
 * @brief Finds the position of the first set bit.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to check.
 * @return int Position of the first set bit (0-indexed) or -1 if no bits are
 * set.
 */
template <UnsignedIntegral T>
constexpr auto findFirstSetBit(T value) ATOM_NOEXCEPT -> int {
    if (value == 0) {
        return -1;
    }

    return std::countr_zero(value);
}

/**
 * @brief Finds the position of the last set bit.
 *
 * @tparam T The unsigned integral type.
 * @param value The value to check.
 * @return int Position of the last set bit (0-indexed) or -1 if no bits are
 * set.
 */
template <UnsignedIntegral T>
constexpr auto findLastSetBit(T value) ATOM_NOEXCEPT -> int {
    if (value == 0) {
        return -1;
    }

    return std::numeric_limits<T>::digits - 1 - std::countl_zero(value);
}

/**
 * @brief Parallel bit operation over a range of values.
 *
 * @tparam T The unsigned integral type.
 * @tparam Op The operation to perform on each value.
 * @param input Input range of values.
 * @param op Binary operation to apply.
 * @return std::vector<T> Result vector after applying the operation.
 */
template <UnsignedIntegral T, typename Op>
auto parallelBitOp(std::span<const T> input, Op op) -> std::vector<T> {
    std::vector<T> result(input.size());

    constexpr size_t PARALLEL_THRESHOLD = 1024;

    if (input.size() < PARALLEL_THRESHOLD) {
        // For small arrays, use ranges for cleaner code
        std::ranges::transform(input, result.begin(), op);
        return result;
    }

    // For larger arrays, use parallel execution
    const size_t num_threads = std::min(std::thread::hardware_concurrency(),
                                        static_cast<unsigned>(16));
    const size_t chunk_size = (input.size() + num_threads - 1) / num_threads;

    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    // Launch parallel tasks
    for (size_t t = 0; t < num_threads; ++t) {
        size_t begin = t * chunk_size;
        size_t end = std::min(begin + chunk_size, input.size());

        if (begin >= input.size())
            break;

        futures.push_back(
            std::async(std::launch::async, [&input, &result, begin, end, op]() {
                for (size_t i = begin; i < end; ++i) {
                    result[i] = op(input[i]);
                }
            }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
        future.wait();
    }

    return result;
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_BIT_HPP