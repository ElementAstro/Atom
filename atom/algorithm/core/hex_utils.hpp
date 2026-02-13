/*
 * hex_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Shared hexadecimal conversion utilities

**************************************************/

#ifndef ATOM_ALGORITHM_CORE_HEX_UTILS_HPP
#define ATOM_ALGORITHM_CORE_HEX_UTILS_HPP

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/type/expected.hpp"

namespace atom::algorithm {

/**
 * @brief Converts a hexadecimal character to its numeric value.
 *
 * @param c The hex character ('0'-'9', 'A'-'F', 'a'-'f')
 * @return Expected containing the nibble value (0-15) or error
 */
[[nodiscard]] inline auto hexToNibble(char c) noexcept
    -> atom::type::expected<u8> {
    if (c >= '0' && c <= '9') {
        return static_cast<u8>(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<u8>(c - 'A' + 10);
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<u8>(c - 'a' + 10);
    }
    return atom::type::make_unexpected("Invalid hex character");
}

/**
 * @brief Converts a numeric nibble value to a hex character.
 *
 * @param nibble Value 0-15
 * @param uppercase If true, use uppercase letters
 * @return The hex character
 */
[[nodiscard]] constexpr auto nibbleToHex(u8 nibble,
                                         bool uppercase = true) noexcept
    -> char {
    if (nibble < 10) {
        return static_cast<char>('0' + nibble);
    }
    return static_cast<char>((uppercase ? 'A' : 'a') + nibble - 10);
}

/**
 * @brief Check if a character is a valid hexadecimal digit.
 *
 * @param c The character to check
 * @return true if valid hex digit
 */
[[nodiscard]] constexpr auto isHexDigit(char c) noexcept -> bool {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
           (c >= 'a' && c <= 'f');
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CORE_HEX_UTILS_HPP
