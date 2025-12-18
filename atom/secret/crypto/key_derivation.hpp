#ifndef ATOM_SECRET_CRYPTO_KEY_DERIVATION_HPP
#define ATOM_SECRET_CRYPTO_KEY_DERIVATION_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Supported key derivation algorithms.
 */
enum class KeyDerivationAlgorithm {
    PBKDF2_SHA256,  ///< PBKDF2 with HMAC-SHA256
    PBKDF2_SHA512,  ///< PBKDF2 with HMAC-SHA512
    Argon2id,       ///< Argon2id (recommended for password hashing)
    Argon2i,        ///< Argon2i (side-channel resistant)
    Argon2d,        ///< Argon2d (GPU resistant)
    Scrypt,         ///< scrypt
};

/**
 * @brief Parameters for PBKDF2 key derivation.
 */
struct Pbkdf2Params {
    int iterations =
        100000;  ///< Number of iterations (minimum 10000 recommended)

    /**
     * @brief Creates default PBKDF2 parameters.
     * @return Default parameters.
     */
    static Pbkdf2Params defaults() { return Pbkdf2Params{}; }

    /**
     * @brief Creates high-security PBKDF2 parameters.
     * @return High-security parameters.
     */
    static Pbkdf2Params highSecurity() {
        return Pbkdf2Params{.iterations = 600000};
    }
};

/**
 * @brief Parameters for Argon2 key derivation.
 */
struct Argon2Params {
    uint32_t memoryCost = 65536;  ///< Memory usage in KiB (64 MB default)
    uint32_t timeCost = 3;        ///< Number of iterations
    uint32_t parallelism = 4;     ///< Degree of parallelism

    /**
     * @brief Creates default Argon2 parameters.
     * @return Default parameters.
     */
    static Argon2Params defaults() { return Argon2Params{}; }

    /**
     * @brief Creates low-memory Argon2 parameters.
     * @return Low-memory parameters suitable for constrained environments.
     */
    static Argon2Params lowMemory() {
        return Argon2Params{
            .memoryCost = 16384, .timeCost = 4, .parallelism = 2};
    }

    /**
     * @brief Creates high-security Argon2 parameters.
     * @return High-security parameters.
     */
    static Argon2Params highSecurity() {
        return Argon2Params{
            .memoryCost = 262144, .timeCost = 4, .parallelism = 8};
    }
};

/**
 * @brief Parameters for scrypt key derivation.
 */
struct ScryptParams {
    uint64_t n = 16384;  ///< CPU/memory cost parameter (power of 2)
    uint32_t r = 8;      ///< Block size
    uint32_t p = 1;      ///< Parallelization parameter

    /**
     * @brief Creates default scrypt parameters.
     * @return Default parameters.
     */
    static ScryptParams defaults() { return ScryptParams{}; }

    /**
     * @brief Creates high-security scrypt parameters.
     * @return High-security parameters.
     */
    static ScryptParams highSecurity() {
        return ScryptParams{.n = 1048576, .r = 8, .p = 1};
    }
};

/**
 * @brief Cryptographic key derivation utilities.
 *
 * This class provides various key derivation functions (KDFs) for deriving
 * cryptographic keys from passwords or other key material.
 */
class KeyDerivation {
public:
    /**
     * @brief Derives a key from a password using PBKDF2-HMAC-SHA256.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation (should be random, at least 16
     * bytes).
     * @param iterations Number of PBKDF2 iterations (minimum 10000).
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> pbkdf2Sha256(
        std::string_view password, const std::vector<uint8_t>& salt,
        int iterations, size_t keyLength);

    /**
     * @brief Derives a key from a password using PBKDF2-HMAC-SHA512.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation.
     * @param iterations Number of PBKDF2 iterations.
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> pbkdf2Sha512(
        std::string_view password, const std::vector<uint8_t>& salt,
        int iterations, size_t keyLength);

    /**
     * @brief Derives a key from a password using PBKDF2 with specified
     * parameters.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation.
     * @param params PBKDF2 parameters.
     * @param keyLength Desired key length in bytes.
     * @param algorithm The hash algorithm to use.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> pbkdf2(
        std::string_view password, const std::vector<uint8_t>& salt,
        const Pbkdf2Params& params, size_t keyLength,
        KeyDerivationAlgorithm algorithm =
            KeyDerivationAlgorithm::PBKDF2_SHA256);

    /**
     * @brief Derives a key from a password using Argon2id.
     *
     * Argon2id is the recommended algorithm for password hashing as it provides
     * resistance against both side-channel and GPU attacks.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation (should be at least 16 bytes).
     * @param params Argon2 parameters.
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> argon2id(
        std::string_view password, const std::vector<uint8_t>& salt,
        const Argon2Params& params, size_t keyLength);

    /**
     * @brief Derives a key from a password using Argon2i.
     *
     * Argon2i is optimized for side-channel resistance.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation.
     * @param params Argon2 parameters.
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> argon2i(
        std::string_view password, const std::vector<uint8_t>& salt,
        const Argon2Params& params, size_t keyLength);

    /**
     * @brief Derives a key from a password using scrypt.
     *
     * @param password The password to derive from.
     * @param salt The salt for key derivation.
     * @param params scrypt parameters.
     * @param keyLength Desired key length in bytes.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> scrypt(std::string_view password,
                                               const std::vector<uint8_t>& salt,
                                               const ScryptParams& params,
                                               size_t keyLength);

    /**
     * @brief Generates a cryptographically secure random salt.
     *
     * @param length Length of salt in bytes (default 32).
     * @return Result containing the salt or error message.
     */
    static Result<std::vector<uint8_t>> generateSalt(size_t length = 32);

    /**
     * @brief Generates a cryptographically secure random key.
     *
     * @param length Length of key in bytes (default 32).
     * @return Result containing the key or error message.
     */
    static Result<std::vector<uint8_t>> generateKey(size_t length = 32);

    /**
     * @brief Generates cryptographically secure random bytes.
     *
     * @param length Number of bytes to generate.
     * @return Result containing the random bytes or error message.
     */
    static Result<std::vector<uint8_t>> generateRandomBytes(size_t length);

    /**
     * @brief Derives a key using HKDF (HMAC-based Key Derivation Function).
     *
     * HKDF is useful for deriving multiple keys from a single master key.
     *
     * @param inputKey The input key material.
     * @param salt Optional salt (can be empty).
     * @param info Optional context/application-specific info.
     * @param keyLength Desired output key length.
     * @return Result containing the derived key or error message.
     */
    static Result<std::vector<uint8_t>> hkdf(
        const std::vector<uint8_t>& inputKey, const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& info, size_t keyLength);

    /**
     * @brief Checks if Argon2 is available on this system.
     * @return True if Argon2 is available.
     */
    static bool isArgon2Available() noexcept;

    /**
     * @brief Checks if scrypt is available on this system.
     * @return True if scrypt is available.
     */
    static bool isScryptAvailable() noexcept;

    /**
     * @brief Benchmarks key derivation to determine appropriate parameters.
     *
     * @param targetMilliseconds Target time for key derivation in milliseconds.
     * @param algorithm The algorithm to benchmark.
     * @return Recommended iteration count or memory cost.
     */
    static int benchmarkIterations(int targetMilliseconds,
                                   KeyDerivationAlgorithm algorithm =
                                       KeyDerivationAlgorithm::PBKDF2_SHA256);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CRYPTO_KEY_DERIVATION_HPP
