#include "hotp.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace atom::secret {

Result<std::string> Hotp::generate(std::string_view secret, int64_t counter,
                                   const HotpConfig& config) {
    // Decode Base32 secret
    auto decodeResult = Base32::decode(secret);
    if (decodeResult.isError()) {
        return Result<std::string>::error(ErrorCode::InvalidOtpSecret,
                                          "Invalid Base32 secret");
    }

    const auto& secretBytes = decodeResult.value();

    // Convert counter to big-endian bytes
    uint8_t counterBytes[8];
    int64_t c = counter;
    for (int i = 7; i >= 0; --i) {
        counterBytes[i] = static_cast<uint8_t>(c & 0xFF);
        c >>= 8;
    }

    // Select hash algorithm
    const EVP_MD* md = nullptr;
    switch (config.algorithm) {
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
        HMAC(md, secretBytes.data(), static_cast<int>(secretBytes.size()),
             counterBytes, 8, hmacResult, &hmacLen);

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
    uint32_t otp = binary % static_cast<uint32_t>(std::pow(10, config.digits));

    // Format with leading zeros
    std::ostringstream ss;
    ss << std::setw(config.digits) << std::setfill('0') << otp;

    return Result<std::string>::success(ss.str());
}

int64_t Hotp::verify(std::string_view secret, std::string_view code,
                     int64_t counter, const HotpConfig& config, int lookAhead) {
    for (int i = 0; i <= lookAhead; ++i) {
        auto result = generate(secret, counter + i, config);
        if (result.isSuccess() && result.value() == code) {
            return counter + i;
        }
    }
    return -1;
}

Result<std::string> Hotp::generateSecret(size_t length) {
    return Totp::generateSecret(length);
}

Result<std::string> Hotp::generateUri(std::string_view secret, int64_t counter,
                                      const HotpConfig& config) {
    if (config.accountName.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Account name is required");
    }

    std::ostringstream uri;
    uri << "otpauth://hotp/";

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
    uri << "&counter=" << counter;

    if (!config.issuer.empty()) {
        uri << "&issuer=" << urlEncode(config.issuer);
    }

    if (config.digits != 6) {
        uri << "&digits=" << config.digits;
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

bool Hotp::parseUri(std::string_view uri, std::string& secret, int64_t& counter,
                    HotpConfig& config) {
    // Check prefix
    if (uri.substr(0, 15) != "otpauth://hotp/") {
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

    // Parse label
    size_t colonPos = label.find(':');
    if (colonPos != std::string::npos) {
        config.issuer = label.substr(0, colonPos);
        config.accountName = label.substr(colonPos + 1);
    } else {
        config.accountName = label;
    }

    // Parse query parameters
    counter = 0;
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
        } else if (key == "counter") {
            counter = std::stoll(value);
        } else if (key == "issuer") {
            config.issuer = value;
        } else if (key == "digits") {
            config.digits = std::stoi(value);
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

}  // namespace atom::secret
