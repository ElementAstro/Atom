#ifndef ATOM_ALGORITHM_COMMON_ENDIAN_HPP
#define ATOM_ALGORITHM_COMMON_ENDIAN_HPP

#include <bit>
#include <cstdint>
#include <span>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Byte swap utilities for endianness conversion.
 *
 * These functions provide efficient byte swapping for converting between
 * big-endian and little-endian representations.
 */
namespace endian {

/**
 * @brief Swap bytes of a 16-bit value.
 * @param val The value to swap.
 * @return The byte-swapped value.
 */
[[nodiscard]] constexpr auto byteSwap16(u16 val) noexcept -> u16 {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(val);
#else
    return static_cast<u16>((val << 8) | (val >> 8));
#endif
}

/**
 * @brief Swap bytes of a 32-bit value.
 * @param val The value to swap.
 * @return The byte-swapped value.
 */
[[nodiscard]] constexpr auto byteSwap32(u32 val) noexcept -> u32 {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(val);
#else
    return ((val & 0xFF000000U) >> 24) | ((val & 0x00FF0000U) >> 8) |
           ((val & 0x0000FF00U) << 8) | ((val & 0x000000FFU) << 24);
#endif
}

/**
 * @brief Swap bytes of a 64-bit value.
 * @param val The value to swap.
 * @return The byte-swapped value.
 */
[[nodiscard]] constexpr auto byteSwap64(u64 val) noexcept -> u64 {
#if __cpp_lib_byteswap >= 202110L
    return std::byteswap(val);
#else
    return ((val & 0xFF00000000000000ULL) >> 56) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x000000FF00000000ULL) >> 8) |
           ((val & 0x00000000FF000000ULL) << 8) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x00000000000000FFULL) << 56);
#endif
}

/**
 * @brief Convert from native byte order to big-endian.
 * @param val The value in native byte order.
 * @return The value in big-endian byte order.
 */
[[nodiscard]] constexpr auto nativeToBig32(u32 val) noexcept -> u32 {
    if constexpr (std::endian::native == std::endian::little) {
        return byteSwap32(val);
    } else {
        return val;
    }
}

/**
 * @brief Convert from big-endian to native byte order.
 * @param val The value in big-endian byte order.
 * @return The value in native byte order.
 */
[[nodiscard]] constexpr auto bigToNative32(u32 val) noexcept -> u32 {
    return nativeToBig32(val);  // Same operation
}

/**
 * @brief Convert from native byte order to little-endian.
 * @param val The value in native byte order.
 * @return The value in little-endian byte order.
 */
[[nodiscard]] constexpr auto nativeToLittle32(u32 val) noexcept -> u32 {
    if constexpr (std::endian::native == std::endian::big) {
        return byteSwap32(val);
    } else {
        return val;
    }
}

/**
 * @brief Convert from little-endian to native byte order.
 * @param val The value in little-endian byte order.
 * @return The value in native byte order.
 */
[[nodiscard]] constexpr auto littleToNative32(u32 val) noexcept -> u32 {
    return nativeToLittle32(val);  // Same operation
}

/**
 * @brief Read a 32-bit big-endian value from a byte span.
 * @param bytes The byte span (must have at least 4 bytes).
 * @return The 32-bit value in native byte order.
 */
[[nodiscard]] inline auto readBig32(std::span<const u8, 4> bytes) noexcept
    -> u32 {
    return (static_cast<u32>(bytes[0]) << 24) |
           (static_cast<u32>(bytes[1]) << 16) |
           (static_cast<u32>(bytes[2]) << 8) | static_cast<u32>(bytes[3]);
}

/**
 * @brief Read a 32-bit little-endian value from a byte span.
 * @param bytes The byte span (must have at least 4 bytes).
 * @return The 32-bit value in native byte order.
 */
[[nodiscard]] inline auto readLittle32(std::span<const u8, 4> bytes) noexcept
    -> u32 {
    return static_cast<u32>(bytes[0]) | (static_cast<u32>(bytes[1]) << 8) |
           (static_cast<u32>(bytes[2]) << 16) |
           (static_cast<u32>(bytes[3]) << 24);
}

/**
 * @brief Write a 32-bit value as big-endian bytes.
 * @param val The value to write.
 * @param bytes The destination byte span (must have at least 4 bytes).
 */
inline void writeBig32(u32 val, std::span<u8, 4> bytes) noexcept {
    bytes[0] = static_cast<u8>(val >> 24);
    bytes[1] = static_cast<u8>(val >> 16);
    bytes[2] = static_cast<u8>(val >> 8);
    bytes[3] = static_cast<u8>(val);
}

/**
 * @brief Write a 32-bit value as little-endian bytes.
 * @param val The value to write.
 * @param bytes The destination byte span (must have at least 4 bytes).
 */
inline void writeLittle32(u32 val, std::span<u8, 4> bytes) noexcept {
    bytes[0] = static_cast<u8>(val);
    bytes[1] = static_cast<u8>(val >> 8);
    bytes[2] = static_cast<u8>(val >> 16);
    bytes[3] = static_cast<u8>(val >> 24);
}

}  // namespace endian

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_COMMON_ENDIAN_HPP
