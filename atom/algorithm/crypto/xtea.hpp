#ifndef ATOM_ALGORITHM_CRYPTO_XTEA_HPP
#define ATOM_ALGORITHM_CRYPTO_XTEA_HPP

#include "tea_common.hpp"

namespace atom::algorithm {

/**
 * @brief Encrypts two 32-bit values using the XTEA (Extended TEA) algorithm.
 *
 * XTEA is a block cipher that corrects some weaknesses of TEA.
 *
 * @param value0 The first 32-bit value to be encrypted (modified in place).
 * @param value1 The second 32-bit value to be encrypted (modified in place).
 * @param key A reference to an XTEAKey representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto xteaEncrypt(u32 &value0, u32 &value1, const XTEAKey &key) noexcept(false)
    -> void;

/**
 * @brief Decrypts two 32-bit values using the XTEA (Extended TEA) algorithm.
 *
 * @param value0 The first 32-bit value to be decrypted (modified in place).
 * @param value1 The second 32-bit value to be decrypted (modified in place).
 * @param key A reference to an XTEAKey representing the 128-bit key.
 * @throws TEAException if the key is invalid.
 */
auto xteaDecrypt(u32 &value0, u32 &value1, const XTEAKey &key) noexcept(false)
    -> void;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CRYPTO_XTEA_HPP
