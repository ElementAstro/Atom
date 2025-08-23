/**
 * @file aes_example.cpp
 * @brief Comprehensive examples demonstrating the AES encryption utilities
 *
 * This file provides examples of all functions in the atom::utils encryption
 * module, including AES encryption/decryption, compression/decompression,
 * and various hash calculation methods.
 */

#include "atom/utils/aes.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

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

// Helper function to generate a random string of specified length
std::string generateRandomString(size_t length) {
    static const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }

    return result;
}

// Helper function to print binary data as hex
void printHex(const std::string& title,
              const std::vector<unsigned char>& data) {
    std::cout << title << ": ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}

void printHex(const std::string& title, std::span<const unsigned char> data) {
    std::cout << title << ": ";
    for (const auto& byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper function to create a test file with content
bool createTestFile(const std::string& filename, const std::string& content) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        file.close();
        return true;
    } catch (...) {
        return false;
    }
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  AES Encryption Utilities Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: AES Encryption and Decryption
        printSection("1. AES Encryption and Decryption");

        // Basic encryption and decryption
        printSubsection("Basic Encryption and Decryption");

        const std::string plaintext =
            "This is a sensitive message that needs encryption!";
        std::cout << "Original plaintext: " << plaintext << std::endl;

        // Use a secure key (in practice, this should be properly generated and
        // stored)
        const std::string key = "ThisIsA32ByteKeyForAES256Encrypt";
        std::vector<unsigned char> iv;   // Initialization vector
        std::vector<unsigned char> tag;  // Authentication tag

        // Encrypt the plaintext
        std::string ciphertext =
            atom::utils::encryptAES(plaintext, key, iv, tag);

        std::cout << "Plaintext length: " << plaintext.length() << " bytes"
                  << std::endl;
        std::cout << "Ciphertext length: " << ciphertext.length() << " bytes"
                  << std::endl;

        // Print the IV and tag values
        printHex("Initialization Vector (IV)", iv);
        printHex("Authentication Tag", tag);

        // Decrypt the ciphertext
        std::string decrypted =
            atom::utils::decryptAES(ciphertext, key, iv, tag);
        std::cout << "Decrypted text: " << decrypted << std::endl;

        // Verify that decryption was successful
        if (decrypted == plaintext) {
            std::cout << "✓ Decryption successful - the decrypted text matches "
                         "the original plaintext"
                      << std::endl;
        } else {
            std::cout << "✗ Decryption failed - the decrypted text does not "
                         "match the original plaintext"
                      << std::endl;
        }

        // Encrypt a larger text
        printSubsection("Encrypting Larger Text");

        // Generate a larger text
        std::string largeText = generateRandomString(1024);  // 1KB of data
        std::cout << "Generated " << largeText.length()
                  << " bytes of random text" << std::endl;

        // Encrypt the large text
        std::vector<unsigned char> largeIv;
        std::vector<unsigned char> largeTag;
        std::string largeCiphertext =
            atom::utils::encryptAES(largeText, key, largeIv, largeTag);

        std::cout << "Large plaintext length: " << largeText.length()
                  << " bytes" << std::endl;
        std::cout << "Large ciphertext length: " << largeCiphertext.length()
                  << " bytes" << std::endl;

        // Decrypt the large ciphertext
        std::string largeDecrypted =
            atom::utils::decryptAES(largeCiphertext, key, largeIv, largeTag);

        // Verify large text decryption
        if (largeDecrypted == largeText) {
            std::cout << "✓ Large text decryption successful" << std::endl;
        } else {
            std::cout << "✗ Large text decryption failed" << std::endl;
        }

        // Example with different key lengths
        printSubsection("Different Key Lengths");

        // Test with a shorter key (will likely be padded or hashed internally)
        const std::string shortKey = "ShortKey";
        std::vector<unsigned char> shortKeyIv;
        std::vector<unsigned char> shortKeyTag;

        try {
            std::string shortKeyCiphertext = atom::utils::encryptAES(
                plaintext, shortKey, shortKeyIv, shortKeyTag);
            std::cout << "Encryption with short key successful." << std::endl;

            // Decrypt with the short key
            std::string shortKeyDecrypted = atom::utils::decryptAES(
                shortKeyCiphertext, shortKey, shortKeyIv, shortKeyTag);

            if (shortKeyDecrypted == plaintext) {
                std::cout << "✓ Short key decryption successful" << std::endl;
            } else {
                std::cout << "✗ Short key decryption failed" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Short key encryption failed: " << e.what()
                      << std::endl;
        }

        // Test with invalid decryption parameters
        printSubsection("Error Handling");

        try {
            // Attempt to decrypt with an incorrect key
            std::string wrongKey = "ThisIsTheWrongKeyForDecryption!";
            std::string failedDecrypt =
                atom::utils::decryptAES(ciphertext, wrongKey, iv, tag);
            std::cout << "Warning: Decryption with wrong key did not throw an "
                         "exception"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✓ Expected exception caught with wrong key: "
                      << e.what() << std::endl;
        }

        try {
            // Attempt to decrypt with modified ciphertext
            std::string modifiedCiphertext = ciphertext;
            if (!modifiedCiphertext.empty()) {
                modifiedCiphertext[0] = modifiedCiphertext[0] ^
                                        0xFF;  // Flip bits in the first byte
            }

            std::string failedDecrypt =
                atom::utils::decryptAES(modifiedCiphertext, key, iv, tag);
            std::cout << "Warning: Decryption with modified ciphertext did not "
                         "throw an exception"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout
                << "✓ Expected exception caught with modified ciphertext: "
                << e.what() << std::endl;
        }

        // Example 2: Compression and Decompression
        printSection("2. Compression and Decompression");

        // Basic compression
        printSubsection("Basic Compression");

        std::string compressibleText =
            "This is a test string that contains repeated text. "
            "This is a test string that contains repeated text. "
            "This is a test string that contains repeated text. "
            "This is a test string that contains repeated text. ";

        std::cout << "Original text length: " << compressibleText.length()
                  << " bytes" << std::endl;

        // Compress the text
        std::string compressed = atom::utils::compress(compressibleText);
        std::cout << "Compressed text length: " << compressed.length()
                  << " bytes" << std::endl;
        std::cout << "Compression ratio: "
                  << static_cast<double>(compressibleText.length()) /
                         compressed.length()
                  << std::endl;

        // Decompress the compressed text
        std::string decompressed = atom::utils::decompress(compressed);
        std::cout << "Decompressed text length: " << decompressed.length()
                  << " bytes" << std::endl;

        // Verify that decompression was successful
        if (decompressed == compressibleText) {
            std::cout << "✓ Decompression successful - the decompressed text "
                         "matches the original"
                      << std::endl;
        } else {
            std::cout << "✗ Decompression failed - the decompressed text does "
                         "not match the original"
                      << std::endl;
        }

        // Compress random data (typically less compressible)
        printSubsection("Compressing Random Data");

        std::string randomData = generateRandomString(1024);
        std::cout << "Random data length: " << randomData.length() << " bytes"
                  << std::endl;

        std::string compressedRandom = atom::utils::compress(randomData);
        std::cout << "Compressed random data length: "
                  << compressedRandom.length() << " bytes" << std::endl;
        std::cout << "Compression ratio: "
                  << static_cast<double>(randomData.length()) /
                         compressedRandom.length()
                  << std::endl;

        std::string decompressedRandom =
            atom::utils::decompress(compressedRandom);

        // Verify random data decompression
        if (decompressedRandom == randomData) {
            std::cout << "✓ Random data decompression successful" << std::endl;
        } else {
            std::cout << "✗ Random data decompression failed" << std::endl;
        }

        // Error handling for decompression
        printSubsection("Compression Error Handling");

        try {
            // Try to decompress invalid data
            std::string invalidCompressed = "This is not valid compressed data";
            std::string failedDecompress =
                atom::utils::decompress(invalidCompressed);
            std::cout << "Warning: Decompression of invalid data did not throw "
                         "an exception"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout
                << "✓ Expected exception caught with invalid compressed data: "
                << e.what() << std::endl;
        }

        // Example 3: Combined Encryption and Compression
        printSection("3. Combined Encryption and Compression");

        // Compress then encrypt
        printSubsection("Compress then Encrypt");

        std::string originalText =
            "This is a message that will be compressed and then encrypted. "
            "Compressing before encryption often results in better security "
            "since compression removes patterns that could be exploited in "
            "cryptanalysis. This message contains repeated patterns to "
            "demonstrate "
            "effective compression.";

        std::cout << "Original text length: " << originalText.length()
                  << " bytes" << std::endl;

        // First compress
        std::string compressedText = atom::utils::compress(originalText);
        std::cout << "Compressed length: " << compressedText.length()
                  << " bytes" << std::endl;

        // Then encrypt
        std::vector<unsigned char> combinedIv;
        std::vector<unsigned char> combinedTag;
        std::string encryptedCompressed = atom::utils::encryptAES(
            compressedText, key, combinedIv, combinedTag);
        std::cout << "Encrypted compressed length: "
                  << encryptedCompressed.length() << " bytes" << std::endl;

        // Now decrypt
        std::string decryptedCompressed = atom::utils::decryptAES(
            encryptedCompressed, key, combinedIv, combinedTag);

        // Finally decompress
        std::string finalText = atom::utils::decompress(decryptedCompressed);

        // Verify the result
        if (finalText == originalText) {
            std::cout << "✓ Combined compression and encryption successful"
                      << std::endl;
        } else {
            std::cout << "✗ Combined compression and encryption failed"
                      << std::endl;
        }

        // Example 4: SHA Hash Functions
        printSection("4. SHA Hash Functions");

        // SHA-256 file hash
        printSubsection("SHA-256 File Hash");

        // Create a test file for hashing
        const std::string testFileName = "test_hash_file.txt";
        const std::string fileContent =
            "This is a test file for SHA-256 hashing.\n"
            "The SHA-256 algorithm produces a 256-bit (32-byte) hash value.\n"
            "It's commonly used for verifying file integrity.";

        if (createTestFile(testFileName, fileContent)) {
            std::cout << "Test file created: " << testFileName << std::endl;

            // Calculate the SHA-256 hash of the file
            std::string fileHash = atom::utils::calculateSha256(testFileName);
            std::cout << "SHA-256 hash of file: " << fileHash << std::endl;
        } else {
            std::cout << "Failed to create test file" << std::endl;
        }

        // SHA-224 string hash
        printSubsection("SHA-224 String Hash");

        const std::string testString =
            "This is a test string for SHA-224 hashing.";
        std::cout << "Test string: " << testString << std::endl;

        // Calculate the SHA-224 hash of the string
        std::string sha224Hash = atom::utils::calculateSha224(testString);
        std::cout << "SHA-224 hash: " << sha224Hash << std::endl;
        std::cout << "Hash length: " << sha224Hash.length() / 2 << " bytes ("
                  << sha224Hash.length() << " hex characters)" << std::endl;

        // SHA-384 string hash
        printSubsection("SHA-384 String Hash");

        // Calculate the SHA-384 hash of the string
        std::string sha384Hash = atom::utils::calculateSha384(testString);
        std::cout << "SHA-384 hash: " << sha384Hash << std::endl;
        std::cout << "Hash length: " << sha384Hash.length() / 2 << " bytes ("
                  << sha384Hash.length() << " hex characters)" << std::endl;

        // SHA-512 string hash
        printSubsection("SHA-512 String Hash");

        // Calculate the SHA-512 hash of the string
        std::string sha512Hash = atom::utils::calculateSha512(testString);
        std::cout << "SHA-512 hash: " << sha512Hash << std::endl;
        std::cout << "Hash length: " << sha512Hash.length() / 2 << " bytes ("
                  << sha512Hash.length() << " hex characters)" << std::endl;

        // Compare hash functions on the same input
        printSubsection("Comparing Different Hash Functions");

        const std::string compareInput =
            "The quick brown fox jumps over the lazy dog";
        std::cout << "Input string: " << compareInput << std::endl;

        std::cout << "SHA-224: " << atom::utils::calculateSha224(compareInput)
                  << std::endl;
        std::cout << "SHA-384: " << atom::utils::calculateSha384(compareInput)
                  << std::endl;
        std::cout << "SHA-512: " << atom::utils::calculateSha512(compareInput)
                  << std::endl;

        // Example 5: String Like Types (Testing the concept)
        printSection("5. Testing StringLike Concept");

        // Test with std::string
        printSubsection("std::string");
        std::string stdString = "Testing with std::string";
        std::vector<unsigned char> conceptIv;
        std::vector<unsigned char> conceptTag;

        std::string encryptedStdString =
            atom::utils::encryptAES(stdString, key, conceptIv, conceptTag);
        std::cout << "Successfully encrypted std::string" << std::endl;

        // Test with string literal
        printSubsection("String Literal");
        auto encryptedLiteral = atom::utils::encryptAES(
            "Testing with string literal", key, conceptIv, conceptTag);
        std::cout << "Successfully encrypted string literal" << std::endl;

        // Test with const char*
        printSubsection("const char*");
        const char* cString = "Testing with const char*";
        auto encryptedCString =
            atom::utils::encryptAES(cString, key, conceptIv, conceptTag);
        std::cout << "Successfully encrypted const char*" << std::endl;

        // Test with std::string_view
        printSubsection("std::string_view");
        std::string_view stringView = "Testing with std::string_view";
        auto encryptedStringView =
            atom::utils::encryptAES(stringView, key, conceptIv, conceptTag);
        std::cout << "Successfully encrypted std::string_view" << std::endl;

        // Example 6: Error cases and exception handling
        printSection("6. Error Cases and Exception Handling");

        // Empty string encryption
        printSubsection("Empty String Encryption");
        try {
            std::string emptyString = "";
            std::vector<unsigned char> emptyIv;
            std::vector<unsigned char> emptyTag;

            std::string encryptedEmpty =
                atom::utils::encryptAES(emptyString, key, emptyIv, emptyTag);
            std::cout << "Empty string encryption result size: "
                      << encryptedEmpty.length() << " bytes" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught during empty string encryption: "
                      << e.what() << std::endl;
        }

        // Empty key
        printSubsection("Empty Key");
        try {
            std::string emptyKey = "";
            std::vector<unsigned char> emptyKeyIv;
            std::vector<unsigned char> emptyKeyTag;

            atom::utils::encryptAES(plaintext, emptyKey, emptyKeyIv,
                                    emptyKeyTag);
            std::cout << "Warning: Encryption with empty key did not throw an "
                         "exception"
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✓ Expected exception caught with empty key: "
                      << e.what() << std::endl;
        }

        // Hash of empty string
        printSubsection("Hash of Empty String");
        try {
            std::string emptyStringHash = atom::utils::calculateSha224("");
            std::cout << "SHA-224 hash of empty string: " << emptyStringHash
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Exception caught while hashing empty string: "
                      << e.what() << std::endl;
        }

        // Non-existent file hash
        printSubsection("Non-existent File Hash");
        try {
            std::string nonExistentFileName = "file_that_does_not_exist.txt";
            std::string nonExistentFileHash =
                atom::utils::calculateSha256(nonExistentFileName);

            if (nonExistentFileHash.empty()) {
                std::cout << "✓ Returned empty hash for non-existent file (as "
                             "expected)"
                          << std::endl;
            } else {
                std::cout << "Unexpected hash for non-existent file: "
                          << nonExistentFileHash << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Exception caught for non-existent file: " << e.what()
                      << std::endl;
        }

        // Clean up test file
        std::remove(testFileName.c_str());
        std::cout << "\nTest file removed." << std::endl;

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
