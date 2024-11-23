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

#include <cstdint>
#include <string>
#include <vector>

#include "atom/type/static_string.hpp"

namespace atom::algorithm {
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

}  // namespace detail

/**
 * @brief Encodes a vector of bytes into a Base32 string.
 *
 * @param data The input data to encode.
 * @return A Base32 encoded string.
 */
auto encodeBase32(const std::vector<uint8_t>& data) -> std::string;

/**
 * @brief Decodes a Base32 encoded string back into a vector of bytes.
 *
 * @param encoded The Base32 encoded string.
 * @return A vector of decoded bytes.
 */
auto decodeBase32(const std::string& encoded) -> std::vector<uint8_t>;

/**
 * @brief Encodes a string into a Base64 encoded string.
 *
 * This function converts the input string into its Base64 representation.
 *
 * @param input The input string to encode.
 * @return A Base64 encoded string.
 */
[[nodiscard("The result of base64Encode is not used.")]] auto base64Encode(
    std::string_view input) -> std::string;

/**
 * @brief Decodes a Base64 encoded string back into its original string.
 *
 * This function converts the Base64 encoded input string back to its original
 * form.
 *
 * @param input The Base64 encoded string to decode.
 * @return The decoded original string.
 */
[[nodiscard("The result of base64Decode is not used.")]] auto base64Decode(
    std::string_view input) -> std::string;

/**
 * @brief Encrypts a string using the XOR algorithm.
 *
 * This function applies an XOR operation on each character of the input string
 * with the provided key.
 *
 * @param plaintext The input string to encrypt.
 * @param key The encryption key.
 * @return The encrypted string.
 */
[[nodiscard("The result of xorEncrypt is not used.")]] auto xorEncrypt(
    std::string_view plaintext, uint8_t key) -> std::string;

/**
 * @brief Decrypts a string using the XOR algorithm.
 *
 * This function reverses the XOR encryption on the input string using the
 * provided key.
 *
 * @param ciphertext The encrypted string to decrypt.
 * @param key The decryption key.
 * @return The decrypted string.
 */
[[nodiscard("The result of xorDecrypt is not used.")]] auto xorDecrypt(
    std::string_view ciphertext, uint8_t key) -> std::string;

namespace detail {

/**
 * @brief Converts a Base64 character to its corresponding numeric value.
 *
 * @param ch The Base64 character to convert.
 * @return The numeric value of the Base64 character.
 */
constexpr auto convertChar(char const ch);

/**
 * @brief Converts a numeric value to its corresponding Base64 character.
 *
 * @param num The numeric value to convert.
 * @return The corresponding Base64 character.
 */
constexpr auto convertNumber(char const num);

}  // namespace detail

/**
 * @brief Decodes a compile-time constant Base64 string.
 *
 * This template function decodes a Base64 encoded string known at compile time.
 *
 * @tparam string A StaticString representing the Base64 encoded string.
 * @return A StaticString containing the decoded bytes.
 */
template <StaticString string>
constexpr auto decode() {
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
        result[j + 1] = bytes[1];
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
[[nodiscard("The result of isBase64 is not used.")]] auto isBase64(
    const std::string& str) -> bool;

}  // namespace atom::algorithm

#endif