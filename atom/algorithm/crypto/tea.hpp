#ifndef ATOM_ALGORITHM_CRYPTO_TEA_HPP
#define ATOM_ALGORITHM_CRYPTO_TEA_HPP

#include <array>

#include "tea_common.hpp"

namespace atom::algorithm {

/**
 * @brief Encrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).
 *
 * The TEA algorithm is a symmetric-key block cipher known for its simplicity.
 * This function encrypts two 32-bit unsigned integers using a 128-bit key.
 *
 * @param value0 The first 32-bit value to be encrypted (modified in place).
 * @param value1 The second 32-bit value to be encrypted (modified in place).
 * @param key A reference to an array of four 32-bit unsigned integers
 * representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto teaEncrypt(u32 &value0, u32 &value1,
                const std::array<u32, 4> &key) noexcept(false) -> void;

/**
 * @brief Decrypts two 32-bit values using the TEA (Tiny Encryption Algorithm).
 *
 * This function decrypts two 32-bit unsigned integers using a 128-bit key.
 *
 * @param value0 The first 32-bit value to be decrypted (modified in place).
 * @param value1 The second 32-bit value to be decrypted (modified in place).
 * @param key A reference to an array of four 32-bit unsigned integers
 * representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto teaDecrypt(u32 &value0, u32 &value1,
                const std::array<u32, 4> &key) noexcept(false) -> void;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_TEA_HPP
