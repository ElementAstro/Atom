/**
 * @file aes_example.cpp
 * @brief Comprehensive examples demonstrating AES cryptographic utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::crypto/aes.hpp:
 * - AES encryption and decryption with proper key/IV handling
 * - Data compression and decompression using Zlib
 * - SHA hash calculations (SHA-224, SHA-256, SHA-384, SHA-512)
 * - File integrity verification
 * - Secure data transmission simulation
 * - Error handling and security best practices
 */

#include "atom/utils/crypto/aes.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to print hex data
void printHex(const std::string& label,
              const std::vector<unsigned char>& data) {
    std::cout << label << ": ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

// Helper function to print hex string
void printHex(const std::string& label, const std::string& data) {
    std::cout << label << ": ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(static_cast<unsigned char>(byte));
    }
    std::cout << std::dec << std::endl;
}

// Generate a random key for demonstration
std::string generateRandomKey(size_t length = 32) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string key;
    key.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        key += static_cast<char>(dis(gen));
    }
    return key;
}

// Create a temporary test file
void createTestFile(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    } else {
        std::cerr << "Failed to create test file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "==========================================" << std::endl;
        std::cout << "  AES Cryptographic Utilities Demo" << std::endl;
        std::cout << "==========================================" << std::endl;

        // ============================
        // Example 1: Basic AES Encryption/Decryption
        // ============================
        printSection("1. Basic AES Encryption and Decryption");

        printSubsection("AES-256-GCM Encryption");

        // Prepare test data
        std::string plaintext =
            "Hello, World! This is a secret message that needs to be "
            "encrypted.";
        std::string key = generateRandomKey(32);  // 256-bit key
        std::vector<unsigned char> iv, tag;

        std::cout << "Original plaintext: " << plaintext << std::endl;
        std::cout << "Key length: " << key.length() << " bytes" << std::endl;

        // Encrypt the data
        std::string ciphertext = encryptAES(plaintext, key, iv, tag);

        std::cout << "Encryption successful!" << std::endl;
        std::cout << "Ciphertext length: " << ciphertext.length() << " bytes"
                  << std::endl;
        printHex("IV", iv);
        printHex("Authentication Tag", tag);
        printHex("Ciphertext (first 32 bytes)",
                 ciphertext.substr(
                     0, std::min(32, static_cast<int>(ciphertext.length()))));

        printSubsection("AES-256-GCM Decryption");

        // Decrypt the data
        std::string decrypted = decryptAES(ciphertext, key, iv, tag);

        std::cout << "Decryption successful!" << std::endl;
        std::cout << "Decrypted text: " << decrypted << std::endl;
        std::cout << "Decryption matches original: "
                  << (decrypted == plaintext ? "YES" : "NO") << std::endl;

        // ============================
        // Example 2: Data Compression
        // ============================
        printSection("2. Data Compression and Decompression");

        printSubsection("Zlib Compression");

        std::string largeText =
            "This is a sample text that will be compressed using Zlib. ";
        // Repeat to make it larger for better compression demonstration
        for (int i = 0; i < 10; ++i) {
            largeText += largeText;
        }

        std::cout << "Original size: " << largeText.length() << " bytes"
                  << std::endl;

        std::string compressed = compress(largeText);
        std::cout << "Compressed size: " << compressed.length() << " bytes"
                  << std::endl;
        std::cout << "Compression ratio: " << std::fixed << std::setprecision(2)
                  << (100.0 * compressed.length() / largeText.length()) << "%"
                  << std::endl;

        printSubsection("Zlib Decompression");

        std::string decompressed = decompress(compressed);
        std::cout << "Decompressed size: " << decompressed.length() << " bytes"
                  << std::endl;
        std::cout << "Decompression matches original: "
                  << (decompressed == largeText ? "YES" : "NO") << std::endl;

        // ============================
        // Example 3: SHA Hash Calculations
        // ============================
        printSection("3. SHA Hash Calculations");

        std::string testData = "The quick brown fox jumps over the lazy dog";
        std::cout << "Test data: " << testData << std::endl;

        printSubsection("SHA-224 Hash");
        try {
            std::string sha224 = calculateSha224(testData);
            std::cout << "SHA-224: " << sha224 << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "SHA-224 error: " << e.what() << std::endl;
        }

        printSubsection("File SHA-256 Hash");
        // Create a test file
        std::string testFilename = "test_file.txt";
        createTestFile(testFilename, testData);

        try {
            std::string sha256 = calculateSha256(testFilename);
            std::cout << "SHA-256 of file: " << sha256 << std::endl;

            // Clean up
            std::remove(testFilename.c_str());
            std::cout << "Test file cleaned up." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "SHA-256 error: " << e.what() << std::endl;
        }

        // ============================
        // Example 4: Secure Data Transmission Simulation
        // ============================
        printSection("4. Secure Data Transmission Simulation");

        printSubsection("Sender Side - Encrypt and Compress");

        std::string message =
            "Confidential business data: Q4 revenue increased by 15%. "
            "New product launch scheduled for next quarter.";
        std::string sessionKey = generateRandomKey(32);

        std::cout << "Original message: " << message << std::endl;
        std::cout << "Message size: " << message.length() << " bytes"
                  << std::endl;

        // First compress, then encrypt (common practice)
        std::string compressedMsg = compress(message);
        std::cout << "Compressed size: " << compressedMsg.length() << " bytes"
                  << std::endl;

        std::vector<unsigned char> transmissionIv, transmissionTag;
        std::string encryptedMsg = encryptAES(compressedMsg, sessionKey,
                                              transmissionIv, transmissionTag);

        std::cout << "Encrypted size: " << encryptedMsg.length() << " bytes"
                  << std::endl;
        std::cout << "Total transmission overhead: "
                  << (transmissionIv.size() + transmissionTag.size())
                  << " bytes" << std::endl;

        printSubsection("Receiver Side - Decrypt and Decompress");

        // Simulate transmission (in real scenario, IV and tag would be
        // transmitted separately)
        std::string receivedCiphertext = encryptedMsg;
        std::vector<unsigned char> receivedIv = transmissionIv;
        std::vector<unsigned char> receivedTag = transmissionTag;

        // Decrypt first
        std::string decryptedCompressed =
            decryptAES(receivedCiphertext, sessionKey, receivedIv, receivedTag);
        std::cout << "Decrypted compressed size: "
                  << decryptedCompressed.length() << " bytes" << std::endl;

        // Then decompress
        std::string finalMessage = decompress(decryptedCompressed);
        std::cout << "Final message: " << finalMessage << std::endl;
        std::cout << "Transmission integrity: "
                  << (finalMessage == message ? "VERIFIED" : "FAILED")
                  << std::endl;

        // ============================
        // Example 5: Error Handling and Edge Cases
        // ============================
        printSection("5. Error Handling and Edge Cases");

        printSubsection("Invalid Key Length");
        try {
            std::string shortKey = "short";
            std::vector<unsigned char> dummyIv, dummyTag;
            [[maybe_unused]] auto result =
                encryptAES("test", shortKey, dummyIv, dummyTag);
        } catch (const std::exception& e) {
            std::cout << "Caught expected error for short key: " << e.what()
                      << std::endl;
        }

        printSubsection("Empty Data Handling");
        try {
            std::string emptyData = "";
            std::string compressed = compress(emptyData);
            std::cout << "Empty data compression result length: "
                      << compressed.length() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Empty data compression error: " << e.what()
                      << std::endl;
        }

        printSubsection("Non-existent File Hash");
        try {
            std::string hash = calculateSha256("non_existent_file.txt");
            std::cout << "Non-existent file hash: " << hash << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Expected error for non-existent file: " << e.what()
                      << std::endl;
        }

        // ============================
        // Example 6: Performance Considerations
        // ============================
        printSection("6. Performance Considerations");

        printSubsection("Large Data Encryption Performance");

        // Create larger test data
        std::string largeData;
        largeData.reserve(1024 * 1024);  // 1MB
        for (int i = 0; i < 1024; ++i) {
            largeData += "This is line " + std::to_string(i) +
                         " of test data for performance testing.\n";
        }

        std::cout << "Large data size: " << largeData.length() << " bytes"
                  << std::endl;

        auto start = std::chrono::high_resolution_clock::now();

        std::string perfKey = generateRandomKey(32);
        std::vector<unsigned char> perfIv, perfTag;
        std::string encryptedLarge =
            encryptAES(largeData, perfKey, perfIv, perfTag);

        auto encrypt_end = std::chrono::high_resolution_clock::now();

        std::string decryptedLarge =
            decryptAES(encryptedLarge, perfKey, perfIv, perfTag);

        auto decrypt_end = std::chrono::high_resolution_clock::now();

        auto encrypt_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(encrypt_end -
                                                                  start);
        auto decrypt_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(decrypt_end -
                                                                  encrypt_end);

        std::cout << "Encryption time: " << encrypt_time.count() << " ms"
                  << std::endl;
        std::cout << "Decryption time: " << decrypt_time.count() << " ms"
                  << std::endl;
        std::cout << "Encryption throughput: " << std::fixed
                  << std::setprecision(2)
                  << (largeData.length() / 1024.0 / 1024.0) /
                         (encrypt_time.count() / 1000.0)
                  << " MB/s" << std::endl;
        std::cout << "Data integrity: "
                  << (decryptedLarge == largeData ? "VERIFIED" : "FAILED")
                  << std::endl;

        std::cout << "\nAll cryptographic examples completed successfully!"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
