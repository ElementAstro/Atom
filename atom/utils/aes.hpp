/*
 * aes.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Simple implementation of AES encryption

**************************************************/

#ifndef ATOM_UTILS_AES_HPP
#define ATOM_UTILS_AES_HPP

#include <string>
#include <vector>

namespace atom::utils {
/**
 * @brief Encrypts the input plaintext using the AES algorithm.
 *
 * @param plaintext The plaintext data to be encrypted
 * @param key The encryption key
 * @return The encrypted ciphertext
 */
[[nodiscard]] auto encryptAES(std::string_view plaintext, std::string_view key,
                              std::vector<unsigned char> &iv,
                              std::vector<unsigned char> &tag) -> std::string;

/**
 * @brief Decrypts the input ciphertext using the AES algorithm.
 *
 * @param ciphertext The ciphertext data to be decrypted
 * @param key The decryption key
 * @return The decrypted plaintext
 */
[[nodiscard]] auto decryptAES(std::string_view ciphertext, std::string_view key,
                              std::vector<unsigned char> &iv,
                              std::vector<unsigned char> &tag) -> std::string;

/**
 * @brief Compresses the input data using the Zlib library.
 *
 * @param data The data to be compressed
 * @return The compressed data
 */
[[nodiscard]] auto compress(std::string_view data) -> std::string;

/**
 * @brief Decompresses the input data using the Zlib library.
 *
 * @param data The data to be decompressed
 * @return The decompressed data
 */
[[nodiscard]] auto decompress(std::string_view data) -> std::string;

/**
 * @brief Calculates the SHA-256 hash of a file.
 *
 * @param filename The name of the file
 * @return The SHA-256 hash of the file
 */
[[nodiscard]] auto calculateSha256(std::string_view filename) -> std::string;

/**
 * @brief Calculates the SHA-224 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-224 hash of the string
 */
[[nodiscard]] auto calculateSha224(const std::string &data) -> std::string;

/**
 * @brief Calculates the SHA-384 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-384 hash of the string
 */
[[nodiscard]] auto calculateSha384(const std::string &data) -> std::string;

/**
 * @brief Calculates the SHA-512 hash of a string.
 *
 * @param data The string to be hashed
 * @return The SHA-512 hash of the string
 */
[[nodiscard]] auto calculateSha512(const std::string &data) -> std::string;
}  // namespace atom::utils

#endif
