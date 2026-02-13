/**
 * @file blowfish.cpp
 * @brief Comprehensive example demonstrating Blowfish encryption algorithm
 * usage
 *
 * This example shows how to:
 * - Encrypt and decrypt data using Blowfish algorithm
 * - Handle different data formats (strings, binary data, files)
 * - Demonstrate proper key management and security practices
 * - Show performance characteristics and use cases
 * - Handle errors and edge cases gracefully
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/crypto/blowfish.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Demonstrates basic Blowfish encryption and decryption
 */
void demonstrateBasicBlowfishEncryption() {
    std::cout << "\n=== Basic Blowfish Encryption Examples ===\n";

    try {
        // Create a key (in practice, use a secure random key)
        std::vector<std::byte> key = {
            std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
            std::byte{0x89}, std::byte{0xAB}, std::byte{0xCD}, std::byte{0xEF},
            std::byte{0xFE}, std::byte{0xDC}, std::byte{0xBA}, std::byte{0x98},
            std::byte{0x76}, std::byte{0x54}, std::byte{0x32}, std::byte{0x10}};

        // Initialize Blowfish with the key
        Blowfish blowfish(key);

        // Test data
        std::string plaintext =
            "Hello, Blowfish encryption! This is a test message.";
        std::cout << "Original text: \"" << plaintext << "\"\n";
        std::cout << "Text length: " << plaintext.length() << " bytes\n";

        // Convert string to byte vector for encryption
        std::vector<std::byte> data;
        for (char c : plaintext) {
            data.push_back(static_cast<std::byte>(c));
        }

        // Pad data to block size and encrypt
        std::size_t original_length = data.size();
        std::size_t padded_length =
            ((data.size() + 7) / 8) * 8;           // Round up to 8-byte blocks
        data.resize(padded_length, std::byte{0});  // Pad with zeros

        std::cout << "Padded length: " << padded_length << " bytes\n";

        // Encrypt the data
        blowfish.encrypt_data(std::span<std::byte>(data));

        std::cout << "Encrypted data (hex): ";
        for (const auto& byte : data) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte) << " ";
        }
        std::cout << std::dec << "\n";

        // Decrypt the data
        std::size_t decrypted_length = padded_length;
        blowfish.decrypt_data(std::span<std::byte>(data), decrypted_length);

        // Convert back to string
        std::string decrypted;
        for (std::size_t i = 0; i < original_length; ++i) {
            decrypted += static_cast<char>(data[i]);
        }

        std::cout << "Decrypted text: \"" << decrypted << "\"\n";
        std::cout << "Decryption successful: "
                  << (plaintext == decrypted ? "✓ YES" : "✗ NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic Blowfish encryption: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates Blowfish encryption with different key sizes
 */
void demonstrateKeyVariations() {
    std::cout << "\n=== Blowfish Key Size Variations ===\n";

    try {
        std::string testData = "Test data for key variations";

        // Test different key sizes (Blowfish supports 32-448 bits)
        std::vector<std::vector<std::byte>> keys = {
            // 64-bit key (minimum)
            {std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
             std::byte{0x89}, std::byte{0xAB}, std::byte{0xCD},
             std::byte{0xEF}},

            // 128-bit key
            {std::byte{0x01}, std::byte{0x23}, std::byte{0x45}, std::byte{0x67},
             std::byte{0x89}, std::byte{0xAB}, std::byte{0xCD}, std::byte{0xEF},
             std::byte{0xFE}, std::byte{0xDC}, std::byte{0xBA}, std::byte{0x98},
             std::byte{0x76}, std::byte{0x54}, std::byte{0x32},
             std::byte{0x10}},

            // 256-bit key
            {std::byte{0x01}, std::byte{0x23}, std::byte{0x45},
             std::byte{0x67}, std::byte{0x89}, std::byte{0xAB},
             std::byte{0xCD}, std::byte{0xEF}, std::byte{0xFE},
             std::byte{0xDC}, std::byte{0xBA}, std::byte{0x98},
             std::byte{0x76}, std::byte{0x54}, std::byte{0x32},
             std::byte{0x10}, std::byte{0x11}, std::byte{0x22},
             std::byte{0x33}, std::byte{0x44}, std::byte{0x55},
             std::byte{0x66}, std::byte{0x77}, std::byte{0x88},
             std::byte{0x99}, std::byte{0xAA}, std::byte{0xBB},
             std::byte{0xCC}, std::byte{0xDD}, std::byte{0xEE},
             std::byte{0xFF}, std::byte{0x00}}};

        for (std::size_t i = 0; i < keys.size(); ++i) {
            std::cout << "\nTesting " << (keys[i].size() * 8) << "-bit key:\n";

            Blowfish blowfish(keys[i]);

            // Prepare data
            std::vector<std::byte> data;
            for (char c : testData) {
                data.push_back(static_cast<std::byte>(c));
            }

            std::size_t original_length = data.size();
            std::size_t padded_length = ((data.size() + 7) / 8) * 8;
            data.resize(padded_length, std::byte{0});

            // Encrypt
            auto start = std::chrono::high_resolution_clock::now();
            blowfish.encrypt_data(std::span<std::byte>(data));
            auto end = std::chrono::high_resolution_clock::now();
            auto encrypt_time =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  Encryption time: " << encrypt_time.count()
                      << " μs\n";

            // Decrypt
            start = std::chrono::high_resolution_clock::now();
            std::size_t decrypted_length = padded_length;
            blowfish.decrypt_data(std::span<std::byte>(data), decrypted_length);
            end = std::chrono::high_resolution_clock::now();
            auto decrypt_time =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  Decryption time: " << decrypt_time.count()
                      << " μs\n";

            // Verify
            std::string decrypted;
            for (std::size_t j = 0; j < original_length; ++j) {
                decrypted += static_cast<char>(data[j]);
            }

            std::cout << "  Verification: "
                      << (testData == decrypted ? "✓ PASSED" : "✗ FAILED")
                      << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in key variations test: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates file encryption and decryption
 */
void demonstrateFileEncryption() {
    std::cout << "\n=== File Encryption/Decryption ===\n";

    try {
        // Create test files
        std::string input_file = "test_input.txt";
        std::string encrypted_file = "test_encrypted.bin";
        std::string decrypted_file = "test_decrypted.txt";

        // Create test input file
        {
            std::ofstream file(input_file);
            file << "This is a test file for Blowfish encryption.\n";
            file << "It contains multiple lines of text.\n";
            file << "The file will be encrypted and then decrypted.\n";
            file << "This demonstrates file-based encryption capabilities.\n";
        }

        // Create encryption key
        std::vector<std::byte> key = {
            std::byte{0x2B}, std::byte{0x7E}, std::byte{0x15}, std::byte{0x16},
            std::byte{0x28}, std::byte{0xAE}, std::byte{0xD2}, std::byte{0xA6},
            std::byte{0xAB}, std::byte{0xF7}, std::byte{0x15}, std::byte{0x88},
            std::byte{0x09}, std::byte{0xCF}, std::byte{0x4F}, std::byte{0x3C}};

        Blowfish blowfish(key);

        std::cout << "Input file: " << input_file << "\n";
        std::cout << "Encrypted file: " << encrypted_file << "\n";
        std::cout << "Decrypted file: " << decrypted_file << "\n";

        // Encrypt file
        auto start = std::chrono::high_resolution_clock::now();
        blowfish.encrypt_file(input_file, encrypted_file);
        auto end = std::chrono::high_resolution_clock::now();
        auto encrypt_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "File encryption completed in " << encrypt_time.count()
                  << " ms\n";

        // Check file sizes
        auto input_size = std::filesystem::file_size(input_file);
        auto encrypted_size = std::filesystem::file_size(encrypted_file);
        std::cout << "Original file size: " << input_size << " bytes\n";
        std::cout << "Encrypted file size: " << encrypted_size << " bytes\n";

        // Decrypt file
        start = std::chrono::high_resolution_clock::now();
        blowfish.decrypt_file(encrypted_file, decrypted_file);
        end = std::chrono::high_resolution_clock::now();
        auto decrypt_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "File decryption completed in " << decrypt_time.count()
                  << " ms\n";

        // Verify files are identical
        std::ifstream original(input_file);
        std::ifstream decrypted(decrypted_file);

        std::string original_content((std::istreambuf_iterator<char>(original)),
                                     std::istreambuf_iterator<char>());
        std::string decrypted_content(
            (std::istreambuf_iterator<char>(decrypted)),
            std::istreambuf_iterator<char>());

        std::cout << "File verification: "
                  << (original_content == decrypted_content ? "✓ PASSED"
                                                            : "✗ FAILED")
                  << "\n";

        // Clean up test files
        std::filesystem::remove(input_file);
        std::filesystem::remove(encrypted_file);
        std::filesystem::remove(decrypted_file);

    } catch (const std::exception& e) {
        std::cerr << "Error in file encryption test: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive Blowfish usage
 */
int main() {
    std::cout << "=== Atom Blowfish Algorithm Comprehensive Example ===\n";
    std::cout
        << "Demonstrating Blowfish symmetric encryption capabilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicBlowfishEncryption();
        demonstrateKeyVariations();
        demonstrateFileEncryption();

        std::cout << "\n=== All Blowfish Examples Completed Successfully ===\n";
        std::cout << "The Blowfish algorithm provides:\n";
        std::cout
            << "  ✓ Strong symmetric encryption with variable key length\n";
        std::cout << "  ✓ Fast encryption/decryption performance\n";
        std::cout << "  ✓ 64-bit block size with Feistel network structure\n";
        std::cout
            << "  ✓ Suitable for applications requiring fast encryption\n";
        std::cout
            << "  ⚠️  Consider AES for new applications (more standardized)\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in Blowfish example: " << e.what()
                  << "\n";
        return 1;
    }
}
