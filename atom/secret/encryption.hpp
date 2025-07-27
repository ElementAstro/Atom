#ifndef ATOM_SECRET_ENCRYPTION_HPP
#define ATOM_SECRET_ENCRYPTION_HPP

#include <openssl/evp.h>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

#include "result.hpp"
#include "common.hpp"

namespace atom::secret {

/**
 * @brief Hardware acceleration detection result.
 */
struct HardwareCapabilities {
    bool aesni_available = false;      ///< AES-NI instruction set available
    bool avx2_available = false;       ///< AVX2 instruction set available
    bool sha_available = false;        ///< SHA instruction set available
    bool rdrand_available = false;     ///< RDRAND instruction available
};

/**
 * @brief Memory pool for frequent cryptographic operations.
 */
class SecureMemoryPool {
public:
    static SecureMemoryPool& getInstance();

    /**
     * @brief Get a buffer from the pool.
     * @param size Required buffer size.
     * @return Unique pointer to the buffer.
     */
    std::unique_ptr<std::vector<unsigned char>> getBuffer(size_t size);

    /**
     * @brief Return a buffer to the pool.
     * @param buffer Buffer to return.
     */
    void returnBuffer(std::unique_ptr<std::vector<unsigned char>> buffer);

private:
    SecureMemoryPool() = default;
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Provides high-level cryptographic operations with multiple algorithms and optimizations.
 */
class Encryption {
public:
    /**
     * @brief Initialize the encryption system and detect hardware capabilities.
     */
    static void initialize();

    /**
     * @brief Get detected hardware capabilities.
     * @return HardwareCapabilities structure.
     */
    static const HardwareCapabilities& getHardwareCapabilities();

    /**
     * @brief Derives a key from a password using PBKDF2.
     * @param password The password.
     * @param salt The salt.
     * @param iterations Number of PBKDF2 iterations.
     * @param key_len The desired key length.
     * @return Result containing the derived key or error.
     */
    static Result<std::vector<unsigned char>> deriveKey(
        std::string_view password,
        std::string_view salt,
        int iterations = 100000,
        int key_len = 32);

    /**
     * @brief Encrypts data using the specified encryption method.
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @param aad Additional authenticated data.
     * @param options Encryption options.
     * @return A Result containing the ciphertext (with appended tag for AEAD),
     * or an error.
     */
    static Result<std::vector<unsigned char>> encrypt(
        std::string_view plaintext,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad,
        const EncryptionOptions& options = EncryptionOptions{});

    /**
     * @brief Decrypts data using the specified encryption method.
     * @param ciphertext_with_tag The data to decrypt (with appended tag for AEAD).
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @param aad Additional authenticated data.
     * @param options Encryption options.
     * @return A Result containing the plaintext, or an error.
     */
    static Result<std::string> decrypt(
        const std::vector<unsigned char>& ciphertext_with_tag,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad,
        const EncryptionOptions& options = EncryptionOptions{});

    /**
     * @brief Generates a cryptographically secure random byte sequence.
     * @param len The number of bytes to generate.
     * @param use_hardware_rng Use hardware RNG if available.
     * @return Result containing random bytes or error.
     */
    static Result<std::vector<unsigned char>> randomBytes(
        int len,
        bool use_hardware_rng = true);

    /**
     * @brief Legacy method for backward compatibility.
     */
    static std::vector<unsigned char> random_bytes(int len);

    /**
     * @brief Legacy method for backward compatibility.
     */
    static std::vector<unsigned char> derive_key(std::string_view password,
                                                 std::string_view salt,
                                                 int key_len = 32);

private:
    static HardwareCapabilities detectHardwareCapabilities();
    static const EVP_CIPHER* getCipher(EncryptionOptions::Method method, int keySize);
    static Result<std::vector<unsigned char>> encryptAESGCM(
        std::string_view plaintext,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);
    static Result<std::vector<unsigned char>> encryptAESCBC(
        std::string_view plaintext,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv);
    static Result<std::vector<unsigned char>> encryptChaCha20Poly1305(
        std::string_view plaintext,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);

    static Result<std::string> decryptAESGCM(
        const std::vector<unsigned char>& ciphertext_with_tag,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);
    static Result<std::string> decryptAESCBC(
        const std::vector<unsigned char>& ciphertext,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv);
    static Result<std::string> decryptChaCha20Poly1305(
        const std::vector<unsigned char>& ciphertext_with_tag,
        const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv,
        const std::vector<unsigned char>& aad);

    static HardwareCapabilities s_hwCapabilities;
    static bool s_initialized;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_ENCRYPTION_HPP
