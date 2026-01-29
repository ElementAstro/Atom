#ifndef ATOM_SECRET_CRYPTO_HASH_HPP
#define ATOM_SECRET_CRYPTO_HASH_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Supported hash algorithms.
 */
enum class HashAlgorithm {
    SHA256,      ///< SHA-256 (256 bits / 32 bytes)
    SHA384,      ///< SHA-384 (384 bits / 48 bytes)
    SHA512,      ///< SHA-512 (512 bits / 64 bytes)
    SHA3_256,    ///< SHA3-256 (256 bits / 32 bytes)
    SHA3_512,    ///< SHA3-512 (512 bits / 64 bytes)
    BLAKE2b256,  ///< BLAKE2b-256 (256 bits / 32 bytes)
    BLAKE2b512,  ///< BLAKE2b-512 (512 bits / 64 bytes)
    BLAKE2s256,  ///< BLAKE2s-256 (256 bits / 32 bytes)
    MD5,  ///< MD5 (128 bits / 16 bytes) - NOT SECURE, for compatibility only
};

/**
 * @brief Cryptographic hash functions.
 *
 * This class provides various cryptographic hash functions for computing
 * message digests.
 */
class Hash {
public:
    /**
     * @brief Computes SHA-256 hash of data.
     * @param data Data to hash.
     * @return Result containing 32-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha256(
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes SHA-256 hash of string.
     * @param data String to hash.
     * @return Result containing 32-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha256(std::string_view data);

    /**
     * @brief Computes SHA-384 hash of data.
     * @param data Data to hash.
     * @return Result containing 48-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha384(
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes SHA-384 hash of string.
     * @param data String to hash.
     * @return Result containing 48-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha384(std::string_view data);

    /**
     * @brief Computes SHA-512 hash of data.
     * @param data Data to hash.
     * @return Result containing 64-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha512(
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes SHA-512 hash of string.
     * @param data String to hash.
     * @return Result containing 64-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha512(std::string_view data);

    /**
     * @brief Computes SHA3-256 hash of data.
     * @param data Data to hash.
     * @return Result containing 32-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha3_256(
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes SHA3-256 hash of string.
     * @param data String to hash.
     * @return Result containing 32-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha3_256(std::string_view data);

    /**
     * @brief Computes SHA3-512 hash of data.
     * @param data Data to hash.
     * @return Result containing 64-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha3_512(
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes SHA3-512 hash of string.
     * @param data String to hash.
     * @return Result containing 64-byte hash or error.
     */
    static Result<std::vector<uint8_t>> sha3_512(std::string_view data);

    /**
     * @brief Computes BLAKE2b hash of data.
     * @param data Data to hash.
     * @param digestLength Output length in bytes (1-64, default 32).
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> blake2b(
        const std::vector<uint8_t>& data, size_t digestLength = 32);

    /**
     * @brief Computes BLAKE2b hash of string.
     * @param data String to hash.
     * @param digestLength Output length in bytes (1-64, default 32).
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> blake2b(std::string_view data,
                                                size_t digestLength = 32);

    /**
     * @brief Computes BLAKE2s hash of data.
     * @param data Data to hash.
     * @param digestLength Output length in bytes (1-32, default 32).
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> blake2s(
        const std::vector<uint8_t>& data, size_t digestLength = 32);

    /**
     * @brief Computes BLAKE2s hash of string.
     * @param data String to hash.
     * @param digestLength Output length in bytes (1-32, default 32).
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> blake2s(std::string_view data,
                                                size_t digestLength = 32);

    /**
     * @brief Computes hash using specified algorithm.
     * @param algorithm Hash algorithm to use.
     * @param data Data to hash.
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> compute(
        HashAlgorithm algorithm, const std::vector<uint8_t>& data);

    /**
     * @brief Computes hash using specified algorithm.
     * @param algorithm Hash algorithm to use.
     * @param data String to hash.
     * @return Result containing hash or error.
     */
    static Result<std::vector<uint8_t>> compute(HashAlgorithm algorithm,
                                                std::string_view data);

    /**
     * @brief Gets the output size for a hash algorithm.
     * @param algorithm Hash algorithm.
     * @return Output size in bytes.
     */
    static size_t getDigestSize(HashAlgorithm algorithm) noexcept;

    /**
     * @brief Gets the algorithm name as string.
     * @param algorithm Hash algorithm.
     * @return Algorithm name.
     */
    static std::string getAlgorithmName(HashAlgorithm algorithm) noexcept;

    /**
     * @brief Checks if an algorithm is available.
     * @param algorithm Hash algorithm to check.
     * @return True if available.
     */
    static bool isAvailable(HashAlgorithm algorithm) noexcept;

    /**
     * @brief Computes hash and returns as hexadecimal string.
     * @param algorithm Hash algorithm to use.
     * @param data Data to hash.
     * @return Result containing hex string or error.
     */
    static Result<std::string> computeHex(HashAlgorithm algorithm,
                                          std::string_view data);

    /**
     * @brief Verifies data against expected hash.
     * @param algorithm Hash algorithm.
     * @param data Data to verify.
     * @param expectedHash Expected hash value.
     * @return True if hash matches.
     */
    static bool verify(HashAlgorithm algorithm,
                       const std::vector<uint8_t>& data,
                       const std::vector<uint8_t>& expectedHash);

    /**
     * @brief Verifies data against expected hash (hex string).
     * @param algorithm Hash algorithm.
     * @param data Data to verify.
     * @param expectedHex Expected hash as hex string.
     * @return True if hash matches.
     */
    static bool verify(HashAlgorithm algorithm, std::string_view data,
                       std::string_view expectedHex);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CRYPTO_HASH_HPP
