/*
 * url.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "url.hpp"
#include "../core/hex_utils.hpp"

#include <spdlog/spdlog.h>
#include <cstddef>
#include <string_view>

namespace atom::algorithm {

// URL encoding implementation
auto urlEncode(std::string_view str, bool encodeSpaceAsPlus) noexcept
    -> std::string {
    std::string result;
    result.reserve(str.size() * 3);  // Worst case: every char needs encoding

    const char* hexChars = "0123456789ABCDEF";

    for (char c : str) {
        u8 uc = static_cast<u8>(c);

        // Unreserved characters (RFC 3986)
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
            c == '~') {
            result += c;
        } else if (c == ' ' && encodeSpaceAsPlus) {
            result += '+';
        } else {
            result += '%';
            result += hexChars[(uc >> 4) & 0x0F];
            result += hexChars[uc & 0x0F];
        }
    }

    return result;
}

auto urlDecode(std::string_view str) noexcept
    -> atom::type::expected<std::string> {
    try {
        std::string result;
        result.reserve(str.size());

        for (usize i = 0; i < str.size(); ++i) {
            if (str[i] == '%') {
                if (i + 2 >= str.size()) {
                    return atom::type::make_unexpected(
                        "Invalid URL encoding: incomplete percent sequence");
                }

                auto highNibble = hexToNibble(str[i + 1]);
                auto lowNibble = hexToNibble(str[i + 2]);

                if (!highNibble || !lowNibble) {
                    return atom::type::make_unexpected(
                        "Invalid URL encoding: invalid hex character");
                }

                result += static_cast<char>((highNibble.value() << 4) |
                                            lowNibble.value());
                i += 2;  // Skip the two hex digits
            } else if (str[i] == '+') {
                result += ' ';  // Convert '+' to space
            } else {
                result += str[i];
            }
        }

        return result;
    } catch (const std::exception& e) {
        spdlog::error("URL decode error: {}", e.what());
        return atom::type::make_unexpected(std::string("URL decode error: ") +
                                           e.what());
    }
}

}  // namespace atom::algorithm
