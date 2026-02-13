#ifndef ATOM_SECRET_CRYPTO_HMAC_HPP
#define ATOM_SECRET_CRYPTO_HMAC_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"
#include "hash.hpp"

namespace atom::secret {

/**
 * @brief HMAC (Hash-based Message Authentication Code) implementation.
 *
 * HMAC provides message authentication using a cryptographic hash function
 * combined with a secret key.
 */
class Hmac {
public:
    /**
     * @brief Computes HMAC-SHA256.
     * @param key Secret key.
     * @param data Data to authenticate.
     * @return Result containing 32-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha256(
        const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

    /**
     * @brief Computes HMAC-SHA256.
     * @param key Secret key as string.
     * @param data Data to authenticate as string.
     * @return Result containing 32-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha256(std::string_view key,
                                               std::string_view data);

    /**
     * @brief Computes HMAC-SHA384.
     * @param key Secret key.
     * @param data Data to authenticate.
     * @return Result containing 48-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha384(
        const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

    /**
     * @brief Computes HMAC-SHA384.
     * @param key Secret key as string.
     * @param data Data to authenticate as string.
     * @return Result containing 48-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha384(std::string_view key,
                                               std::string_view data);

    /**
     * @brief Computes HMAC-SHA512.
     * @param key Secret key.
     * @param data Data to authenticate.
     * @return Result containing 64-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha512(
        const std::vector<uint8_t>& key, const std::vector<uint8_t>& data);

    /**
     * @brief Computes HMAC-SHA512.
     * @param key Secret key as string.
     * @param data Data to authenticate as string.
     * @return Result containing 64-byte HMAC or error.
     */
    static Result<std::vector<uint8_t>> sha512(std::string_view key,
                                               std::string_view data);

    /**
     * @brief Computes HMAC using specified hash algorithm.
     * @param algorithm Hash algorithm to use.
     * @param key Secret key.
     * @param data Data to authenticate.
     * @return Result containing HMAC or error.
     */
    static Result<std::vector<uint8_t>> compute(
        HashAlgorithm algorithm, const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& data);

    /**
     * @brief Computes HMAC using specified hash algorithm.
     * @param algorithm Hash algorithm to use.
     * @param key Secret key as string.
     * @param data Data to authenticate as string.
     * @return Result containing HMAC or error.
     */
    static Result<std::vector<uint8_t>> compute(HashAlgorithm algorithm,
                                                std::string_view key,
                                                std::string_view data);

    /**
     * @brief Verifies HMAC in constant time.
     * @param algorithm Hash algorithm.
     * @param key Secret key.
     * @param data Data to verify.
     * @param expectedMac Expected HMAC value.
     * @return True if HMAC matches.
     */
    static bool verify(HashAlgorithm algorithm, const std::vector<uint8_t>& key,
                       const std::vector<uint8_t>& data,
                       const std::vector<uint8_t>& expectedMac);

    /**
     * @brief Verifies HMAC in constant time.
     * @param algorithm Hash algorithm.
     * @param key Secret key as string.
     * @param data Data to verify as string.
     * @param expectedMacHex Expected HMAC as hex string.
     * @return True if HMAC matches.
     */
    static bool verify(HashAlgorithm algorithm, std::string_view key,
                       std::string_view data, std::string_view expectedMacHex);

    /**
     * @brief Computes HMAC and returns as hex string.
     * @param algorithm Hash algorithm.
     * @param key Secret key as string.
     * @param data Data to authenticate as string.
     * @return Result containing hex string or error.
     */
    static Result<std::string> computeHex(HashAlgorithm algorithm,
                                          std::string_view key,
                                          std::string_view data);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CRYPTO_HMAC_HPP
