/*
 * base32.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Base32 encoding/decoding algorithms

**************************************************/

#ifndef ATOM_ALGORITHM_ENCODING_BASE32_HPP
#define ATOM_ALGORITHM_ENCODING_BASE32_HPP

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "detail.hpp"
#include "atom/type/expected.hpp"

namespace atom::algorithm {

/**
 * @brief Encodes a byte container into a Base32 string.
 *
 * @tparam T Container type that satisfies ByteContainer concept
 * @param data The input data to encode
 * @return atom::type::expected<std::string> Encoded string or error
 */
template <detail::ByteContainer T>
[[nodiscard]] auto encodeBase32(const T& data) noexcept
    -> atom::type::expected<std::string>;

/**
 * @brief Specialized Base32 encoder for vector<uint8_t>
 * @param data The input data to encode
 * @return atom::type::expected<std::string> Encoded string or error
 */
[[nodiscard]] auto encodeBase32(std::span<const uint8_t> data) noexcept
    -> atom::type::expected<std::string>;

/**
 * @brief Decodes a Base32 encoded string back into bytes.
 *
 * @param encoded The Base32 encoded string
 * @return atom::type::expected<std::vector<uint8_t>> Decoded bytes or error
 */
[[nodiscard]] auto decodeBase32(std::string_view encoded) noexcept
    -> atom::type::expected<std::vector<uint8_t>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ENCODING_BASE32_HPP
