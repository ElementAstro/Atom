#ifndef ATOM_SECRET_OTP_HOTP_HPP
#define ATOM_SECRET_OTP_HOTP_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "../core/result.hpp"
#include "totp.hpp"

namespace atom::secret {

/**
 * @brief HOTP (HMAC-based One-Time Password) configuration.
 */
struct HotpConfig {
    int digits = 6;  ///< Number of digits (6 or 8)
    OtpHashAlgorithm algorithm = OtpHashAlgorithm::SHA1;  ///< Hash algorithm
    std::string issuer;       ///< Issuer name for URI
    std::string accountName;  ///< Account name for URI

    /**
     * @brief Creates default HOTP configuration.
     * @return Default configuration.
     */
    static HotpConfig defaults() { return HotpConfig{}; }
};

/**
 * @brief HOTP (HMAC-based One-Time Password) implementation.
 *
 * Implements RFC 4226 for counter-based one-time passwords.
 */
class Hotp {
public:
    /**
     * @brief Generates an HOTP code for a counter value.
     * @param secret Base32-encoded secret key.
     * @param counter Counter value.
     * @param config HOTP configuration.
     * @return Result containing the OTP code or error.
     */
    static Result<std::string> generate(
        std::string_view secret, int64_t counter,
        const HotpConfig& config = HotpConfig::defaults());

    /**
     * @brief Verifies an HOTP code.
     * @param secret Base32-encoded secret key.
     * @param code Code to verify.
     * @param counter Expected counter value.
     * @param config HOTP configuration.
     * @param lookAhead Number of counter values to check ahead.
     * @return Counter value if valid, -1 if invalid.
     */
    static int64_t verify(std::string_view secret, std::string_view code,
                          int64_t counter,
                          const HotpConfig& config = HotpConfig::defaults(),
                          int lookAhead = 10);

    /**
     * @brief Generates a random secret key.
     * @param length Length in bytes (default 20 for SHA1).
     * @return Result containing Base32-encoded secret or error.
     */
    static Result<std::string> generateSecret(size_t length = 20);

    /**
     * @brief Generates an otpauth:// URI for QR code generation.
     * @param secret Base32-encoded secret key.
     * @param counter Initial counter value.
     * @param config HOTP configuration.
     * @return Result containing the URI or error.
     */
    static Result<std::string> generateUri(std::string_view secret,
                                           int64_t counter,
                                           const HotpConfig& config);

    /**
     * @brief Parses an otpauth:// URI.
     * @param uri The URI to parse.
     * @param secret Output: the secret key.
     * @param counter Output: the counter value.
     * @param config Output: the configuration.
     * @return True if parsing succeeded.
     */
    static bool parseUri(std::string_view uri, std::string& secret,
                         int64_t& counter, HotpConfig& config);
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_OTP_HOTP_HPP
