/*
 * encryption_example.cpp
 *
 * Demonstrates encryption and decryption using the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/encryption.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Encryption Example ===" << std::endl
              << std::endl;

    std::string plaintext =
        "This is a secret message that needs to be encrypted!";
    std::string password = "MySecurePassword123!";

    // ========================================================================
    // AES-256-GCM Encryption (Default)
    // ========================================================================
    std::cout << "--- AES-256-GCM Encryption ---" << std::endl;
    std::cout << "Original: " << plaintext << std::endl;

    auto encryptResult = Encryption::encrypt(plaintext, password);
    if (encryptResult.isError()) {
        std::cerr << "Encryption failed: " << encryptResult.errorMessage()
                  << std::endl;
        return 1;
    }

    std::cout << "Encrypted successfully!" << std::endl;
    std::cout << "Algorithm: "
              << Encryption::getAlgorithmName(encryptResult.value().algorithm)
              << std::endl;

    auto decryptResult = Encryption::decrypt(encryptResult.value(), password);
    if (decryptResult.isError()) {
        std::cerr << "Decryption failed: " << decryptResult.errorMessage()
                  << std::endl;
        return 1;
    }

    std::cout << "Decrypted: " << decryptResult.value() << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // AES-128-GCM Encryption
    // ========================================================================
    std::cout << "--- AES-128-GCM Encryption ---" << std::endl;

    EncryptionParams params128;
    params128.algorithm = EncryptionAlgorithm::AES_128_GCM;
    params128.keyIterations = 100000;

    auto encrypt128 = Encryption::encrypt(plaintext, password, params128);
    if (encrypt128.isSuccess()) {
        std::cout << "AES-128-GCM encryption successful" << std::endl;
        std::cout << "Key size: "
                  << Encryption::getKeySize(EncryptionAlgorithm::AES_128_GCM)
                  << " bytes" << std::endl;

        auto decrypt128 = Encryption::decrypt(encrypt128.value(), password);
        if (decrypt128.isSuccess()) {
            std::cout << "Decryption verified!" << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // ChaCha20-Poly1305 Encryption
    // ========================================================================
    std::cout << "--- ChaCha20-Poly1305 Encryption ---" << std::endl;

    if (Encryption::isAvailable(EncryptionAlgorithm::ChaCha20_Poly1305)) {
        EncryptionParams paramsChaCha;
        paramsChaCha.algorithm = EncryptionAlgorithm::ChaCha20_Poly1305;

        auto encryptChaCha =
            Encryption::encrypt(plaintext, password, paramsChaCha);
        if (encryptChaCha.isSuccess()) {
            std::cout << "ChaCha20-Poly1305 encryption successful" << std::endl;

            auto decryptChaCha =
                Encryption::decrypt(encryptChaCha.value(), password);
            if (decryptChaCha.isSuccess()) {
                std::cout << "Decryption verified!" << std::endl;
            }
        }
    } else {
        std::cout << "ChaCha20-Poly1305 not available on this system"
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Binary Data Encryption
    // ========================================================================
    std::cout << "--- Binary Data Encryption ---" << std::endl;

    std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
    std::cout << "Original binary data: ";
    for (uint8_t b : binaryData) {
        std::cout << std::hex << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;

    auto encryptBinary = Encryption::encrypt(binaryData, password);
    if (encryptBinary.isSuccess()) {
        auto decryptBinary =
            Encryption::decryptBytes(encryptBinary.value(), password);
        if (decryptBinary.isSuccess()) {
            std::cout << "Decrypted binary data: ";
            for (uint8_t b : decryptBinary.value()) {
                std::cout << std::hex << static_cast<int>(b) << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Serialization
    // ========================================================================
    std::cout << "--- Encrypted Data Serialization ---" << std::endl;

    auto encrypted = Encryption::encrypt("Serialize me!", password);
    if (encrypted.isSuccess()) {
        std::string serialized = encrypted.value().serialize();
        std::cout << "Serialized length: " << serialized.length() << " bytes"
                  << std::endl;

        auto deserialized = EncryptedData::deserialize(serialized);
        if (deserialized.isSuccess()) {
            auto decrypted =
                Encryption::decrypt(deserialized.value(), password);
            if (decrypted.isSuccess()) {
                std::cout << "Deserialized and decrypted: " << decrypted.value()
                          << std::endl;
            }
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Wrong Password Test
    // ========================================================================
    std::cout << "--- Wrong Password Test ---" << std::endl;

    auto wrongDecrypt =
        Encryption::decrypt(encryptResult.value(), "WrongPassword");
    if (wrongDecrypt.isError()) {
        std::cout << "Decryption with wrong password failed as expected: "
                  << wrongDecrypt.errorMessage() << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
