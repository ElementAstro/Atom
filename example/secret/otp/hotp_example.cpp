/*
 * hotp_example.cpp
 *
 * Demonstrates HOTP (HMAC-based One-Time Password) usage.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>

#include "atom/secret/otp/hotp.hpp"
#include "atom/secret/otp/totp.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret HOTP Example ===" << std::endl << std::endl;

    // ========================================================================
    // Generate a Secret
    // ========================================================================
    std::cout << "--- Generate HOTP Secret ---" << std::endl;

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
    // Generate HOTP Codes
    // ========================================================================
    std::cout << "--- Generate HOTP Codes ---" << std::endl;

    HotpConfig config;
    config.secret = secret;
    config.counter = 0;
    config.digits = 6;

    std::cout << "Generating codes for counters 0-9:" << std::endl;
    for (uint64_t i = 0; i < 10; ++i) {
        config.counter = i;
        auto codeResult = Hotp::generate(config);
        if (codeResult.isSuccess()) {
            std::cout << "  Counter " << i << ": " << codeResult.value()
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Verify HOTP Code
    // ========================================================================
    std::cout << "--- Verify HOTP Code ---" << std::endl;

    config.counter = 5;
    auto code5 = Hotp::generate(config);
    if (code5.isSuccess()) {
        std::cout << "Generated code for counter 5: " << code5.value()
                  << std::endl;

        config.counter = 5;
        auto verifyResult = Hotp::verify(config, code5.value());
        if (verifyResult.isSuccess()) {
            std::cout << "Verification: "
                      << (verifyResult.value() ? "VALID" : "INVALID")
                      << std::endl;
        }

        // Try with wrong counter
        config.counter = 6;
        auto verifyWrong = Hotp::verify(config, code5.value());
        if (verifyWrong.isSuccess()) {
            std::cout << "With wrong counter: "
                      << (verifyWrong.value() ? "VALID" : "INVALID")
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Resync with Lookahead
    // ========================================================================
    std::cout << "--- Resync with Lookahead ---" << std::endl;

    config.counter = 0;
    auto code10 = Hotp::generate(HotpConfig{secret, 10, 6});
    if (code10.isSuccess()) {
        std::cout << "Code for counter 10: " << code10.value() << std::endl;

        // Try to verify with counter 0 but lookahead of 20
        auto resyncResult = Hotp::verifyWithResync(config, code10.value(), 20);
        if (resyncResult.isSuccess()) {
            std::cout << "Resync found at counter: " << resyncResult.value()
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Generate and Increment
    // ========================================================================
    std::cout << "--- Generate and Increment ---" << std::endl;

    config.counter = 0;
    std::cout << "Initial counter: " << config.counter << std::endl;

    for (int i = 0; i < 5; ++i) {
        auto result = Hotp::generateAndIncrement(config);
        if (result.isSuccess()) {
            std::cout << "Generated: " << result.value()
                      << " (counter now: " << config.counter << ")"
                      << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Different Digit Lengths
    // ========================================================================
    std::cout << "--- Different Digit Lengths ---" << std::endl;

    config.counter = 0;

    config.digits = 6;
    auto code6 = Hotp::generate(config);
    if (code6.isSuccess()) {
        std::cout << "6-digit: " << code6.value() << std::endl;
    }

    config.digits = 8;
    auto code8 = Hotp::generate(config);
    if (code8.isSuccess()) {
        std::cout << "8-digit: " << code8.value() << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
