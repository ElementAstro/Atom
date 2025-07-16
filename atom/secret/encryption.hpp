#ifndef ATOM_SECRET_ENCRYPTION_HPP
#define ATOM_SECRET_ENCRYPTION_HPP

#include <openssl/evp.h>
#include <string>
#include <string_view>
#include <vector>

#include "result.hpp"

namespace atom::secret {

/**
 * @brief Provides high-level cryptographic operations.
 */
class Encryption {
public:
    /**
     * @brief Derives a key from a password using PBKDF2.
     * @param password The password.
     * @param salt The salt.
     * @param key_len The desired key length.
     * @return The derived key.
     */
    static std::vector<unsigned char> derive_key(std::string_view password,
                                                 std::string_view salt,
                                                 int key_len = 32);

    /**
     * @brief Encrypts data using AES-256-GCM.
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @param aad Additional authenticated data.
     * @return A Result containing the ciphertext (with appended 16-byte tag),
     * or an error string.
     */
    static Result<std::vector<unsigned char>> encrypt(
        std::string_view plaintext, const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);

    /**
     * @brief Decrypts data using AES-256-GCM.
     * @param ciphertext_with_tag The data to decrypt (with appended 16-byte
     * tag).
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @param aad Additional authenticated data.
     * @return A Result containing the plaintext, or an error string.
     */
    static Result<std::string> decrypt(
        const std::vector<unsigned char>& ciphertext_with_tag,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);

    /**
     * @brief Generates a random byte sequence.
     * @param len The number of bytes to generate.
     * @return A vector of random bytes.
     */
    static std::vector<unsigned char> random_bytes(int len);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_ENCRYPTION_HPP
