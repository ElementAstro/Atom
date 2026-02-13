/*
 * hex_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Hexadecimal string conversion utilities for data encoding
             and decoding.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_HEX_UTILS_HPP
#define ATOM_ALGORITHM_HASH_HEX_UTILS_HPP

#include <string>
#include <string_view>

#include "atom/macro.hpp"

namespace atom::algorithm {

/**
 * @brief Converts a string to a hexadecimal string representation.
 *
 * @param data The input string.
 * @return std::string The hexadecimal string representation.
 * @throws std::bad_alloc If memory allocation fails
 */
ATOM_NODISCARD auto hexstringFromData(std::string_view data) noexcept(false)
    -> std::string;

/**
 * @brief Converts a hexadecimal string representation to binary data.
 *
 * @param data The input hexadecimal string.
 * @return std::string The binary data.
 * @throws std::invalid_argument If the input hexstring is not a valid
 * hexadecimal string.
 * @throws std::bad_alloc If memory allocation fails
 */
ATOM_NODISCARD auto dataFromHexstring(std::string_view data) noexcept(false)
    -> std::string;

/**
 * @brief Checks if a string can be converted to hexadecimal.
 *
 * @param str The string to check.
 * @return bool True if convertible to hexadecimal, false otherwise.
 */
[[nodiscard]] bool supportsHexStringConversion(std::string_view str) noexcept;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_HASH_HEX_UTILS_HPP
