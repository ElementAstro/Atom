/*
 * base32.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "base32.hpp"
#include "../core/rust_numeric.hpp"

#include <spdlog/spdlog.h>
#include <span>
#include <string_view>
#include <vector>

namespace atom::algorithm {

// Base32实现
constexpr std::string_view BASE32_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

auto encodeBase32(std::span<const u8> data) noexcept
    -> atom::type::expected<std::string> {
    try {
        if (data.empty()) {
            return std::string{};
        }

        std::string encoded;
        encoded.reserve(((data.size() * 8) + 4) / 5);
        u32 buffer = 0;
        i32 bitsLeft = 0;

        for (u8 byte : data) {
            buffer = (buffer << 8) | byte;
            bitsLeft += 8;

            while (bitsLeft >= 5) {
                bitsLeft -= 5;
                encoded += BASE32_ALPHABET[(buffer >> bitsLeft) & 0x1F];
            }
        }

        // 处理剩余位
        if (bitsLeft > 0) {
            buffer <<= (5 - bitsLeft);
            encoded += BASE32_ALPHABET[buffer & 0x1F];
        }

        // 添加填充
        while (encoded.size() % 8 != 0) {
            encoded += '=';
        }

        return encoded;
    } catch (const std::exception& e) {
        spdlog::error("Base32 encode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 encode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 encoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 encoding");
    }
}

template <detail::ByteContainer T>
auto encodeBase32(const T& data) noexcept -> atom::type::expected<std::string> {
    try {
        const auto* byteData = reinterpret_cast<const u8*>(data.data());
        return encodeBase32(std::span<const u8>(byteData, data.size()));
    } catch (const std::exception& e) {
        spdlog::error("Base32 encode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 encode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 encoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 encoding");
    }
}

auto decodeBase32(std::string_view encoded_sv) noexcept
    -> atom::type::expected<std::vector<u8>> {
    try {
        // 验证输入
        for (char c_char : encoded_sv) {
            u8 c = static_cast<u8>(c_char);
            if (c != '=' &&
                BASE32_ALPHABET.find(c_char) == std::string_view::npos) {
                spdlog::error("Invalid character in Base32 input");
                return atom::type::make_unexpected(
                    "Invalid character in Base32 input");
            }
        }

        std::vector<u8> decoded;
        decoded.reserve((encoded_sv.size() * 5) / 8);

        u32 buffer = 0;
        i32 bitsLeft = 0;

        for (char c_char : encoded_sv) {
            u8 c = static_cast<u8>(c_char);
            if (c == '=') {
                break;  // 忽略填充
            }

            auto pos = BASE32_ALPHABET.find(c_char);
            if (pos == std::string_view::npos) {
                continue;  // 忽略无效字符
            }

            buffer = (buffer << 5) | static_cast<u32>(pos);
            bitsLeft += 5;

            if (bitsLeft >= 8) {
                bitsLeft -= 8;
                decoded.push_back(static_cast<u8>((buffer >> bitsLeft) & 0xFF));
            }
        }

        return decoded;
    } catch (const std::exception& e) {
        spdlog::error("Base32 decode error: {}", e.what());
        return atom::type::make_unexpected(
            std::string("Base32 decode error: ") + e.what());
    } catch (...) {
        spdlog::error("Unknown error during Base32 decoding");
        return atom::type::make_unexpected(
            "Unknown error during Base32 decoding");
    }
}

}  // namespace atom::algorithm
