/**
 * @file base.cpp
 * @brief Comprehensive example demonstrating base encoding algorithms
 *
 * This example shows how to:
 * - Use Base64 encoding with SIMD optimizations
 * - Use Base32 encoding for different applications
 * - Perform XOR encryption for simple obfuscation
 * - Handle different data types and edge cases
 * - Demonstrate RFC compliance and performance benefits
 * - Show error handling and validation
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/base.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates comprehensive Base64 encoding and decoding
 */
void demonstrateBase64Encoding() {
    printHeader("Base64 Encoding/Decoding Examples");

    try {
        // Test various input types and sizes
        std::vector<std::string> testInputs = {
            "Hello, World!",
            "",       // Empty string
            "A",      // Single character
            "AB",     // Two characters
            "ABC",    // Three characters (no padding)
            "ABCD",   // Four characters (one padding)
            "ABCDE",  // Five characters (two padding)
            "The quick brown fox jumps over the lazy dog",
            "Binary data: \x00\x01\x02\x03\xFF\xFE\xFD",
            "Unicode: 你好世界 🌍 🚀",
            std::string(1000, 'A')  // Large string
        };

        for (const auto& data : testInputs) {
            std::cout << "\nTesting: \""
                      << (data.length() > 50 ? data.substr(0, 47) + "..."
                                             : data)
                      << "\"\n";
            std::cout << "Length: " << data.length() << " bytes\n";

            // Encode
            auto encodedResult = base64Encode(data);
            if (encodedResult) {
                std::string encoded = encodedResult.value();
                std::cout << "Base64 Encoded: " << encoded << "\n";
                std::cout << "Encoded length: " << encoded.length()
                          << " characters\n";

                // Calculate expected length (4 * ceil(n/3))
                size_t expectedLength = ((data.length() + 2) / 3) * 4;
                std::cout << "Expected length: " << expectedLength
                          << " characters "
                          << (encoded.length() == expectedLength ? "✓" : "✗")
                          << "\n";

                // Decode
                auto decodedResult = base64Decode(encoded);
                if (decodedResult) {
                    std::string decoded = decodedResult.value();
                    bool isIdentical = (data == decoded);
                    std::cout << "Round-trip test: "
                              << (isIdentical ? "✓ PASSED" : "✗ FAILED")
                              << "\n";

                    if (!isIdentical) {
                        std::cout << "  Original length: " << data.length()
                                  << "\n";
                        std::cout << "  Decoded length: " << decoded.length()
                                  << "\n";
                    }
                } else {
                    std::cout << "Base64 Decode Error: "
                              << decodedResult.error().error() << "\n";
                }
            } else {
                std::cout << "Base64 Encode Error: "
                          << encodedResult.error().error() << "\n";
            }
        }

        // Test invalid Base64 strings
        std::cout << "\nTesting invalid Base64 strings:\n";
        std::vector<std::string> invalidInputs = {
            "Invalid!@#$",  // Invalid characters
            "SGVsbG8=X",    // Extra character after padding
            "SGVsbG",       // Incomplete padding
            "SGVs bG8=",    // Space in middle
        };

        for (const auto& invalid : invalidInputs) {
            std::cout << "  Testing: \"" << invalid << "\" -> ";
            auto result = base64Decode(invalid);
            if (result) {
                std::cout << "Unexpectedly succeeded\n";
            } else {
                std::cout << "Failed as expected (" << result.error().error()
                          << ")\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in Base64 demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates XOR encryption and decryption
 */
void demonstrateXOREncryption() {
    printHeader("XOR Encryption/Decryption Examples");

    try {
        std::cout << "XOR encryption provides simple obfuscation (not "
                     "cryptographically secure):\n\n";

        // Test different keys and data
        std::vector<std::pair<std::string, uint8_t>> testCases = {
            {"Secret Message", 0xAA},
            {"Hello, World!", 0x55},
            {"Binary data: \x00\x01\x02\x03", 0xFF},
            {"", 0x42},  // Empty string
            {"A", 0x33}  // Single character
        };

        for (const auto& [plaintext, key] : testCases) {
            std::cout << "Test case:\n";
            std::cout << "  Original: \"" << plaintext << "\"\n";
            std::cout << "  Key: 0x" << std::hex << std::setw(2)
                      << std::setfill('0') << static_cast<int>(key) << std::dec
                      << "\n";

            // Encrypt
            std::string encrypted = xorEncrypt(plaintext, key);
            std::cout << "  Encrypted (hex): ";
            for (unsigned char c : encrypted) {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(c) << " ";
            }
            std::cout << std::dec << "\n";

            // Decrypt
            std::string decrypted = xorDecrypt(encrypted, key);
            std::cout << "  Decrypted: \"" << decrypted << "\"\n";

            // Verify
            bool success = (plaintext == decrypted);
            std::cout << "  Verification: "
                      << (success ? "✓ PASSED" : "✗ FAILED") << "\n\n";
        }

        // Demonstrate XOR properties
        std::cout << "XOR encryption properties:\n";
        std::cout << "  • Symmetric: encryption and decryption use the same "
                     "operation\n";
        std::cout
            << "  • Self-inverse: XOR with same key twice returns original\n";
        std::cout << "  • Fast: single XOR operation per byte\n";
        std::cout << "  ⚠️  Not secure: vulnerable to frequency analysis\n";
        std::cout << "  ⚠️  Use only for simple obfuscation, not security\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in XOR encryption demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates Base64 validation functionality
 */
void demonstrateBase64Validation() {
    printHeader("Base64 Validation Examples");

    try {
        std::cout << "Testing Base64 string validation:\n\n";

        std::vector<std::pair<std::string, bool>> testCases = {
            {"SGVsbG8sIFdvcmxkIQ==", true},  // "Hello, World!" - valid
            {"", true},                      // Empty string - valid
            {"QQ==", true},                  // "A" - valid with padding
            {"QUI=", true},                  // "AB" - valid with padding
            {"QUJD", true},                  // "ABC" - valid without padding
            {"InvalidBase64String", false},  // Invalid characters
            {"SGVsbG8=X", false},            // Extra character after padding
            {"SGVs bG8=", false},            // Space in middle
            {"SGVsbG", false},               // Incomplete
            {"SGVsbG8==X", false},           // Character after padding
        };

        for (const auto& [testString, expected] : testCases) {
            bool result = isBase64(testString);
            std::cout << "  \"" << testString << "\" -> "
                      << (result ? "valid" : "invalid") << " "
                      << (result == expected ? "✓" : "✗") << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in Base64 validation: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates Base32 encoding and decoding
 */
void demonstrateBase32Encoding() {
    printHeader("Base32 Encoding/Decoding Examples");

    try {
        std::cout << "Base32 encoding uses 32 characters (A-Z, 2-7) for data "
                     "representation:\n\n";

        // Test various data patterns
        std::vector<std::vector<uint8_t>> testData = {
            {0x48, 0x65, 0x6C, 0x6C, 0x6F},  // "Hello"
            {},                              // Empty
            {0x41},                          // "A"
            {0x41, 0x42},                    // "AB"
            {0x00, 0x01, 0x02, 0x03, 0x04},  // Binary sequence
            {0xFF, 0xFE, 0xFD, 0xFC, 0xFB},  // High values
        };

        for (const auto& data : testData) {
            std::cout << "Original data (hex): ";
            if (data.empty()) {
                std::cout << "(empty)";
            } else {
                for (auto byte : data) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(byte) << " ";
                }
            }
            std::cout << std::dec << "\n";

            // Encode
            auto encodedResult = encodeBase32(std::span<const uint8_t>(data));
            if (encodedResult) {
                std::string encoded = encodedResult.value();
                std::cout << "Base32 Encoded: \"" << encoded << "\"\n";
                std::cout << "Encoded length: " << encoded.length()
                          << " characters\n";

                // Decode
                auto decodedResult = decodeBase32(encoded);
                if (decodedResult) {
                    std::vector<uint8_t> decoded = decodedResult.value();
                    std::cout << "Base32 Decoded (hex): ";
                    if (decoded.empty()) {
                        std::cout << "(empty)";
                    } else {
                        for (auto byte : decoded) {
                            std::cout << std::hex << std::setw(2)
                                      << std::setfill('0')
                                      << static_cast<int>(byte) << " ";
                        }
                    }
                    std::cout << std::dec << "\n";

                    // Verify
                    bool isIdentical = (data == decoded);
                    std::cout << "Round-trip test: "
                              << (isIdentical ? "✓ PASSED" : "✗ FAILED")
                              << "\n\n";
                } else {
                    std::cout << "Base32 Decode Error: "
                              << decodedResult.error().error() << "\n\n";
                }
            } else {
                std::cout << "Base32 Encode Error: "
                          << encodedResult.error().error() << "\n\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in Base32 demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics of encoding algorithms
 */
void demonstratePerformanceCharacteristics() {
    printHeader("Performance Characteristics");

    try {
        std::cout << "Performance analysis of encoding algorithms:\n\n";

        // Test with different data sizes
        std::vector<size_t> dataSizes = {100, 1000, 10000, 100000};
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (size_t size : dataSizes) {
            // Generate random data
            std::string testData;
            testData.reserve(size);
            for (size_t i = 0; i < size; ++i) {
                testData += static_cast<char>(dis(gen));
            }

            std::cout << "Data size: " << size << " bytes\n";

            // Measure Base64 encoding time
            auto start = std::chrono::high_resolution_clock::now();
            auto base64Result = base64Encode(testData);
            auto end = std::chrono::high_resolution_clock::now();
            auto base64Time =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            if (base64Result) {
                std::cout << "  Base64 encoding: " << base64Time.count()
                          << " μs\n";

                // Measure Base64 decoding time
                start = std::chrono::high_resolution_clock::now();
                auto decodeResult = base64Decode(base64Result.value());
                end = std::chrono::high_resolution_clock::now();
                auto decodeTime =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        end - start);

                std::cout << "  Base64 decoding: " << decodeTime.count()
                          << " μs\n";
            }

            // Measure XOR encryption time
            start = std::chrono::high_resolution_clock::now();
            auto xorResult = xorEncrypt(testData, 0xAA);
            end = std::chrono::high_resolution_clock::now();
            auto xorTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  XOR encryption: " << xorTime.count() << " μs\n";

            // Calculate throughput
            double base64Throughput =
                (static_cast<double>(size) / 1024.0 / 1024.0) /
                (base64Time.count() / 1000000.0);
            double xorThroughput =
                (static_cast<double>(size) / 1024.0 / 1024.0) /
                (xorTime.count() / 1000000.0);

            std::cout << "  Base64 throughput: " << std::fixed
                      << std::setprecision(2) << base64Throughput << " MB/s\n";
            std::cout << "  XOR throughput: " << std::fixed
                      << std::setprecision(2) << xorThroughput << " MB/s\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in performance demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive encoding capabilities
 */
int main() {
    std::cout << "=== Atom Encoding Algorithms Comprehensive Example ===\n";
    std::cout << "Demonstrating base encoding and encryption capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBase64Encoding();
        demonstrateXOREncryption();
        demonstrateBase64Validation();
        demonstrateBase32Encoding();
        demonstratePerformanceCharacteristics();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Encoding Algorithm Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The encoding algorithm module provides:\n";
        std::cout << "  ✓ Base64 encoding with SIMD optimizations\n";
        std::cout << "  ✓ Base32 encoding for different applications\n";
        std::cout << "  ✓ XOR encryption for simple obfuscation\n";
        std::cout << "  ✓ RFC compliance and validation\n";
        std::cout << "  ✓ High-performance implementations\n";
        std::cout << "  ✓ Comprehensive error handling\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in encoding example: " << e.what()
                  << "\n";
        return 1;
    }
}
