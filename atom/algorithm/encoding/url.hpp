/*
 * url.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: URL encoding/decoding algorithms (RFC 3986)

**************************************************/

#ifndef ATOM_ALGORITHM_ENCODING_URL_HPP
#define ATOM_ALGORITHM_ENCODING_URL_HPP

#include <string>

#include "atom/type/expected.hpp"

namespace atom::algorithm {

/**
 * @brief URL-encodes a string according to RFC 3986.
 *
 * @param str The string to encode
 * @param encodeSpaceAsPlus Whether to encode spaces as '+' instead of '%20'
 * @return URL-encoded string
 */
[[nodiscard]] auto urlEncode(std::string_view str,
                             bool encodeSpaceAsPlus = false) noexcept
    -> std::string;

/**
 * @brief URL-decodes a string.
 *
 * @param str The URL-encoded string to decode
 * @return Decoded string or error if invalid encoding
 */
[[nodiscard]] auto urlDecode(std::string_view str) noexcept
    -> atom::type::expected<std::string>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_ENCODING_URL_HPP
