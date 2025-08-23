/**
 * @file md5.cpp
 * @brief Comprehensive example demonstrating MD5 hash algorithm usage
 *
 * This example shows how to:
 * - Compute MD5 hashes for various types of input
 * - Handle different data formats (strings, binary data, files)
 * - Verify hash consistency and demonstrate use cases
 * - Handle errors and edge cases gracefully
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/md5.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace atom::algorithm;

/**
 * @brief Demonstrates basic MD5 hashing with various string inputs
 */
void demonstrateBasicMD5Hashing() {
    std::cout << "\n=== Basic MD5 Hashing Examples ===\n";

    try {
        // Test various string inputs
        std::vector<std::string> testInputs = {
            "Hello, World!",
            "",  // Empty string
            "The quick brown fox jumps over the lazy dog",
            "MD5 is a widely used hash function producing a 128-bit hash value",
            "1234567890",
            "Special characters: !@#$%^&*()_+-=[]{}|;:,.<>?",
            "Unicode: 你好世界 🌍 🚀"
        };

        for (const auto& input : testInputs) {
            std::string hash = MD5::encrypt(input);
            std::cout << "Input: \"" << input << "\"\n";
            std::cout << "MD5:   " << hash << "\n";
            std::cout << "Length: " << hash.length() << " characters\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic MD5 hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates MD5 hash consistency and verification
 */
void demonstrateHashConsistency() {
    std::cout << "\n=== MD5 Hash Consistency Verification ===\n";

    try {
        std::string testInput = "Consistency test string";

        // Compute hash multiple times to verify consistency
        std::cout << "Testing hash consistency for: \"" << testInput << "\"\n";

        std::vector<std::string> hashes;
        for (int i = 0; i < 5; ++i) {
            std::string hash = MD5::encrypt(testInput);
            hashes.push_back(hash);
            std::cout << "Hash " << (i + 1) << ": " << hash << "\n";
        }

        // Verify all hashes are identical
        bool consistent = true;
        for (size_t i = 1; i < hashes.size(); ++i) {
            if (hashes[i] != hashes[0]) {
                consistent = false;
                break;
            }
        }

        std::cout << "\nConsistency check: " << (consistent ? "✓ PASSED" : "✗ FAILED") << "\n";

        // Test hash collision resistance (different inputs should produce different hashes)
        std::cout << "\nTesting collision resistance:\n";
        std::string input1 = "test string 1";
        std::string input2 = "test string 2";
        std::string hash1 = MD5::encrypt(input1);
        std::string hash2 = MD5::encrypt(input2);

        std::cout << "\"" << input1 << "\" -> " << hash1 << "\n";
        std::cout << "\"" << input2 << "\" -> " << hash2 << "\n";
        std::cout << "Different inputs produce different hashes: "
                  << (hash1 != hash2 ? "✓ PASSED" : "✗ FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in hash consistency test: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates MD5 hashing for binary data
 */
void demonstrateBinaryDataHashing() {
    std::cout << "\n=== Binary Data MD5 Hashing ===\n";

    try {
        // Create some binary data
        std::vector<unsigned char> binaryData = {
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8
        };

        // Convert binary data to string for hashing
        std::string binaryString(binaryData.begin(), binaryData.end());
        std::string hash = MD5::encrypt(binaryString);

        std::cout << "Binary data (hex): ";
        for (unsigned char byte : binaryData) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << "\n";
        std::cout << "MD5 hash: " << hash << "\n";

        // Test with different binary patterns
        std::vector<unsigned char> pattern1(256, 0x00);  // All zeros
        std::vector<unsigned char> pattern2(256, 0xFF);  // All ones
        std::vector<unsigned char> pattern3;             // Sequential pattern
        for (int i = 0; i < 256; ++i) {
            pattern3.push_back(static_cast<unsigned char>(i));
        }

        std::string hash1 = MD5::encrypt(std::string(pattern1.begin(), pattern1.end()));
        std::string hash2 = MD5::encrypt(std::string(pattern2.begin(), pattern2.end()));
        std::string hash3 = MD5::encrypt(std::string(pattern3.begin(), pattern3.end()));

        std::cout << "\nBinary pattern hashes:\n";
        std::cout << "All zeros (256 bytes): " << hash1 << "\n";
        std::cout << "All ones (256 bytes):  " << hash2 << "\n";
        std::cout << "Sequential (0-255):    " << hash3 << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in binary data hashing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates practical MD5 use cases
 */
void demonstratePracticalUseCases() {
    std::cout << "\n=== Practical MD5 Use Cases ===\n";

    try {
        // Use case 1: Password verification (demonstration only - MD5 is not secure for passwords)
        std::cout << "1. Password Hash Verification (educational only):\n";
        std::string password = "mySecretPassword123";
        std::string storedHash = MD5::encrypt(password);
        std::string inputPassword = "mySecretPassword123";
        std::string inputHash = MD5::encrypt(inputPassword);

        std::cout << "   Stored hash: " << storedHash << "\n";
        std::cout << "   Input hash:  " << inputHash << "\n";
        std::cout << "   Match: " << (storedHash == inputHash ? "✓ YES" : "✗ NO") << "\n";
        std::cout << "   ⚠️  Note: MD5 is NOT secure for password hashing in production!\n\n";

        // Use case 2: Data integrity verification
        std::cout << "2. Data Integrity Verification:\n";
        std::string originalData = "Important data that must not be corrupted";
        std::string originalHash = MD5::encrypt(originalData);

        // Simulate data transmission/storage
        std::string receivedData = originalData;  // No corruption
        std::string receivedHash = MD5::encrypt(receivedData);

        std::cout << "   Original data: \"" << originalData << "\"\n";
        std::cout << "   Original hash: " << originalHash << "\n";
        std::cout << "   Received hash: " << receivedHash << "\n";
        std::cout << "   Data integrity: " << (originalHash == receivedHash ? "✓ INTACT" : "✗ CORRUPTED") << "\n\n";

        // Use case 3: Simple checksums for non-cryptographic purposes
        std::cout << "3. Simple Checksum Generation:\n";
        std::vector<std::string> dataItems = {
            "Item 1: Configuration data",
            "Item 2: User preferences",
            "Item 3: Application state"
        };

        for (size_t i = 0; i < dataItems.size(); ++i) {
            std::string checksum = MD5::encrypt(dataItems[i]);
            std::cout << "   " << dataItems[i] << "\n";
            std::cout << "   Checksum: " << checksum << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in practical use cases: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics and limitations
 */
void demonstratePerformanceAndLimitations() {
    std::cout << "\n=== Performance and Security Considerations ===\n";

    try {
        // Performance test with different input sizes
        std::cout << "Performance test with different input sizes:\n";

        std::vector<size_t> sizes = {10, 100, 1000, 10000};
        for (size_t size : sizes) {
            std::string testData(size, 'A');

            auto start = std::chrono::high_resolution_clock::now();
            std::string hash = MD5::encrypt(testData);
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            std::cout << "   " << size << " bytes: " << hash.substr(0, 16) << "... ";
            std::cout << "(" << duration.count() << " μs)\n";
        }

        // Security warnings
        std::cout << "\n⚠️  SECURITY WARNINGS:\n";
        std::cout << "   • MD5 is cryptographically broken and unsuitable for security purposes\n";
        std::cout << "   • MD5 is vulnerable to collision attacks\n";
        std::cout << "   • Use SHA-256 or better for cryptographic applications\n";
        std::cout << "   • MD5 is acceptable for non-cryptographic checksums only\n";

        // Demonstrate known MD5 collision (if available)
        std::cout << "\nMD5 Properties:\n";
        std::cout << "   • Always produces 128-bit (32 hex character) output\n";
        std::cout << "   • Deterministic: same input always produces same output\n";
        std::cout << "   • Fast computation\n";
        std::cout << "   • Avalanche effect: small input changes cause large output changes\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in performance demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive MD5 usage
 *
 * @return int Exit status (0 for success, 1 for error)
 */
int main() {
    std::cout << "=== Atom MD5 Algorithm Comprehensive Example ===\n";
    std::cout << "Demonstrating MD5 hash computation capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicMD5Hashing();
        demonstrateHashConsistency();
        demonstrateBinaryDataHashing();
        demonstratePracticalUseCases();
        demonstratePerformanceAndLimitations();

        std::cout << "\n=== All MD5 Examples Completed Successfully ===\n";
        std::cout << "The MD5 algorithm provides:\n";
        std::cout << "  ✓ Fast hash computation for various data types\n";
        std::cout << "  ✓ Consistent and deterministic results\n";
        std::cout << "  ✓ 128-bit hash output (32 hex characters)\n";
        std::cout << "  ✓ Suitable for non-cryptographic checksums\n";
        std::cout << "  ⚠️  NOT suitable for cryptographic security\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in MD5 example: " << e.what() << "\n";
        return 1;
    }
}
