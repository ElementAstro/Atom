#ifndef ATOM_ALGORITHM_COMMON_HEX_HPP
#define ATOM_ALGORITHM_COMMON_HEX_HPP

#include <array>
#include <span>
#include <string>
#include <string_view>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Hexadecimal conversion utilities.
 */
namespace hex {

/**
 * @brief Lookup table for byte to hex conversion.
 */
inline constexpr std::array<char, 16> HEX_CHARS = {'0', '1', '2', '3', '4', '5',
                                                   '6', '7', '8', '9', 'a', 'b',
                                                   'c', 'd', 'e', 'f'};

/**
 * @brief Lookup table for uppercase hex conversion.
 */
inline constexpr std::array<char, 16> HEX_CHARS_UPPER = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/**
 * @brief Convert a byte array to a hexadecimal string.
 * @param bytes The byte array to convert.
 * @param uppercase Use uppercase hex characters if true.
 * @return The hexadecimal string representation.
 */
[[nodiscard]] inline auto toHexString(std::span<const u8> bytes,
                                      bool uppercase = false) -> std::string {
    std::string result;
    result.reserve(bytes.size() * 2);

    const auto& chars = uppercase ? HEX_CHARS_UPPER : HEX_CHARS;
    for (u8 byte : bytes) {
        result.push_back(chars[(byte >> 4) & 0x0F]);
        result.push_back(chars[byte & 0x0F]);
    }

    return result;
}

/**
 * @brief Convert a fixed-size byte array to a hexadecimal string.
 * @tparam N The size of the byte array.
 * @param bytes The byte array to convert.
 * @param uppercase Use uppercase hex characters if true.
 * @return The hexadecimal string representation.
 */
template <usize N>
[[nodiscard]] inline auto toHexString(const std::array<u8, N>& bytes,
                                      bool uppercase = false) -> std::string {
    return toHexString(std::span<const u8>{bytes.data(), bytes.size()},
                       uppercase);
}

/**
 * @brief Convert a span of std::byte to a hexadecimal string.
 * @param bytes The byte span to convert.
 * @param uppercase Use uppercase hex characters if true.
 * @return The hexadecimal string representation.
 */
[[nodiscard]] inline auto toHexString(std::span<const std::byte> bytes,
                                      bool uppercase = false) -> std::string {
    std::string result;
    result.reserve(bytes.size() * 2);

    const auto& chars = uppercase ? HEX_CHARS_UPPER : HEX_CHARS;
    for (std::byte byte : bytes) {
        auto val = std::to_integer<u8>(byte);
        result.push_back(chars[(val >> 4) & 0x0F]);
        result.push_back(chars[val & 0x0F]);
    }

    return result;
}

/**
 * @brief Convert a 32-bit word array to a hexadecimal string.
 *
 * Each word is converted to 8 hex characters in little-endian order.
 *
 * @param words The word array to convert.
 * @param uppercase Use uppercase hex characters if true.
 * @return The hexadecimal string representation.
 */
[[nodiscard]] inline auto wordsToHexString(std::span<const u32> words,
                                           bool uppercase = false)
    -> std::string {
    std::string result;
    result.reserve(words.size() * 8);

    const auto& chars = uppercase ? HEX_CHARS_UPPER : HEX_CHARS;
    for (u32 word : words) {
        // Little-endian byte order
        for (int i = 0; i < 4; ++i) {
            u8 byte = static_cast<u8>(word >> (i * 8));
            result.push_back(chars[(byte >> 4) & 0x0F]);
            result.push_back(chars[byte & 0x0F]);
        }
    }

    return result;
}

/**
 * @brief Convert a hex character to its numeric value.
 * @param c The hex character ('0'-'9', 'a'-'f', 'A'-'F').
 * @return The numeric value (0-15), or -1 if invalid.
 */
[[nodiscard]] constexpr auto hexCharToValue(char c) noexcept -> i32 {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/**
 * @brief Convert a hexadecimal string to a byte vector.
 * @param hex The hexadecimal string (must have even length).
 * @return The byte vector, or empty if invalid input.
 */
[[nodiscard]] inline auto fromHexString(std::string_view hex)
    -> std::vector<u8> {
    if (hex.size() % 2 != 0) {
        return {};
    }

    std::vector<u8> result;
    result.reserve(hex.size() / 2);

    for (usize i = 0; i < hex.size(); i += 2) {
        i32 high = hexCharToValue(hex[i]);
        i32 low = hexCharToValue(hex[i + 1]);

        if (high < 0 || low < 0) {
            return {};
        }

        result.push_back(static_cast<u8>((high << 4) | low));
    }

    return result;
}

}  // namespace hex

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_COMMON_HEX_HPP
