#ifndef ATOM_ALGORITHM_CRYPTO_TEA_COMMON_HPP
#define ATOM_ALGORITHM_CRYPTO_TEA_COMMON_HPP

#include <array>
#include <concepts>
#include <span>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/algorithm_exception.hpp"  // CryptoException
#include "atom/algorithm/common/concepts.hpp"      // UInt32Container

namespace atom::algorithm {

// Shared constants for TEA family algorithms
constexpr u32 TEA_DELTA = 0x9E3779B9;
constexpr i32 TEA_NUM_ROUNDS = 32;
constexpr i32 SHIFT_4 = 4;
constexpr i32 SHIFT_5 = 5;
constexpr i32 BYTE_SHIFT = 8;
constexpr usize MIN_ROUNDS = 6;
constexpr usize MAX_ROUNDS = 52;
constexpr i32 SHIFT_3 = 3;
constexpr i32 SHIFT_2 = 2;
constexpr u32 KEY_MASK = 3;
constexpr i32 SHIFT_11 = 11;

/**
 * @brief Helper function to validate a 128-bit TEA key.
 *
 * Checks if the key is all zeros (insecure) and warns about low entropy.
 *
 * @param key The 128-bit key as an array of four 32-bit unsigned integers.
 * @return true if the key is valid, false otherwise.
 */
bool isValidKey(const std::array<u32, 4>& key) noexcept;

/**
 * @brief Custom exception class for TEA-related errors.
 *
 * Inherits from CryptoException in the unified algorithm exception hierarchy.
 * Provides a simple string constructor for backward compatibility.
 */
class TEAException : public CryptoException {
public:
    explicit TEAException(const std::string& message)
        : CryptoException("", 0, "", message) {}
    using CryptoException::CryptoException;
};

// UInt32Container concept is now defined in atom/algorithm/common/concepts.hpp

/**
 * @brief Type alias for a 128-bit key used in the XTEA algorithm.
 *
 * Represents the key as an array of four 32-bit unsigned integers.
 */
using XTEAKey = std::array<u32, 4>;

/**
 * @brief Converts a byte array to a vector of 32-bit unsigned integers.
 *
 * This function is used to prepare byte data for encryption or decryption with
 * the XXTEA algorithm.
 *
 * @tparam T A type that satisfies the requirements of a contiguous range of
 * uint8_t.
 * @param data The byte array to be converted.
 * @return A vector of 32-bit unsigned integers.
 */
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             std::same_as<std::ranges::range_value_t<T>, u8>
auto toUint32Vector(const T &data) -> std::vector<u32>;

/**
 * @brief Converts a vector of 32-bit unsigned integers back to a byte array.
 *
 * This function is used to convert the result of XXTEA decryption back into a
 * byte array.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param data The vector of 32-bit unsigned integers to be converted.
 * @return A byte array.
 */
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<u8>;

/**
 * @brief Implementation detail for converting a byte array to a vector of
 * u32.
 *
 * This function performs the actual conversion from a byte array to a vector of
 * 32-bit unsigned integers.
 *
 * @param data A span of bytes to convert.
 * @return A vector of 32-bit unsigned integers.
 */
auto toUint32VectorImpl(std::span<const u8> data) -> std::vector<u32>;

/**
 * @brief Implementation detail for converting a vector of u32 to a byte
 * array.
 *
 * This function performs the actual conversion from a vector of 32-bit unsigned
 * integers to a byte array.
 *
 * @param data A span of 32-bit unsigned integers to convert.
 * @return A vector of bytes.
 */
auto toByteArrayImpl(std::span<const u32> data) -> std::vector<u8>;

/**
 * @brief Converts a byte array to a vector of 32-bit unsigned integers.
 *
 * This function is used to prepare byte data for encryption or decryption with
 * the XXTEA algorithm.
 *
 * @tparam T A type that satisfies the requirements of a contiguous range of
 * u8.
 * @param data The byte array to be converted.
 * @return A vector of 32-bit unsigned integers.
 */
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             std::same_as<std::ranges::range_value_t<T>, u8>
auto toUint32Vector(const T &data) -> std::vector<u32> {
    return toUint32VectorImpl(std::span<const u8>{data.data(), data.size()});
}

/**
 * @brief Converts a vector of 32-bit unsigned integers back to a byte array.
 *
 * This function is used to convert the result of XXTEA decryption back into a
 * byte array.
 *
 * @tparam Container A type that satisfies the UInt32Container concept.
 * @param data The vector of 32-bit unsigned integers to be converted.
 * @return A byte array.
 */
template <UInt32Container Container>
auto toByteArray(const Container &data) -> std::vector<u8> {
    return toByteArrayImpl(std::span<const u32>{data.data(), data.size()});
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_TEA_COMMON_HPP
