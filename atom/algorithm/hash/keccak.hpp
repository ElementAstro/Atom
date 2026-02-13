/*
 * keccak.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Keccak-256 cryptographic hash function implementation.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_KECCAK_HPP
#define ATOM_ALGORITHM_HASH_KECCAK_HPP

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#include "../core/rust_numeric.hpp"

namespace atom::algorithm {

inline constexpr usize K_HASH_SIZE = 32;

/**
 * @brief Computes the Keccak-256 hash of the input data
 *
 * @param input Span of input data
 * @return std::array<u8, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc If memory allocation fails
 */
[[nodiscard]] auto keccak256(std::span<const u8> input) noexcept(false)
    -> std::array<u8, K_HASH_SIZE>;

/**
 * @brief Computes the Keccak-256 hash of the input string
 *
 * @param input Input string
 * @return std::array<u8, K_HASH_SIZE> The computed hash
 * @throws std::bad_alloc If memory allocation fails
 */
[[nodiscard]] inline auto keccak256(std::string_view input) noexcept(false)
    -> std::array<u8, K_HASH_SIZE> {
    return keccak256(std::span<const u8>(
        reinterpret_cast<const u8*>(input.data()), input.size()));
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_HASH_KECCAK_HPP
