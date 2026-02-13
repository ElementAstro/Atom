/*
 * key_derivation_example.cpp
 *
 * Demonstrates key derivation functions using the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/key_derivation.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Key Derivation Example ===" << std::endl
              << std::endl;

    std::string password = "MySecurePassword123!";

    // ========================================================================
    // Salt Generation
    // ========================================================================
    std::cout << "--- Salt Generation ---" << std::endl;

    auto saltResult = KeyDerivation::generateSalt(16);
    if (saltResult.isSuccess()) {
        std::cout << "Generated salt (16 bytes): "
                  << bytesToHex(saltResult.value()) << std::endl;
    }

    auto salt32 = KeyDerivation::generateSalt(32);
    if (salt32.isSuccess()) {
        std::cout << "Generated salt (32 bytes): " << bytesToHex(salt32.value())
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Random Bytes Generation
    // ========================================================================
    std::cout << "--- Random Bytes ---" << std::endl;

    auto randomBytes = KeyDerivation::generateRandomBytes(32);
    if (randomBytes.isSuccess()) {
        std::cout << "Random bytes: " << bytesToHex(randomBytes.value())
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // PBKDF2-SHA256
    // ========================================================================
    std::cout << "--- PBKDF2-SHA256 ---" << std::endl;

    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};

    auto pbkdf2Result = KeyDerivation::pbkdf2Sha256(password, salt, 100000, 32);
    if (pbkdf2Result.isSuccess()) {
        std::cout << "Password: " << password << std::endl;
        std::cout << "Salt: " << bytesToHex(salt) << std::endl;
        std::cout << "Iterations: 100000" << std::endl;
        std::cout << "Derived key (32 bytes): "
                  << bytesToHex(pbkdf2Result.value()) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // PBKDF2-SHA512
    // ========================================================================
    std::cout << "--- PBKDF2-SHA512 ---" << std::endl;

    auto pbkdf2_512 = KeyDerivation::pbkdf2Sha512(password, salt, 100000, 64);
    if (pbkdf2_512.isSuccess()) {
        std::cout << "Derived key (64 bytes): "
                  << bytesToHex(pbkdf2_512.value()) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Different Iterations
    // ========================================================================
    std::cout << "--- Effect of Iterations ---" << std::endl;

    auto key1k = KeyDerivation::pbkdf2Sha256(password, salt, 1000, 32);
    auto key10k = KeyDerivation::pbkdf2Sha256(password, salt, 10000, 32);
    auto key100k = KeyDerivation::pbkdf2Sha256(password, salt, 100000, 32);

    if (key1k.isSuccess() && key10k.isSuccess() && key100k.isSuccess()) {
        std::cout << "1,000 iterations:   "
                  << bytesToHex(key1k.value()).substr(0, 32) << "..."
                  << std::endl;
        std::cout << "10,000 iterations:  "
                  << bytesToHex(key10k.value()).substr(0, 32) << "..."
                  << std::endl;
        std::cout << "100,000 iterations: "
                  << bytesToHex(key100k.value()).substr(0, 32) << "..."
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Argon2id (if available)
    // ========================================================================
    std::cout << "--- Argon2id ---" << std::endl;

    if (KeyDerivation::isArgon2Available()) {
        std::vector<uint8_t> argonSalt(16);
        auto saltGen = KeyDerivation::generateRandomBytes(16);
        if (saltGen.isSuccess()) {
            argonSalt = saltGen.value();
        }

        Argon2Params params;
        params.memoryCost = 65536;  // 64 MB
        params.timeCost = 3;
        params.parallelism = 1;

        auto argonResult =
            KeyDerivation::argon2id(password, argonSalt, 32, params);
        if (argonResult.isSuccess()) {
            std::cout << "Argon2id derived key: "
                      << bytesToHex(argonResult.value()) << std::endl;
            std::cout << "Memory cost: " << params.memoryCost << " KB"
                      << std::endl;
            std::cout << "Time cost: " << params.timeCost << std::endl;
        }
    } else {
        std::cout << "Argon2 not available on this system" << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Scrypt (if available)
    // ========================================================================
    std::cout << "--- Scrypt ---" << std::endl;

    if (KeyDerivation::isScryptAvailable()) {
        ScryptParams scryptParams;
        scryptParams.N = 16384;
        scryptParams.r = 8;
        scryptParams.p = 1;

        auto scryptResult =
            KeyDerivation::scrypt(password, salt, 32, scryptParams);
        if (scryptResult.isSuccess()) {
            std::cout << "Scrypt derived key: "
                      << bytesToHex(scryptResult.value()) << std::endl;
            std::cout << "N: " << scryptParams.N << ", r: " << scryptParams.r
                      << ", p: " << scryptParams.p << std::endl;
        }
    } else {
        std::cout << "Scrypt not available on this system" << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
