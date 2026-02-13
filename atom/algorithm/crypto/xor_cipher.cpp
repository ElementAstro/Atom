/*
 * xor_cipher.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "xor_cipher.hpp"
#include "atom/algorithm/core/rust_numeric.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>

namespace atom::algorithm {

auto xorEncryptDecrypt(std::string_view text, u8 key) noexcept -> std::string {
    std::string result;
    result.reserve(text.size());

    std::ranges::transform(text, std::back_inserter(result), [key](char c) {
        return static_cast<char>(static_cast<u8>(c) ^ key);
    });
    return result;
}

auto xorEncrypt(std::string_view plaintext, u8 key) noexcept -> std::string {
    return xorEncryptDecrypt(plaintext, key);
}

auto xorDecrypt(std::string_view ciphertext, u8 key) noexcept -> std::string {
    return xorEncryptDecrypt(ciphertext, key);
}

}  // namespace atom::algorithm
