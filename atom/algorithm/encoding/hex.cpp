/*
 * hex.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "hex.hpp"
#include "../core/hex_utils.hpp"

#include <spdlog/spdlog.h>
#include <cstddef>
#include <string_view>
#include <vector>

namespace atom::algorithm {

// Base16/Hex encoding implementation
auto encodeHex(std::span<const std::uint8_t> data, bool uppercase) noexcept
    -> std::string {
    if (data.empty()) {
        return {};
    }

    const char* hexChars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string result;
    result.reserve(data.size() * 2);

    for (u8 byte : data) {
        result += hexChars[(byte >> 4) & 0x0F];
        result += hexChars[byte & 0x0F];
    }

    return result;
}

auto decodeHex(std::string_view hex) noexcept
    -> atom::type::expected<std::vector<std::uint8_t>> {
    try {
        if (hex.size() % 2 != 0) {
            return atom::type::make_unexpected(
                "Hex string must have even length");
        }

        std::vector<std::uint8_t> result;
        result.reserve(hex.size() / 2);

        for (usize i = 0; i < hex.size(); i += 2) {
            auto highNibble = hexToNibble(hex[i]);
            auto lowNibble = hexToNibble(hex[i + 1]);

            if (!highNibble || !lowNibble) {
                return atom::type::make_unexpected("Invalid hex character");
            }

            result.push_back((highNibble.value() << 4) | lowNibble.value());
        }

        return result;
    } catch (const std::exception& e) {
        spdlog::error("Hex decode error: {}", e.what());
        return atom::type::make_unexpected(std::string("Hex decode error: ") +
                                           e.what());
    }
}

}  // namespace atom::algorithm
