/*
 * base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: A collection of algorithms for C++

**************************************************/

#ifndef ATOM_ALGORITHM_BASE16_HPP
#define ATOM_ALGORITHM_BASE16_HPP

#include <concepts>
#include <cstdint>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "atom/type/expected.hpp"
#include "atom/type/static_string.hpp"

namespace atom::algorithm {

using Error = std::string;

namespace detail {
/**
 * @brief Base64 character set.
 */
constexpr std::string_view BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

/**
 * @brief Number of Base64 characters.
 */
constexpr size_t BASE64_CHAR_COUNT = 64;

/**
 * @brief Mask for extracting 6 bits.
 */
constexpr uint8_t MASK_6_BITS = 0x3F;

/**
 * @brief Mask for extracting 4 bits.
 */
constexpr uint8_t MASK_4_BITS = 0x0F;

/**
 * @brief Mask for extracting 2 bits.
 */
constexpr uint8_t MASK_2_BITS = 0x03;

/**
 * @brief Mask for extracting 8 bits.
 */
constexpr uint8_t MASK_8_BITS = 0xFC;

/**
 * @brief Mask for extracting 12 bits.
 */
constexpr uint8_t MASK_12_BITS = 0xF0;

/**
 * @brief Mask for extracting 14 bits.
 */
constexpr uint8_t MASK_14_BITS = 0xC0;

/**
 * @brief Mask for extracting 16 bits.
 */
constexpr uint8_t MASK_16_BITS = 0x30;

/**
 * @brief Mask for extracting 18 bits.
 */
constexpr uint8_t MASK_18_BITS = 0x3C;

/**
 * @brief Converts a Base64 character to its corresponding value.
 *
 * @param ch The Base64 character to convert.
 * @return The numeric value of the Base64 character.
 */
constexpr auto convertChar(char const ch) {
    return ch >= 'A' && ch <= 'Z'   ? ch - 'A'
           : ch >= 'a' && ch <= 'z' ? ch - 'a' + 26
           : ch >= '0' && ch <= '9' ? ch - '0' + 52
           : ch == '+'              ? 62
                                    : 63;
}

/**
 * @brief Converts a numeric value to its corresponding Base64 character.
 *
 * @param num The numeric value to convert.
 * @return The corresponding Base64 character.
 */
constexpr auto convertNumber(char const num) {
    return num < 26    ? static_cast<char>(num + 'A')
           : num < 52  ? static_cast<char>(num - 26 + 'a')
           : num < 62  ? static_cast<char>(num - 52 + '0')
           : num == 62 ? '+'
                       : '/';
}

constexpr bool isValidBase64Char(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

// 使用concept约束输入类型
template <typename T>
concept ByteContainer =
    std::ranges::contiguous_range<T> && requires(T container) {
        { container.data() } -> std::convertible_to<const std::byte*>;
        { container.size() } -> std::convertible_to<std::size_t>;
    };

}  // namespace detail

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

/**
 * @brief Encodes a string into a Base64 encoded string.
 *
 * @param input The input string to encode
 * @param padding Whether to add padding characters (=) to the output
 * @return atom::type::expected<std::string> Encoded string or error
 */
[[nodiscard]] auto base64Encode(std::string_view input,
                                bool padding = true) noexcept
    -> atom::type::expected<std::string>;

/**
 * @brief Decodes a Base64 encoded string back into its original form.
 *
 * @param input The Base64 encoded string to decode
 * @return atom::type::expected<std::string> Decoded string or error
 */
[[nodiscard]] auto base64Decode(std::string_view input) noexcept
    -> atom::type::expected<std::string>;

/**
 * @brief Encrypts a string using the XOR algorithm.
 *
 * @param plaintext The input string to encrypt
 * @param key The encryption key
 * @return std::string The encrypted string
 */
[[nodiscard]] auto xorEncrypt(std::string_view plaintext,
                              uint8_t key) noexcept -> std::string;

/**
 * @brief Decrypts a string using the XOR algorithm.
 *
 * @param ciphertext The encrypted string to decrypt
 * @param key The decryption key
 * @return std::string The decrypted string
 */
[[nodiscard]] auto xorDecrypt(std::string_view ciphertext,
                              uint8_t key) noexcept -> std::string;

/**
 * @brief Decodes a compile-time constant Base64 string.
 *
 * @tparam string A StaticString representing the Base64 encoded string
 * @return StaticString containing the decoded bytes or empty if invalid
 */
template <StaticString string>
consteval auto decodeBase64() {
    // 验证输入是否为有效的Base64
    constexpr bool valid = [&]() {
        for (size_t i = 0; i < string.size(); ++i) {
            if (!detail::isValidBase64Char(string[i])) {
                return false;
            }
        }
        return string.size() % 4 == 0;
    }();

    if constexpr (!valid) {
        return StaticString<0>{};
    }

    constexpr auto STRING_SIZE = string.size();
    constexpr auto PADDING_POS = std::ranges::find(string.buf, '=');
    constexpr auto DECODED_SIZE = ((PADDING_POS - string.buf.data()) * 3) / 4;

    StaticString<DECODED_SIZE> result;

    for (std::size_t i = 0, j = 0; i < STRING_SIZE; i += 4, j += 3) {
        char bytes[3] = {
            static_cast<char>(detail::convertChar(string[i]) << 2 |
                              detail::convertChar(string[i + 1]) >> 4),
            static_cast<char>(detail::convertChar(string[i + 1]) << 4 |
                              detail::convertChar(string[i + 2]) >> 2),
            static_cast<char>(detail::convertChar(string[i + 2]) << 6 |
                              detail::convertChar(string[i + 3]))};
        result[j] = bytes[0];
        if (string[i + 2] != '=') {
            result[j + 1] = bytes[1];
        }
        if (string[i + 3] != '=') {
            result[j + 2] = bytes[2];
        }
    }
    return result;
}

/**
 * @brief Encodes a compile-time constant string into Base64.
 *
 * This template function encodes a string known at compile time into its Base64
 * representation.
 *
 * @tparam string A StaticString representing the input string to encode.
 * @return A StaticString containing the Base64 encoded string.
 */
template <StaticString string>
constexpr auto encode() {
    constexpr auto STRING_SIZE = string.size();
    constexpr auto RESULT_SIZE_NO_PADDING = (STRING_SIZE * 4 + 2) / 3;
    constexpr auto RESULT_SIZE = (RESULT_SIZE_NO_PADDING + 3) & ~3;
    constexpr auto PADDING_SIZE = RESULT_SIZE - RESULT_SIZE_NO_PADDING;

    StaticString<RESULT_SIZE> result;
    for (std::size_t i = 0, j = 0; i < STRING_SIZE; i += 3, j += 4) {
        char bytes[4] = {
            static_cast<char>(string[i] >> 2),
            static_cast<char>((string[i] & 0x03) << 4 | string[i + 1] >> 4),
            static_cast<char>((string[i + 1] & 0x0F) << 2 | string[i + 2] >> 6),
            static_cast<char>(string[i + 2] & 0x3F)};
        std::ranges::transform(bytes, bytes + 4, result.buf.begin() + j,
                               detail::convertNumber);
    }
    std::fill_n(result.buf.data() + RESULT_SIZE_NO_PADDING, PADDING_SIZE, '=');
    return result;
}

/**
 * @brief Checks if a given string is a valid Base64 encoded string.
 *
 * This function verifies whether the input string conforms to the Base64
 * encoding standards.
 *
 * @param str The string to validate.
 * @return true If the string is a valid Base64 encoded string.
 * @return false Otherwise.
 */
[[nodiscard]] auto isBase64(std::string_view str) noexcept -> bool;

/**
 * @brief 基于指定线程数的并行算法执行器
 *
 * @param data 要处理的数据
 * @param threadCount 线程数量（0表示使用硬件支持的线程数）
 * @param func 每个线程执行的函数
 */
template <typename T, std::invocable<std::span<T>> Func>
void parallelExecute(std::span<T> data, size_t threadCount, Func func) noexcept;

}  // namespace atom::algorithm

#endif