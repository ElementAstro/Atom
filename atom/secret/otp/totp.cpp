#include "totp.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace atom::secret {

Result<std::string> Totp::generate(std::string_view secret,
                                   const TotpConfig& config) {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return generateAt(secret, timestamp, config);
}

Result<std::string> Totp::generateAt(std::string_view secret, int64_t timestamp,
                                     const TotpConfig& config) {
    // Decode Base32 secret
    auto decodeResult = Base32::decode(secret);
    if (decodeResult.isError()) {
        return Result<std::string>::error(ErrorCode::InvalidOtpSecret,
                                          "Invalid Base32 secret");
    }

    // Calculate counter
    int64_t counter = timestamp / config.period;

    return generateHotp(decodeResult.value(), counter, config.digits,
                        config.algorithm);
}

bool Totp::verify(std::string_view secret, std::string_view code,
                  const TotpConfig& config, int window) {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return verifyAt(secret, code, timestamp, config, window);
}

bool Totp::verifyAt(std::string_view secret, std::string_view code,
                    int64_t timestamp, const TotpConfig& config, int window) {
    // Check codes within the window
    for (int i = -window; i <= window; ++i) {
        int64_t checkTime = timestamp + (i * config.period);
        auto result = generateAt(secret, checkTime, config);
        if (result.isSuccess() && result.value() == code) {
            return true;
        }
    }
    return false;
}

Result<std::string> Totp::generateSecret(size_t length) {
    if (length < 10 || length > 64) {
        return Result<std::string>::error(
            ErrorCode::InvalidArgument,
            "Secret length must be between 10 and 64 bytes");
    }

    std::vector<uint8_t> secret(length);
    if (RAND_bytes(secret.data(), static_cast<int>(length)) != 1) {
        return Result<std::string>::error(ErrorCode::RandomGenerationFailed,
                                          "Failed to generate random secret");
    }

    return Result<std::string>::success(Base32::encode(secret));
}

Result<std::string> Totp::generateUri(std::string_view secret,
                                      const TotpConfig& config) {
    if (config.accountName.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Account name is required");
    }

    std::ostringstream uri;
    uri << "otpauth://totp/";

    // URL encode the label
    auto urlEncode = [](const std::string& str) {
        std::ostringstream encoded;
        for (char c : str) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
                c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else if (c == ' ') {
                encoded << "%20";
            } else {
                encoded << '%' << std::uppercase << std::hex << std::setw(2)
                        << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(c));
            }
        }
        return encoded.str();
    };

    if (!config.issuer.empty()) {
        uri << urlEncode(config.issuer) << ":";
    }
    uri << urlEncode(config.accountName);

    uri << "?secret=" << secret;

    if (!config.issuer.empty()) {
        uri << "&issuer=" << urlEncode(config.issuer);
    }

    if (config.digits != 6) {
        uri << "&digits=" << config.digits;
    }

    if (config.period != 30) {
        uri << "&period=" << config.period;
    }

    if (config.algorithm != OtpHashAlgorithm::SHA1) {
        uri << "&algorithm=";
        switch (config.algorithm) {
            case OtpHashAlgorithm::SHA256:
                uri << "SHA256";
                break;
            case OtpHashAlgorithm::SHA512:
                uri << "SHA512";
                break;
            default:
                uri << "SHA1";
                break;
        }
    }

    return Result<std::string>::success(uri.str());
}

bool Totp::parseUri(std::string_view uri, std::string& secret,
                    TotpConfig& config) {
    // Check prefix
    if (uri.substr(0, 15) != "otpauth://totp/") {
        return false;
    }

    std::string remaining(uri.substr(15));

    // Find query string
    size_t queryPos = remaining.find('?');
    if (queryPos == std::string::npos) {
        return false;
    }

    std::string label = remaining.substr(0, queryPos);
    std::string query = remaining.substr(queryPos + 1);

    // Parse label (issuer:account or just account)
    size_t colonPos = label.find(':');
    if (colonPos != std::string::npos) {
        config.issuer = label.substr(0, colonPos);
        config.accountName = label.substr(colonPos + 1);
    } else {
        config.accountName = label;
    }

    // Parse query parameters
    size_t pos = 0;
    while (pos < query.length()) {
        size_t eqPos = query.find('=', pos);
        if (eqPos == std::string::npos)
            break;

        size_t ampPos = query.find('&', eqPos);
        if (ampPos == std::string::npos)
            ampPos = query.length();

        std::string key = query.substr(pos, eqPos - pos);
        std::string value = query.substr(eqPos + 1, ampPos - eqPos - 1);

        if (key == "secret") {
            secret = value;
        } else if (key == "issuer") {
            config.issuer = value;
        } else if (key == "digits") {
            config.digits = std::stoi(value);
        } else if (key == "period") {
            config.period = std::stoi(value);
        } else if (key == "algorithm") {
            if (value == "SHA256") {
                config.algorithm = OtpHashAlgorithm::SHA256;
            } else if (value == "SHA512") {
                config.algorithm = OtpHashAlgorithm::SHA512;
            } else {
                config.algorithm = OtpHashAlgorithm::SHA1;
            }
        }

        pos = ampPos + 1;
    }

    return !secret.empty();
}

int Totp::getRemainingSeconds(int period) {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return period - static_cast<int>(timestamp % period);
}

int64_t Totp::getCounter(int period) {
    auto now = std::chrono::system_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch())
            .count();
    return timestamp / period;
}

int64_t Totp::getCounterAt(int64_t timestamp, int period) {
    return timestamp / period;
}

Result<std::string> Totp::generateHotp(const std::vector<uint8_t>& secret,
                                       int64_t counter, int digits,
                                       OtpHashAlgorithm algorithm) {
    // Convert counter to big-endian bytes
    uint8_t counterBytes[8];
    for (int i = 7; i >= 0; --i) {
        counterBytes[i] = static_cast<uint8_t>(counter & 0xFF);
        counter >>= 8;
    }

    // Select hash algorithm
    const EVP_MD* md = nullptr;
    switch (algorithm) {
        case OtpHashAlgorithm::SHA1:
            md = EVP_sha1();
            break;
        case OtpHashAlgorithm::SHA256:
            md = EVP_sha256();
            break;
        case OtpHashAlgorithm::SHA512:
            md = EVP_sha512();
            break;
        default:
            return Result<std::string>::error(ErrorCode::InvalidOtpAlgorithm,
                                              "Invalid OTP algorithm");
    }

    // Compute HMAC
    unsigned char hmacResult[EVP_MAX_MD_SIZE];
    unsigned int hmacLen = 0;

    unsigned char* result =
        HMAC(md, secret.data(), static_cast<int>(secret.size()), counterBytes,
             8, hmacResult, &hmacLen);

    if (!result) {
        return Result<std::string>::error(ErrorCode::OtpGenerationFailed,
                                          "HMAC computation failed");
    }

    // Dynamic truncation (RFC 4226)
    int offset = hmacResult[hmacLen - 1] & 0x0F;
    uint32_t binary =
        ((static_cast<uint32_t>(hmacResult[offset]) & 0x7F) << 24) |
        ((static_cast<uint32_t>(hmacResult[offset + 1]) & 0xFF) << 16) |
        ((static_cast<uint32_t>(hmacResult[offset + 2]) & 0xFF) << 8) |
        (static_cast<uint32_t>(hmacResult[offset + 3]) & 0xFF);

    // Generate OTP
    uint32_t otp = binary % static_cast<uint32_t>(std::pow(10, digits));

    // Format with leading zeros
    std::ostringstream ss;
    ss << std::setw(digits) << std::setfill('0') << otp;

    return Result<std::string>::success(ss.str());
}

// ============================================================================
// Base32 Implementation - Using atom::algorithm
// ============================================================================

std::string Base32::encode(const std::vector<uint8_t>& data) {
    auto result = atom::algorithm::encodeBase32(std::span<const uint8_t>(data));
    if (result.has_value()) {
        return result.value();
    }
    return "";
}

std::string Base32::encode(const uint8_t* data, size_t length) {
    auto result =
        atom::algorithm::encodeBase32(std::span<const uint8_t>(data, length));
    if (result.has_value()) {
        return result.value();
    }
    return "";
}

Result<std::vector<uint8_t>> Base32::decode(std::string_view encoded) {
    if (encoded.empty()) {
        return Result<std::vector<uint8_t>>::success(std::vector<uint8_t>{});
    }

    auto result = atom::algorithm::decodeBase32(encoded);
    if (result.has_value()) {
        return Result<std::vector<uint8_t>>::success(std::move(result.value()));
    }
    return Result<std::vector<uint8_t>>::error(ErrorCode::InvalidFormat,
                                               "Invalid Base32 encoding");
}

bool Base32::isValid(std::string_view str) {
    for (char c : str) {
        if (c == '=' || std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        char upper =
            static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (!((upper >= 'A' && upper <= 'Z') ||
              (upper >= '2' && upper <= '7'))) {
            return false;
        }
    }
    return true;
}

}  // namespace atom::secret
