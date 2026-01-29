#ifndef ATOM_SECRET_OTP_TOTP_HPP
#define ATOM_SECRET_OTP_TOTP_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "atom/algorithm/encoding/base.hpp"

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Hash algorithm for OTP generation.
 */
enum class OtpHashAlgorithm {
    SHA1,    ///< SHA-1 (default, most compatible)
    SHA256,  ///< SHA-256
    SHA512   ///< SHA-512
};

/**
 * @brief TOTP (Time-based One-Time Password) configuration.
 */
struct TotpConfig {
    int digits = 6;   ///< Number of digits (6 or 8)
    int period = 30;  ///< Time step in seconds
    OtpHashAlgorithm algorithm = OtpHashAlgorithm::SHA1;  ///< Hash algorithm
    std::string issuer;       ///< Issuer name for URI
    std::string accountName;  ///< Account name for URI

    /**
     * @brief Creates default TOTP configuration.
     * @return Default configuration.
     */
    static TotpConfig defaults() { return TotpConfig{}; }

    /**
     * @brief Creates configuration for 8-digit codes.
     * @return 8-digit configuration.
     */
    static TotpConfig eightDigits() {
        TotpConfig config;
        config.digits = 8;
        return config;
    }
};

/**
 * @brief TOTP (Time-based One-Time Password) implementation.
 *
 * Implements RFC 6238 for time-based one-time passwords.
 */
class Totp {
public:
    /**
     * @brief Generates a TOTP code for the current time.
     * @param secret Base32-encoded secret key.
     * @param config TOTP configuration.
     * @return Result containing the OTP code or error.
     */
    static Result<std::string> generate(
        std::string_view secret,
        const TotpConfig& config = TotpConfig::defaults());

    /**
     * @brief Generates a TOTP code for a specific timestamp.
     * @param secret Base32-encoded secret key.
     * @param timestamp Unix timestamp in seconds.
     * @param config TOTP configuration.
     * @return Result containing the OTP code or error.
     */
    static Result<std::string> generateAt(
        std::string_view secret, int64_t timestamp,
        const TotpConfig& config = TotpConfig::defaults());

    /**
     * @brief Verifies a TOTP code.
     * @param secret Base32-encoded secret key.
     * @param code Code to verify.
     * @param config TOTP configuration.
     * @param window Number of time steps to check before/after current.
     * @return True if code is valid.
     */
    static bool verify(std::string_view secret, std::string_view code,
                       const TotpConfig& config = TotpConfig::defaults(),
                       int window = 1);

    /**
     * @brief Verifies a TOTP code at a specific timestamp.
     * @param secret Base32-encoded secret key.
     * @param code Code to verify.
     * @param timestamp Unix timestamp in seconds.
     * @param config TOTP configuration.
     * @param window Number of time steps to check before/after.
     * @return True if code is valid.
     */
    static bool verifyAt(std::string_view secret, std::string_view code,
                         int64_t timestamp,
                         const TotpConfig& config = TotpConfig::defaults(),
                         int window = 1);

    /**
     * @brief Generates a random secret key.
     * @param length Length in bytes (default 20 for SHA1).
     * @return Result containing Base32-encoded secret or error.
     */
    static Result<std::string> generateSecret(size_t length = 20);

    /**
     * @brief Generates an otpauth:// URI for QR code generation.
     * @param secret Base32-encoded secret key.
     * @param config TOTP configuration (must include issuer and accountName).
     * @return Result containing the URI or error.
     */
    static Result<std::string> generateUri(std::string_view secret,
                                           const TotpConfig& config);

    /**
     * @brief Parses an otpauth:// URI.
     * @param uri The URI to parse.
     * @param secret Output: the secret key.
     * @param config Output: the configuration.
     * @return True if parsing succeeded.
     */
    static bool parseUri(std::string_view uri, std::string& secret,
                         TotpConfig& config);

    /**
     * @brief Gets the remaining seconds until the current code expires.
     * @param period Time step in seconds.
     * @return Remaining seconds.
     */
    static int getRemainingSeconds(int period = 30);

    /**
     * @brief Gets the current time counter value.
     * @param period Time step in seconds.
     * @return Counter value.
     */
    static int64_t getCounter(int period = 30);

    /**
     * @brief Gets the current time counter for a specific timestamp.
     * @param timestamp Unix timestamp.
     * @param period Time step in seconds.
     * @return Counter value.
     */
    static int64_t getCounterAt(int64_t timestamp, int period = 30);

private:
    /**
     * @brief Generates HOTP code for a counter value.
     * @param secret Decoded secret key.
     * @param counter Counter value.
     * @param digits Number of digits.
     * @param algorithm Hash algorithm.
     * @return Result containing the OTP code or error.
     */
    static Result<std::string> generateHotp(const std::vector<uint8_t>& secret,
                                            int64_t counter, int digits,
                                            OtpHashAlgorithm algorithm);
};

/**
 * @brief Base32 encoding/decoding utilities.
 *
 * This class wraps atom::algorithm::encodeBase32/decodeBase32 for convenience.
 */
class Base32 {
public:
    /**
     * @brief Encodes data to Base32.
     * @param data Data to encode.
     * @return Base32-encoded string.
     */
    static std::string encode(const std::vector<uint8_t>& data);

    /**
     * @brief Encodes data to Base32.
     * @param data Data to encode.
     * @param length Data length.
     * @return Base32-encoded string.
     */
    static std::string encode(const uint8_t* data, size_t length);

    /**
     * @brief Decodes Base32 string using atom::algorithm::decodeBase32.
     * @param encoded Base32-encoded string.
     * @return Result containing decoded data or error.
     */
    static Result<std::vector<uint8_t>> decode(std::string_view encoded);

    /**
     * @brief Checks if a string is valid Base32.
     * @param str String to check.
     * @return True if valid Base32.
     */
    static bool isValid(std::string_view str);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_OTP_TOTP_HPP
