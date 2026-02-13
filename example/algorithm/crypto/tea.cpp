/**
 * @file tea.cpp
 * @brief Comprehensive example demonstrating TEA family encryption algorithms
 *
 * This example shows how to:
 * - Use TEA (Tiny Encryption Algorithm) for basic encryption
 * - Use XTEA (eXtended TEA) for improved security
 * - Use XXTEA for variable-length data encryption
 * - Handle different data formats and key management
 * - Compare performance characteristics of different variants
 * - Demonstrate proper usage patterns and security considerations
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/crypto/tea.hpp"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
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
 * @brief Demonstrates basic TEA encryption and decryption
 */
void demonstrateBasicTEA() {
    printHeader("Basic TEA Encryption/Decryption");

    try {
        // Define a 128-bit key for TEA
        std::array<uint32_t, 4> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                       0x2468ACE0};

        std::cout << "TEA Key: ";
        for (const auto& k : key) {
            std::cout << std::hex << std::setw(8) << std::setfill('0') << k
                      << " ";
        }
        std::cout << std::dec << "\n\n";

        // Test multiple data blocks
        std::vector<std::pair<uint32_t, uint32_t>> testData = {
            {0x01234567, 0x89ABCDEF},
            {0x00000000, 0x00000000},
            {0xFFFFFFFF, 0xFFFFFFFF},
            {0xAAAAAAAA, 0x55555555}};

        for (size_t i = 0; i < testData.size(); ++i) {
            uint32_t value0 = testData[i].first;
            uint32_t value1 = testData[i].second;
            uint32_t original0 = value0;
            uint32_t original1 = value1;

            std::cout << "Test " << (i + 1) << ":\n";
            std::cout << "  Original: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Encrypt
            teaEncrypt(value0, value1, key);
            std::cout << "  Encrypted: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Decrypt
            teaDecrypt(value0, value1, key);
            std::cout << "  Decrypted: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Verify
            bool success = (value0 == original0 && value1 == original1);
            std::cout << "  Verification: "
                      << (success ? "✓ PASSED" : "✗ FAILED") << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic TEA demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates XTEA encryption and decryption
 */
void demonstrateXTEA() {
    printHeader("XTEA (eXtended TEA) Encryption/Decryption");

    try {
        std::array<uint32_t, 4> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                       0x2468ACE0};

        std::cout << "XTEA provides improved security over basic TEA\n";
        std::cout << "Key schedule is more complex and resistant to "
                     "related-key attacks\n\n";

        // Test data
        std::vector<std::pair<uint32_t, uint32_t>> testData = {
            {0x01234567, 0x89ABCDEF},
            {0x12345678, 0x9ABCDEF0},
            {0xDEADBEEF, 0xCAFEBABE}};

        for (size_t i = 0; i < testData.size(); ++i) {
            uint32_t value0 = testData[i].first;
            uint32_t value1 = testData[i].second;
            uint32_t original0 = value0;
            uint32_t original1 = value1;

            std::cout << "XTEA Test " << (i + 1) << ":\n";
            std::cout << "  Original: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Encrypt
            xteaEncrypt(value0, value1, key);
            std::cout << "  Encrypted: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Decrypt
            xteaDecrypt(value0, value1, key);
            std::cout << "  Decrypted: " << std::hex << std::setw(8)
                      << std::setfill('0') << value0 << " " << std::setw(8)
                      << std::setfill('0') << value1 << "\n";

            // Verify
            bool success = (value0 == original0 && value1 == original1);
            std::cout << "  Verification: "
                      << (success ? "✓ PASSED" : "✗ FAILED") << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in XTEA demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates XXTEA encryption for variable-length data
 */
void demonstrateXXTEA() {
    printHeader("XXTEA (Corrected Block TEA) for Variable-Length Data");

    try {
        std::vector<uint32_t> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                     0x2468ACE0};

        std::cout << "XXTEA can encrypt variable-length data (not just 64-bit "
                     "blocks)\n";
        std::cout << "It's more suitable for encrypting arbitrary amounts of "
                     "data\n\n";

        // Test different data sizes
        std::vector<std::vector<uint32_t>> testDataSets = {
            {0x01234567, 0x89ABCDEF},                          // 2 words
            {0x01234567, 0x89ABCDEF, 0xFEDCBA98},              // 3 words
            {0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210},  // 4 words
            {0x01234567, 0x89ABCDEF, 0xFEDCBA98, 0x76543210, 0x13579BDF}
            // 5 words
        };

        for (size_t i = 0; i < testDataSets.size(); ++i) {
            std::vector<uint32_t> data = testDataSets[i];
            std::vector<uint32_t> originalData = data;

            std::cout << "XXTEA Test " << (i + 1) << " (" << data.size()
                      << " words):\n";
            std::cout << "  Original: ";
            for (const auto& val : data) {
                std::cout << std::hex << std::setw(8) << std::setfill('0')
                          << val << " ";
            }
            std::cout << "\n";

            // Encrypt
            std::vector<uint32_t> encrypted =
                xxteaEncrypt(data, std::span<const uint32_t, 4>(key.data(), 4));
            std::cout << "  Encrypted: ";
            for (const auto& val : encrypted) {
                std::cout << std::hex << std::setw(8) << std::setfill('0')
                          << val << " ";
            }
            std::cout << "\n";

            // Decrypt
            std::vector<uint32_t> decrypted = xxteaDecrypt(
                encrypted, std::span<const uint32_t, 4>(key.data(), 4));
            std::cout << "  Decrypted: ";
            for (const auto& val : decrypted) {
                std::cout << std::hex << std::setw(8) << std::setfill('0')
                          << val << " ";
            }
            std::cout << "\n";

            // Verify
            bool success = (decrypted == originalData);
            std::cout << "  Verification: "
                      << (success ? "✓ PASSED" : "✗ FAILED") << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in XXTEA demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates data conversion utilities
 */
void demonstrateDataConversion() {
    printHeader("Data Conversion Utilities");

    try {
        std::cout << "TEA family algorithms work with 32-bit words\n";
        std::cout << "Conversion utilities help work with byte arrays\n\n";

        // Test byte array to uint32 conversion
        std::vector<uint8_t> byteArray = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB,
                                          0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98,
                                          0x76, 0x54, 0x32, 0x10};

        std::cout << "Original byte array: ";
        for (const auto& byte : byteArray) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << "\n";

        // Convert to uint32 vector
        std::vector<uint32_t> uint32Vector = toUint32Vector(byteArray);
        std::cout << "Converted to uint32 vector: ";
        for (const auto& val : uint32Vector) {
            std::cout << std::hex << std::setw(8) << std::setfill('0') << val
                      << " ";
        }
        std::cout << "\n";

        // Convert back to byte array
        std::vector<uint8_t> convertedByteArray = toByteArray(uint32Vector);
        std::cout << "Converted back to byte array: ";
        for (const auto& byte : convertedByteArray) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << "\n";

        // Verify round-trip conversion
        bool conversionSuccess = (byteArray == convertedByteArray);
        std::cout << "Round-trip conversion: "
                  << (conversionSuccess ? "✓ PASSED" : "✗ FAILED") << "\n\n";

        // Demonstrate string encryption using XXTEA
        std::cout << "String encryption example:\n";
        std::string message = "Hello, TEA encryption!";
        std::cout << "Original message: \"" << message << "\"\n";

        // Convert string to byte array
        std::vector<uint8_t> messageBytes(message.begin(), message.end());

        // Pad to multiple of 4 bytes for uint32 conversion
        while (messageBytes.size() % 4 != 0) {
            messageBytes.push_back(0);
        }

        // Convert to uint32 and encrypt
        std::vector<uint32_t> messageWords = toUint32Vector(messageBytes);
        std::vector<uint32_t> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                     0x2468ACE0};

        std::vector<uint32_t> encryptedWords = xxteaEncrypt(
            messageWords, std::span<const uint32_t, 4>(key.data(), 4));

        // Convert back to bytes
        std::vector<uint8_t> encryptedBytes = toByteArray(encryptedWords);

        std::cout << "Encrypted bytes: ";
        for (const auto& byte : encryptedBytes) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << "\n";

        // Decrypt
        std::vector<uint32_t> decryptedWords = xxteaDecrypt(
            encryptedWords, std::span<const uint32_t, 4>(key.data(), 4));
        std::vector<uint8_t> decryptedBytes = toByteArray(decryptedWords);

        // Convert back to string (remove padding)
        std::string decryptedMessage(decryptedBytes.begin(),
                                     decryptedBytes.begin() + message.length());
        std::cout << "Decrypted message: \"" << decryptedMessage << "\"\n";

        bool messageSuccess = (message == decryptedMessage);
        std::cout << "Message encryption: "
                  << (messageSuccess ? "✓ PASSED" : "✗ FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in data conversion demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates performance comparison between TEA variants
 */
void demonstratePerformanceComparison() {
    printHeader("Performance Comparison");

    try {
        const size_t iterations = 100000;
        std::array<uint32_t, 4> key = {0x12345678, 0x9ABCDEF0, 0x13579BDF,
                                       0x2468ACE0};

        std::cout << "Performance test with " << iterations
                  << " iterations:\n\n";

        // TEA performance
        uint32_t teaValue0 = 0x01234567;
        uint32_t teaValue1 = 0x89ABCDEF;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            teaEncrypt(teaValue0, teaValue1, key);
            teaDecrypt(teaValue0, teaValue1, key);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto teaTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // XTEA performance
        uint32_t xteaValue0 = 0x01234567;
        uint32_t xteaValue1 = 0x89ABCDEF;

        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            xteaEncrypt(xteaValue0, xteaValue1, key);
            xteaDecrypt(xteaValue0, xteaValue1, key);
        }
        end = std::chrono::high_resolution_clock::now();
        auto xteaTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // XXTEA performance (smaller iteration count due to higher overhead)
        std::vector<uint32_t> xxteaData = {0x01234567, 0x89ABCDEF, 0xFEDCBA98,
                                           0x76543210};
        const size_t xxteaIterations = iterations / 10;

        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < xxteaIterations; ++i) {
            auto encrypted = xxteaEncrypt(
                xxteaData, std::span<const uint32_t, 4>(key.data(), 4));
            auto decrypted = xxteaDecrypt(
                encrypted, std::span<const uint32_t, 4>(key.data(), 4));
            (void)decrypted;  // Suppress unused variable warning
        }
        end = std::chrono::high_resolution_clock::now();
        auto xxteaTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "TEA:   " << teaTime.count() << " μs (" << iterations
                  << " encrypt/decrypt cycles)\n";
        std::cout << "XTEA:  " << xteaTime.count() << " μs (" << iterations
                  << " encrypt/decrypt cycles)\n";
        std::cout << "XXTEA: " << xxteaTime.count() << " μs ("
                  << xxteaIterations << " encrypt/decrypt cycles)\n\n";

        std::cout << "Performance per operation:\n";
        std::cout << "TEA:   "
                  << static_cast<double>(teaTime.count()) / (iterations * 2)
                  << " μs per operation\n";
        std::cout << "XTEA:  "
                  << static_cast<double>(xteaTime.count()) / (iterations * 2)
                  << " μs per operation\n";
        std::cout << "XXTEA: "
                  << static_cast<double>(xxteaTime.count()) /
                         (xxteaIterations * 2)
                  << " μs per operation\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in performance comparison: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive TEA family usage
 */
int main() {
    std::cout << "=== Atom TEA Family Algorithms Comprehensive Example ===\n";
    std::cout
        << "Demonstrating TEA, XTEA, and XXTEA encryption capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicTEA();
        demonstrateXTEA();
        demonstrateXXTEA();
        demonstrateDataConversion();
        demonstratePerformanceComparison();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All TEA Family Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The TEA family algorithms provide:\n";
        std::cout << "  ✓ Fast and simple encryption (TEA)\n";
        std::cout << "  ✓ Improved security with better key schedule (XTEA)\n";
        std::cout << "  ✓ Variable-length data encryption (XXTEA)\n";
        std::cout << "  ✓ Minimal memory footprint and high performance\n";
        std::cout << "  ✓ Suitable for embedded systems and "
                     "resource-constrained environments\n";
        std::cout << "  ⚠️  Consider AES for new applications requiring strong "
                     "security\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in TEA example: " << e.what() << "\n";
        return 1;
    }
}
