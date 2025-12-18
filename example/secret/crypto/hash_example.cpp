/*
 * hash_example.cpp
 *
 * Demonstrates cryptographic hashing using the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/hash.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Hash Example ===" << std::endl << std::endl;

    std::string data = "The quick brown fox jumps over the lazy dog";

    // ========================================================================
    // SHA-256
    // ========================================================================
    std::cout << "--- SHA-256 ---" << std::endl;
    std::cout << "Input: " << data << std::endl;

    auto sha256Result = Hash::sha256(data);
    if (sha256Result.isSuccess()) {
        std::cout << "SHA-256: " << bytesToHex(sha256Result.value())
                  << std::endl;
        std::cout << "Digest size: " << sha256Result.value().size() << " bytes"
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // SHA-384
    // ========================================================================
    std::cout << "--- SHA-384 ---" << std::endl;

    auto sha384Result = Hash::sha384(data);
    if (sha384Result.isSuccess()) {
        std::cout << "SHA-384: " << bytesToHex(sha384Result.value())
                  << std::endl;
        std::cout << "Digest size: " << sha384Result.value().size() << " bytes"
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // SHA-512
    // ========================================================================
    std::cout << "--- SHA-512 ---" << std::endl;

    auto sha512Result = Hash::sha512(data);
    if (sha512Result.isSuccess()) {
        std::cout << "SHA-512: " << bytesToHex(sha512Result.value())
                  << std::endl;
        std::cout << "Digest size: " << sha512Result.value().size() << " bytes"
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // SHA3-256
    // ========================================================================
    std::cout << "--- SHA3-256 ---" << std::endl;

    auto sha3Result = Hash::sha3_256(data);
    if (sha3Result.isSuccess()) {
        std::cout << "SHA3-256: " << bytesToHex(sha3Result.value())
                  << std::endl;
    } else {
        std::cout << "SHA3-256 not available: " << sha3Result.errorMessage()
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // BLAKE2b
    // ========================================================================
    std::cout << "--- BLAKE2b ---" << std::endl;

    auto blake2b256 = Hash::blake2b256(data);
    if (blake2b256.isSuccess()) {
        std::cout << "BLAKE2b-256: " << bytesToHex(blake2b256.value())
                  << std::endl;
    } else {
        std::cout << "BLAKE2b-256 not available" << std::endl;
    }

    auto blake2b512 = Hash::blake2b512(data);
    if (blake2b512.isSuccess()) {
        std::cout << "BLAKE2b-512: " << bytesToHex(blake2b512.value())
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Binary Data Hashing
    // ========================================================================
    std::cout << "--- Binary Data Hashing ---" << std::endl;

    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04, 0x05};
    auto binaryHash = Hash::sha256(binaryData);
    if (binaryHash.isSuccess()) {
        std::cout << "SHA-256 of binary data: "
                  << bytesToHex(binaryHash.value()) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Empty String Hash
    // ========================================================================
    std::cout << "--- Empty String Hash ---" << std::endl;

    auto emptyHash = Hash::sha256("");
    if (emptyHash.isSuccess()) {
        std::cout << "SHA-256 of empty string: "
                  << bytesToHex(emptyHash.value()) << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Hash Comparison
    // ========================================================================
    std::cout << "--- Hash Comparison ---" << std::endl;

    auto hash1 = Hash::sha256("password123");
    auto hash2 = Hash::sha256("password123");
    auto hash3 = Hash::sha256("password456");

    if (hash1.isSuccess() && hash2.isSuccess()) {
        bool same = (hash1.value() == hash2.value());
        std::cout << "Same input produces same hash: "
                  << (same ? "true" : "false") << std::endl;
    }

    if (hash1.isSuccess() && hash3.isSuccess()) {
        bool different = (hash1.value() != hash3.value());
        std::cout << "Different input produces different hash: "
                  << (different ? "true" : "false") << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Digest Sizes
    // ========================================================================
    std::cout << "--- Digest Sizes ---" << std::endl;
    std::cout << "MD5: " << Hash::getDigestSize(HashAlgorithm::MD5) << " bytes"
              << std::endl;
    std::cout << "SHA-256: " << Hash::getDigestSize(HashAlgorithm::SHA256)
              << " bytes" << std::endl;
    std::cout << "SHA-384: " << Hash::getDigestSize(HashAlgorithm::SHA384)
              << " bytes" << std::endl;
    std::cout << "SHA-512: " << Hash::getDigestSize(HashAlgorithm::SHA512)
              << " bytes" << std::endl;

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
