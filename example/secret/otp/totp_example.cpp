/*
 * totp_example.cpp
 *
 * Demonstrates TOTP (Time-based One-Time Password) usage.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "atom/secret/otp/totp.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret TOTP Example ===" << std::endl << std::endl;

    // ========================================================================
    // Generate a Secret
    // ========================================================================
    std::cout << "--- Generate TOTP Secret ---" << std::endl;

    auto secretResult = Totp::generateSecret(20);
    if (secretResult.isError()) {
        std::cerr << "Failed to generate secret: "
                  << secretResult.errorMessage() << std::endl;
        return 1;
    }

    std::string secret = secretResult.value();
    std::cout << "Generated secret (Base32): " << secret << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Generate TOTP Code
    // ========================================================================
    std::cout << "--- Generate TOTP Code ---" << std::endl;

    TotpConfig config;
    config.secret = secret;
    config.digits = 6;
    config.period = 30;
    config.algorithm = HashAlgorithm::SHA1;
    config.issuer = "AtomSecret";
    config.accountName = "user@example.com";

    auto codeResult = Totp::generate(config);
    if (codeResult.isSuccess()) {
        std::cout << "Current TOTP code: " << codeResult.value() << std::endl;
    } else {
        std::cerr << "Failed to generate code: " << codeResult.errorMessage()
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Generate URI for QR Code
    // ========================================================================
    std::cout << "--- Generate OTPAuth URI ---" << std::endl;

    std::string uri = Totp::generateUri(config);
    std::cout << "OTPAuth URI: " << uri << std::endl;
    std::cout << "(Scan this as QR code in authenticator app)" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Verify TOTP Code
    // ========================================================================
    std::cout << "--- Verify TOTP Code ---" << std::endl;

    if (codeResult.isSuccess()) {
        bool valid = Totp::verify(config, codeResult.value());
        std::cout << "Code '" << codeResult.value() << "' is "
                  << (valid ? "VALID" : "INVALID") << std::endl;

        // Verify with window
        bool validWithWindow = Totp::verify(config, codeResult.value(), 1);
        std::cout << "With window=1: "
                  << (validWithWindow ? "VALID" : "INVALID") << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Different Digit Lengths
    // ========================================================================
    std::cout << "--- Different Digit Lengths ---" << std::endl;

    TotpConfig config6 = config;
    config6.digits = 6;
    auto code6 = Totp::generate(config6);
    if (code6.isSuccess()) {
        std::cout << "6-digit code: " << code6.value() << std::endl;
    }

    TotpConfig config8 = config;
    config8.digits = 8;
    auto code8 = Totp::generate(config8);
    if (code8.isSuccess()) {
        std::cout << "8-digit code: " << code8.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Watch Codes Change
    // ========================================================================
    std::cout << "--- Watch TOTP Codes (5 iterations) ---" << std::endl;

    for (int i = 0; i < 5; ++i) {
        auto code = Totp::generate(config);
        if (code.isSuccess()) {
            auto now = std::chrono::system_clock::now();
            auto epoch = now.time_since_epoch();
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
            int remaining = 30 - (seconds % 30);

            std::cout << "Code: " << code.value() << " (expires in "
                      << remaining << "s)" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    std::cout << std::endl;

    // ========================================================================
    // Parse OTPAuth URI
    // ========================================================================
    std::cout << "--- Parse OTPAuth URI ---" << std::endl;

    std::string testUri =
        "otpauth://totp/"
        "Example:alice@example.com?secret=JBSWY3DPEHPK3PXP&issuer=Example&"
        "digits=6&period=30";

    auto parseResult = Totp::parseUri(testUri);
    if (parseResult.isSuccess()) {
        const auto& parsed = parseResult.value();
        std::cout << "Parsed URI:" << std::endl;
        std::cout << "  Secret: " << parsed.secret << std::endl;
        std::cout << "  Issuer: " << parsed.issuer << std::endl;
        std::cout << "  Account: " << parsed.accountName << std::endl;
        std::cout << "  Digits: " << parsed.digits << std::endl;
        std::cout << "  Period: " << parsed.period << std::endl;
    } else {
        std::cerr << "Failed to parse URI: " << parseResult.errorMessage()
                  << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
