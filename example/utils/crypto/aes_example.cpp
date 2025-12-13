/**
 * @file aes_example.cpp
 * @brief Examples for atom::utils crypto utilities (AES, SHA, compression)
 */

#include "atom/utils/crypto/aes.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void printHex(const std::string& label, const std::vector<unsigned char>& data) {
    std::cout << label << ": ";
    for (auto byte : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

void demonstrateAESEncryption() {
    printSection("1. AES Encryption/Decryption");

    std::string plaintext = "Hello, World! This is a secret message.";
    std::string key = "0123456789abcdef0123456789abcdef";  // 32 bytes for AES-256

    std::cout << "Original: \"" << plaintext << "\"" << std::endl;
    std::cout << "Key length: " << key.length() << " bytes" << std::endl;

    try {
        std::vector<unsigned char> iv;
        std::vector<unsigned char> tag;

        std::string ciphertext = encryptAES(plaintext, key, iv, tag);
        std::cout << "\nEncryption successful!" << std::endl;
        std::cout << "Ciphertext length: " << ciphertext.length() << " bytes" << std::endl;
        printHex("IV", iv);
        printHex("Tag", tag);

        std::string decrypted = decryptAES(ciphertext, key, iv, tag);
        std::cout << "\nDecrypted: \"" << decrypted << "\"" << std::endl;
        std::cout << "Match: " << (plaintext == decrypted ? "Yes" : "No") << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void demonstrateCompression() {
    printSection("2. Compression/Decompression");

    std::string original = "This is a test string that will be compressed. "
                           "Repeated text helps compression: "
                           "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

    std::cout << "Original size: " << original.length() << " bytes" << std::endl;

    try {
        std::string compressed = compress(original);
        std::cout << "Compressed size: " << compressed.length() << " bytes" << std::endl;
        std::cout << "Compression ratio: "
                  << std::fixed << std::setprecision(2)
                  << (1.0 - static_cast<double>(compressed.length()) / original.length()) * 100
                  << "%" << std::endl;

        std::string decompressed = decompress(compressed);
        std::cout << "Decompressed size: " << decompressed.length() << " bytes" << std::endl;
        std::cout << "Match: " << (original == decompressed ? "Yes" : "No") << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void demonstrateSHA() {
    printSection("3. SHA Hash Functions");

    std::string data = "Hello, World!";
    std::cout << "Input: \"" << data << "\"" << std::endl;

    std::cout << "\n--- SHA-224 ---" << std::endl;
    std::string sha224 = calculateSha224(data);
    std::cout << "Hash: " << sha224 << std::endl;

    std::cout << "\n--- SHA-384 ---" << std::endl;
    std::string sha384 = calculateSha384(data);
    std::cout << "Hash: " << sha384 << std::endl;

    std::cout << "\n--- SHA-512 ---" << std::endl;
    std::string sha512 = calculateSha512(data);
    std::cout << "Hash: " << sha512 << std::endl;

    std::cout << "\n--- Hash Comparison ---" << std::endl;
    std::string data2 = "Hello, World!";
    std::string data3 = "Hello, World?";

    std::cout << "Same input produces same hash: "
              << (calculateSha224(data) == calculateSha224(data2) ? "Yes" : "No") << std::endl;
    std::cout << "Different input produces different hash: "
              << (calculateSha224(data) != calculateSha224(data3) ? "Yes" : "No") << std::endl;
}

void demonstrateSecureDataStorage() {
    printSection("4. Secure Data Storage Example");

    struct SecureData {
        std::string data;
        std::vector<unsigned char> iv;
        std::vector<unsigned char> tag;
        std::string hash;
    };

    std::string sensitiveData = "Credit Card: 1234-5678-9012-3456";
    std::string masterKey = "MySecretMasterKey123456789012345";

    std::cout << "Storing sensitive data securely..." << std::endl;

    try {
        SecureData stored;
        stored.hash = calculateSha224(sensitiveData);
        stored.data = encryptAES(sensitiveData, masterKey, stored.iv, stored.tag);

        std::cout << "Data encrypted and stored" << std::endl;
        std::cout << "Hash for integrity: " << stored.hash.substr(0, 32) << "..." << std::endl;

        std::cout << "\nRetrieving and verifying..." << std::endl;
        std::string retrieved = decryptAES(stored.data, masterKey, stored.iv, stored.tag);
        std::string verifyHash = calculateSha224(retrieved);

        std::cout << "Decrypted successfully" << std::endl;
        std::cout << "Integrity check: " << (stored.hash == verifyHash ? "PASSED" : "FAILED") << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Crypto Utilities Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateAESEncryption();
        demonstrateCompression();
        demonstrateSHA();
        demonstrateSecureDataStorage();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All crypto examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
