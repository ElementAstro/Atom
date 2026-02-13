/*
 * hex.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Hexadecimal (Base16) encoding/decoding algorithms

**************************************************/

#ifndef ATOM_ALGORITHM_ENCODING_HEX_HPP
#define ATOM_ALGORITHM_ENCODING_HEX_HPP

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"

namespace atom::algorithm {

/**
 * @brief Encodes binary data to hexadecimal string (Base16).
 *
 * @param data The binary data to encode
 * @param uppercase Whether to use uppercase letters (default: true)
 * @return Hexadecimal string representation
 */
[[nodiscard]] auto encodeHex(std::span<const std::uint8_t> data,
                             bool uppercase = true) noexcept -> std::string;

/**
 * @brief Decodes hexadecimal string to binary data.
 *
 * @param hex The hexadecimal string to decode
 * @return Binary data or error if invalid hex string
 */
[[nodiscard]] auto decodeHex(std::string_view hex) noexcept
    -> atom::type::expected<std::vector<std::uint8_t>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ENCODING_HEX_HPP
