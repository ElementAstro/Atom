#ifndef ATOM_SECRET_CRYPTO_ENCRYPTION_HPP
#define ATOM_SECRET_CRYPTO_ENCRYPTION_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Supported encryption algorithms.
 */
enum class EncryptionAlgorithm {
    AES_128_GCM,        ///< AES-128 in GCM mode (AEAD)
    AES_256_GCM,        ///< AES-256 in GCM mode (AEAD) - recommended
    AES_128_CBC,        ///< AES-128 in CBC mode
    AES_256_CBC,        ///< AES-256 in CBC mode
    ChaCha20_Poly1305,  ///< ChaCha20-Poly1305 (AEAD)
};

/**
 * @brief Encryption options and parameters.
 */
struct EncryptionParams {
    EncryptionAlgorithm algorithm = EncryptionAlgorithm::AES_256_GCM;
    int keyIterations =
        100000;  ///< PBKDF2 iterations for password-based encryption
    bool useHardwareAcceleration = true;

    static EncryptionParams defaults() { return EncryptionParams{}; }
    static EncryptionParams highSecurity() {
        return EncryptionParams{.algorithm = EncryptionAlgorithm::AES_256_GCM,
                                .keyIterations = 600000,
                                .useHardwareAcceleration = true};
    }
};

/**
 * @brief Container for encrypted data with all necessary metadata.
 */
struct EncryptedData {
    std::vector<uint8_t> ciphertext;  ///< The encrypted data
    std::vector<uint8_t> iv;          ///< Initialization vector / nonce
    std::vector<uint8_t> salt;  ///< Salt for key derivation (if password-based)
    std::vector<uint8_t> tag;   ///< Authentication tag (for AEAD modes)
    EncryptionAlgorithm algorithm;  ///< Algorithm used
    int keyIterations;              ///< Key derivation iterations used

    /**
     * @brief Serializes the encrypted data to binary format.
     * @return Serialized binary data.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserializes encrypted data from binary format.
     * @param data Serialized data.
     * @return Result containing EncryptedData or error.
     */
    static Result<EncryptedData> deserialize(const std::vector<uint8_t>& data);

    /**
     * @brief Checks if this is AEAD encrypted data.
     * @return True if AEAD mode was used.
     */
    bool isAead() const noexcept;
};

/**
 * @brief High-level encryption and decryption operations.
 */
class Encryption {
public:
    // ========================================================================
    // Password-based encryption
    // ========================================================================

    /**
     * @brief Encrypts data using a password.
     *
     * The password is used to derive an encryption key using PBKDF2.
     * A random salt and IV are generated automatically.
     *
     * @param plaintext Data to encrypt.
     * @param password Password for encryption.
     * @param params Encryption parameters.
     * @return Result containing encrypted data or error.
     */
    static Result<EncryptedData> encrypt(
        std::string_view plaintext, std::string_view password,
        const EncryptionParams& params = EncryptionParams::defaults());

    /**
     * @brief Encrypts binary data using a password.
     * @param plaintext Binary data to encrypt.
     * @param password Password for encryption.
     * @param params Encryption parameters.
     * @return Result containing encrypted data or error.
     */
    static Result<EncryptedData> encrypt(
        const std::vector<uint8_t>& plaintext, std::string_view password,
        const EncryptionParams& params = EncryptionParams::defaults());

    /**
     * @brief Decrypts data using a password.
     * @param encryptedData Encrypted data to decrypt.
     * @param password Password for decryption.
     * @return Result containing decrypted string or error.
     */
    static Result<std::string> decrypt(const EncryptedData& encryptedData,
                                       std::string_view password);

    /**
     * @brief Decrypts data to binary using a password.
     * @param encryptedData Encrypted data to decrypt.
     * @param password Password for decryption.
     * @return Result containing decrypted bytes or error.
     */
    static Result<std::vector<uint8_t>> decryptBytes(
        const EncryptedData& encryptedData, std::string_view password);

    // ========================================================================
    // Key-based encryption
    // ========================================================================

    /**
     * @brief Encrypts data using a pre-derived key.
     * @param plaintext Data to encrypt.
     * @param key Encryption key (must be correct size for algorithm).
     * @param params Encryption parameters.
     * @return Result containing encrypted data or error.
     */
    static Result<EncryptedData> encryptWithKey(
        std::string_view plaintext, const std::vector<uint8_t>& key,
        const EncryptionParams& params = EncryptionParams::defaults());

    /**
     * @brief Encrypts binary data using a pre-derived key.
     * @param plaintext Binary data to encrypt.
     * @param key Encryption key.
     * @param params Encryption parameters.
     * @return Result containing encrypted data or error.
     */
    static Result<EncryptedData> encryptWithKey(
        const std::vector<uint8_t>& plaintext, const std::vector<uint8_t>& key,
        const EncryptionParams& params = EncryptionParams::defaults());

    /**
     * @brief Decrypts data using a pre-derived key.
     * @param encryptedData Encrypted data to decrypt.
     * @param key Decryption key.
     * @return Result containing decrypted string or error.
     */
    static Result<std::string> decryptWithKey(
        const EncryptedData& encryptedData, const std::vector<uint8_t>& key);

    /**
     * @brief Decrypts data to binary using a pre-derived key.
     * @param encryptedData Encrypted data to decrypt.
     * @param key Decryption key.
     * @return Result containing decrypted bytes or error.
     */
    static Result<std::vector<uint8_t>> decryptBytesWithKey(
        const EncryptedData& encryptedData, const std::vector<uint8_t>& key);

    // ========================================================================
    // Utility functions
    // ========================================================================

    /**
     * @brief Gets the required key size for an algorithm.
     * @param algorithm Encryption algorithm.
     * @return Key size in bytes.
     */
    static size_t getKeySize(EncryptionAlgorithm algorithm) noexcept;

    /**
     * @brief Gets the required IV/nonce size for an algorithm.
     * @param algorithm Encryption algorithm.
     * @return IV size in bytes.
     */
    static size_t getIvSize(EncryptionAlgorithm algorithm) noexcept;

    /**
     * @brief Gets the authentication tag size for AEAD algorithms.
     * @param algorithm Encryption algorithm.
     * @return Tag size in bytes (0 for non-AEAD).
     */
    static size_t getTagSize(EncryptionAlgorithm algorithm) noexcept;

    /**
     * @brief Checks if an algorithm uses AEAD mode.
     * @param algorithm Encryption algorithm.
     * @return True if AEAD.
     */
    static bool isAead(EncryptionAlgorithm algorithm) noexcept;

    /**
     * @brief Checks if an algorithm is available.
     * @param algorithm Encryption algorithm.
     * @return True if available.
     */
    static bool isAvailable(EncryptionAlgorithm algorithm) noexcept;

    /**
     * @brief Gets algorithm name as string.
     * @param algorithm Encryption algorithm.
     * @return Algorithm name.
     */
    static std::string getAlgorithmName(EncryptionAlgorithm algorithm) noexcept;

private:
    static Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
    encryptAesGcm(const uint8_t* plaintext, size_t plaintextLen,
                  const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& iv);

    static Result<std::vector<uint8_t>> decryptAesGcm(
        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv, const std::vector<uint8_t>& tag);

    static Result<std::vector<uint8_t>> encryptAesCbc(
        const uint8_t* plaintext, size_t plaintextLen,
        const std::vector<uint8_t>& key, const std::vector<uint8_t>& iv);

    static Result<std::vector<uint8_t>> decryptAesCbc(
        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv);

    static Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
    encryptChaCha20Poly1305(const uint8_t* plaintext, size_t plaintextLen,
                            const std::vector<uint8_t>& key,
                            const std::vector<uint8_t>& nonce);

    static Result<std::vector<uint8_t>> decryptChaCha20Poly1305(
        const std::vector<uint8_t>& ciphertext, const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& nonce, const std::vector<uint8_t>& tag);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CRYPTO_ENCRYPTION_HPP
