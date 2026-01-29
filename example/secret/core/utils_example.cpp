/*
 * utils_example.cpp
 *
 * Demonstrates the usage of utility functions in the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/types.hpp"
#include "atom/secret/core/utils.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Utils Example ===" << std::endl << std::endl;

    // ========================================================================
    // Time Utilities
    // ========================================================================
    std::cout << "--- Time Utilities ---" << std::endl;

    auto currentTime = now();
    std::string isoTime = Utils::formatTimeIso8601(currentTime);
    std::cout << "Current time (ISO 8601): " << isoTime << std::endl;

    auto parsedTime = Utils::parseTimeIso8601(isoTime);
    std::cout << "Parsed back: " << Utils::formatTimeIso8601(parsedTime)
              << std::endl;

    std::cout << std::endl << "Duration formatting:" << std::endl;
    std::cout << "  30 seconds: " << Utils::formatDuration(30) << std::endl;
    std::cout << "  120 seconds: " << Utils::formatDuration(120) << std::endl;
    std::cout << "  7200 seconds: " << Utils::formatDuration(7200) << std::endl;
    std::cout << "  172800 seconds: " << Utils::formatDuration(172800)
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // String Utilities
    // ========================================================================
    std::cout << "--- String Utilities ---" << std::endl;

    std::string testStr = "  Hello World  ";
    std::cout << "Original: '" << testStr << "'" << std::endl;
    std::cout << "Trimmed: '" << Utils::trim(testStr) << "'" << std::endl;
    std::cout << "Lowercase: '" << Utils::toLower(testStr) << "'" << std::endl;
    std::cout << "Uppercase: '" << Utils::toUpper(testStr) << "'" << std::endl;
    std::cout << std::endl;

    std::cout << "Contains 'world' (case-insensitive): "
              << (Utils::containsIgnoreCase("Hello World", "world") ? "true"
                                                                    : "false")
              << std::endl;
    std::cout << "Contains 'xyz' (case-insensitive): "
              << (Utils::containsIgnoreCase("Hello World", "xyz") ? "true"
                                                                  : "false")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Masking
    // ========================================================================
    std::cout << "--- Password Masking ---" << std::endl;

    std::string password = "MySecretPassword123";
    std::cout << "Original: " << password << std::endl;
    std::cout << "Fully masked: " << Utils::mask(password) << std::endl;
    std::cout << "Show first 2 and last 2: " << Utils::mask(password, 2, 2)
              << std::endl;
    std::cout << "Custom mask char (#): " << Utils::mask(password, 0, 0, '#')
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Validation
    // ========================================================================
    std::cout << "--- Validation Utilities ---" << std::endl;

    std::cout << "Email validation:" << std::endl;
    std::cout << "  'user@example.com': "
              << (Utils::isValidEmail("user@example.com") ? "valid" : "invalid")
              << std::endl;
    std::cout << "  'invalid-email': "
              << (Utils::isValidEmail("invalid-email") ? "valid" : "invalid")
              << std::endl;
    std::cout << "  '@example.com': "
              << (Utils::isValidEmail("@example.com") ? "valid" : "invalid")
              << std::endl;
    std::cout << std::endl;

    std::cout << "URL validation:" << std::endl;
    std::cout << "  'https://example.com': "
              << (Utils::isValidUrl("https://example.com") ? "valid"
                                                           : "invalid")
              << std::endl;
    std::cout << "  'http://localhost:8080': "
              << (Utils::isValidUrl("http://localhost:8080") ? "valid"
                                                             : "invalid")
              << std::endl;
    std::cout << "  'example.com': "
              << (Utils::isValidUrl("example.com") ? "valid" : "invalid")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Random Utilities
    // ========================================================================
    std::cout << "--- Random Utilities ---" << std::endl;

    std::cout << "Random integers (1-100): ";
    for (int i = 0; i < 5; ++i) {
        std::cout << Utils::randomInt(1, 100) << " ";
    }
    std::cout << std::endl;

    std::string toShuffle = "ABCDEFGHIJ";
    std::cout << "Original string: " << toShuffle << std::endl;
    std::cout << "Shuffled: " << Utils::shuffleString(toShuffle) << std::endl;
    std::cout << "Shuffled again: " << Utils::shuffleString(toShuffle)
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Hex Encoding
    // ========================================================================
    std::cout << "--- Hex Encoding ---" << std::endl;

    ByteVector data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    std::string hex = bytesToHex(data);
    std::cout << "Bytes to hex: " << hex << std::endl;
    std::cout << "Bytes to hex (uppercase): " << bytesToHex(data, true)
              << std::endl;

    ByteVector decoded = hexToBytes(hex);
    std::cout << "Hex to bytes: ";
    for (uint8_t b : decoded) {
        std::cout << static_cast<char>(b);
    }
    std::cout << std::endl;

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
