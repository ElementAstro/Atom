/*
 * xor_cipher.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: XOR encryption/decryption algorithms

**************************************************/

#ifndef ATOM_ALGORITHM_CRYPTO_XOR_CIPHER_HPP
#define ATOM_ALGORITHM_CRYPTO_XOR_CIPHER_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace atom::algorithm {

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

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_XOR_CIPHER_HPP
