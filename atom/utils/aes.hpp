/*
 * aes.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_UTILS_AES_HPP
#define ATOM_UTILS_AES_HPP

#include <concepts>
#include <span>
#include <string>
#include <vector>

namespace atom::utils {

template <typename T>
concept StringLike = requires(T t) {
    { std::string_view(t) } -> std::convertible_to<std::string_view>;
};

/**
 * @brief Encrypts the input plaintext using the AES algorithm.
 *
 * @param plaintext The plaintext data to be encrypted
 * @param key The encryption key
 * @param iv Output vector for initialization vector
 * @param tag Output vector for authentication tag
 * @return The encrypted ciphertext
 * @throws std::invalid_argument If inputs are invalid
 * @throws std::runtime_error If encryption fails
 */
[[nodiscard]] auto encryptAES(StringLike auto&& plaintext,
                              StringLike auto&& key,
                              std::vector<unsigned char>& iv,
                              std::vector<unsigned char>& tag) -> std::string;

/**
 * @brief Decrypts the input ciphertext using the AES algorithm.
 *
 * @param ciphertext The ciphertext data to be decrypted
 * @param key The decryption key
 * @param iv Initialization vector used during encryption
 * @param tag Authentication tag from encryption
 * @return The decrypted plaintext
 * @throws std::invalid_argument If inputs are invalid
 * @throws std::runtime_error If decryption fails
 */
[[nodiscard]] auto decryptAES(StringLike auto&& ciphertext,
                              StringLike auto&& key,
                              std::span<const unsigned char> iv,
                              std::span<const unsigned char> tag)
    -> std::string;

/**
 * @brief Compresses the input data using the Zlib library.
 *
 * @param data The data to be compressed
 * @return The compressed data
 * @throws std::invalid_argument If input is empty
 * @throws std::runtime_error If compression fails
 */
[[nodiscard]] auto compress(StringLike auto&& data) -> std::string;

/**
 * @brief Decompresses the input data using the Zlib library.
 *
 * @param data The data to be decompressed
 * @return The decompressed data
 * @throws std::invalid_argument If input is empty
 * @throws std::runtime_error If decompression fails
 */
[[nodiscard]] auto decompress(StringLike auto&& data) -> std::string;

/**
 * @brief Calculates the SHA-256 hash of a file.
 *
 * @param filename The name of the file
 * @return The SHA-256 hash of the file, empty string if file doesn't exist
 * @throws std::runtime_error If hash calculation fails
 */
[[nodiscard]] auto calculateSha256(StringLike auto&& filename) -> std::string;

/**
 * @brief Calculates the SHA-224 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-224 hash of the string
 * @throws std::invalid_argument If input is empty
 * @throws std::runtime_error If hash calculation fails
 */
[[nodiscard]] auto calculateSha224(const std::string& data) noexcept
    -> std::string;

/**
 * @brief Calculates the SHA-384 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-384 hash of the string
 * @throws std::invalid_argument If input is empty
 * @throws std::runtime_error If hash calculation fails
 */
[[nodiscard]] auto calculateSha384(const std::string& data) noexcept
    -> std::string;

/**
 * @brief Calculates the SHA-512 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-512 hash of the string
 * @throws std::invalid_argument If input is empty
 * @throws std::runtime_error If hash calculation fails
 */
[[nodiscard]] auto calculateSha512(const std::string& data) noexcept
    -> std::string;

}  // namespace atom::utils

#endif
