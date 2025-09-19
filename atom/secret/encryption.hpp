#ifndef ATOM_SECRET_ENCRYPTION_HPP
#define ATOM_SECRET_ENCRYPTION_HPP

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "common.hpp"
#include "result.hpp"

namespace atom::secret {

// Forward declaration for OpenSSL context
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;

/**
 * @brief RAII wrapper for OpenSSL EVP_CIPHER_CTX.
 * Ensures the context is properly freed.
 */
class SslCipherContext {
private:
    EVP_CIPHER_CTX* ctx;  ///< Pointer to the OpenSSL cipher context.

public:
    /**
     * @brief Constructs an SslCipherContext, creating a new EVP_CIPHER_CTX.
     * @throws std::runtime_error if context creation fails.
     */
    SslCipherContext();

    /**
     * @brief Destroys the SslCipherContext, freeing the EVP_CIPHER_CTX.
     */
    ~SslCipherContext();

    // Disable copy construction and assignment
    SslCipherContext(const SslCipherContext&) = delete;
    SslCipherContext& operator=(const SslCipherContext&) = delete;

    // Enable move construction and assignment
    SslCipherContext(SslCipherContext&& other) noexcept;
    SslCipherContext& operator=(SslCipherContext&& other) noexcept;

    /**
     * @brief Gets the raw pointer to the EVP_CIPHER_CTX.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    EVP_CIPHER_CTX* get() const noexcept { return ctx; }

    /**
     * @brief Implicit conversion to the raw EVP_CIPHER_CTX pointer.
     * @return The raw EVP_CIPHER_CTX pointer.
     */
    operator EVP_CIPHER_CTX*() const noexcept { return ctx; }
};

/**
 * @brief Secure memory management utilities for sensitive data.
 */
class SecureMemory {
public:
    /**
     * @brief Securely clears memory by overwriting with random data.
     * @param ptr Pointer to memory to clear.
     * @param size Size of memory to clear.
     */
    static void secureClear(void* ptr, size_t size) noexcept;

    /**
     * @brief Securely clears a string's contents.
     * @param str String to clear.
     */
    static void secureClear(std::string& str) noexcept;

    /**
     * @brief Securely clears a vector's contents.
     * @tparam T Type of vector elements.
     * @param vec Vector to clear.
     */
    template<typename T>
    static void secureClear(std::vector<T>& vec) noexcept;

    /**
     * @brief Locks memory pages to prevent swapping to disk.
     * @param ptr Pointer to memory to lock.
     * @param size Size of memory to lock.
     * @return True if successful, false otherwise.
     */
    static bool lockMemory(void* ptr, size_t size) noexcept;

    /**
     * @brief Unlocks previously locked memory pages.
     * @param ptr Pointer to memory to unlock.
     * @param size Size of memory to unlock.
     * @return True if successful, false otherwise.
     */
    static bool unlockMemory(void* ptr, size_t size) noexcept;

    /**
     * @brief Allocates secure memory that won't be swapped to disk.
     * @param size Size of memory to allocate.
     * @return Pointer to allocated memory or nullptr on failure.
     */
    static void* allocateSecure(size_t size) noexcept;

    /**
     * @brief Frees secure memory allocated with allocateSecure.
     * @param ptr Pointer to memory to free.
     * @param size Size of memory to free.
     */
    static void freeSecure(void* ptr, size_t size) noexcept;
};

/**
 * @brief RAII wrapper for secure memory allocation.
 */
template<typename T>
class SecureBuffer {
private:
    T* data_;
    size_t size_;

public:
    /**
     * @brief Constructs a secure buffer of the specified size.
     * @param size Number of elements to allocate.
     */
    explicit SecureBuffer(size_t size);

    /**
     * @brief Destructor that securely clears and frees memory.
     */
    ~SecureBuffer();

    // Disable copy construction and assignment
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // Enable move construction and assignment
    SecureBuffer(SecureBuffer&& other) noexcept;
    SecureBuffer& operator=(SecureBuffer&& other) noexcept;

    /**
     * @brief Gets pointer to the buffer data.
     * @return Pointer to buffer data.
     */
    T* data() noexcept { return data_; }

    /**
     * @brief Gets const pointer to the buffer data.
     * @return Const pointer to buffer data.
     */
    const T* data() const noexcept { return data_; }

    /**
     * @brief Gets the buffer size.
     * @return Number of elements in the buffer.
     */
    size_t size() const noexcept { return size_; }

    /**
     * @brief Array access operator.
     * @param index Index of element to access.
     * @return Reference to element at index.
     */
    T& operator[](size_t index) noexcept { return data_[index]; }

    /**
     * @brief Const array access operator.
     * @param index Index of element to access.
     * @return Const reference to element at index.
     */
    const T& operator[](size_t index) const noexcept { return data_[index]; }

    /**
     * @brief Checks if the buffer is valid.
     * @return True if buffer is allocated, false otherwise.
     */
    bool isValid() const noexcept { return data_ != nullptr; }
};

/**
 * @brief Cryptographic key derivation and management.
 */
class KeyDerivation {
public:
    /**
     * @brief Derives a key from a password using PBKDF2.
     * @param password The password to derive from.
     * @param salt The salt for key derivation.
     * @param iterations Number of PBKDF2 iterations.
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> deriveKey(
        std::string_view password,
        const std::vector<uint8_t>& salt,
        int iterations,
        size_t keyLength);

    /**
     * @brief Generates a cryptographically secure random salt.
     * @param length Length of salt in bytes.
     * @return Result containing the salt or error message.
     */
    static Result<std::vector<uint8_t>> generateSalt(size_t length = 32);

    /**
     * @brief Generates a cryptographically secure random key.
     * @param length Length of key in bytes.
     * @return Result containing the key or error message.
     */
    static Result<std::vector<uint8_t>> generateKey(size_t length = 32);
};

/**
 * @brief Encrypted data container with metadata.
 */
struct EncryptedData {
    std::vector<uint8_t> ciphertext;  ///< The encrypted data.
    std::vector<uint8_t> iv;          ///< Initialization vector.
    std::vector<uint8_t> salt;        ///< Salt used for key derivation.
    std::vector<uint8_t> tag;         ///< Authentication tag (for AEAD modes).
    EncryptionOptions::Method method; ///< Encryption method used.
    int keyIterations;                ///< PBKDF2 iterations used.

    /**
     * @brief Serializes the encrypted data to a binary format.
     * @return Serialized data.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserializes encrypted data from binary format.
     * @param data Serialized data.
     * @return Result containing EncryptedData or error message.
     */
    static Result<EncryptedData> deserialize(const std::vector<uint8_t>& data);
};

/**
 * @brief High-level encryption and decryption utilities.
 */
class Encryption {
public:
    /**
     * @brief Encrypts data using the specified options.
     * @param plaintext The data to encrypt.
     * @param password The password for encryption.
     * @param options Encryption options.
     * @return Result containing encrypted data or error message.
     */
    static Result<EncryptedData> encrypt(
        std::string_view plaintext,
        std::string_view password,
        const EncryptionOptions& options = {});

    /**
     * @brief Decrypts data using the provided password.
     * @param encryptedData The encrypted data to decrypt.
     * @param password The password for decryption.
     * @return Result containing decrypted plaintext or error message.
     */
    static Result<std::string> decrypt(
        const EncryptedData& encryptedData,
        std::string_view password);

    /**
     * @brief Encrypts data with a pre-derived key.
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param options Encryption options.
     * @return Result containing encrypted data or error message.
     */
    static Result<EncryptedData> encryptWithKey(
        std::string_view plaintext,
        const std::vector<uint8_t>& key,
        const EncryptionOptions& options = {});

    /**
     * @brief Decrypts data with a pre-derived key.
     * @param encryptedData The encrypted data to decrypt.
     * @param key The decryption key.
     * @return Result containing decrypted plaintext or error message.
     */
    static Result<std::string> decryptWithKey(
        const EncryptedData& encryptedData,
        const std::vector<uint8_t>& key);

private:
    /**
     * @brief Encrypts using AES-GCM.
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @return Result containing encrypted data and tag or error message.
     */
    static Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
        encryptAesGcm(
            std::string_view plaintext,
            const std::vector<uint8_t>& key,
            const std::vector<uint8_t>& iv);

    /**
     * @brief Decrypts using AES-GCM.
     * @param ciphertext The encrypted data.
     * @param key The decryption key.
     * @param iv The initialization vector.
     * @param tag The authentication tag.
     * @return Result containing decrypted plaintext or error message.
     */
    static Result<std::string> decryptAesGcm(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& tag);

    /**
     * @brief Encrypts using AES-CBC.
     * @param plaintext The data to encrypt.
     * @param key The encryption key.
     * @param iv The initialization vector.
     * @return Result containing encrypted data or error message.
     */
    static Result<std::vector<uint8_t>> encryptAesCbc(
        std::string_view plaintext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv);

    /**
     * @brief Decrypts using AES-CBC.
     * @param ciphertext The encrypted data.
     * @param key The decryption key.
     * @param iv The initialization vector.
     * @return Result containing decrypted plaintext or error message.
     */
    static Result<std::string> decryptAesCbc(
        const std::vector<uint8_t>& ciphertext,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv);

    /**
     * @brief Gets the OpenSSL cipher for the specified method.
     * @param method The encryption method.
     * @return Pointer to the EVP_CIPHER or nullptr if unsupported.
     */
    static const EVP_CIPHER* getCipher(EncryptionOptions::Method method);

    /**
     * @brief Gets the required key size for the specified method.
     * @param method The encryption method.
     * @return Key size in bytes.
     */
    static size_t getKeySize(EncryptionOptions::Method method);

    /**
     * @brief Gets the required IV size for the specified method.
     * @param method The encryption method.
     * @return IV size in bytes.
     */
    static size_t getIvSize(EncryptionOptions::Method method);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_ENCRYPTION_HPP
