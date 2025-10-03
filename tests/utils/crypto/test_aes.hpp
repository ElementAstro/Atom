#ifndef ATOM_UTILS_TEST_AES_HPP
#define ATOM_UTILS_TEST_AES_HPP

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <random>
#include <future>
#include <chrono>
#include "atom/utils/crypto/aes.hpp"

namespace atom::utils::test {

class AESTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate random test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        testData_.reserve(1024);
        for (int i = 0; i < 1024; ++i) {
            testData_.push_back(static_cast<char>(dis(gen)));
        }

        // Create test key (32 bytes for AES-256)
        key_ = "0123456789abcdef0123456789abcdef";
    }

    std::string testData_;
    std::string key_;
};

TEST_F(AESTest, EncryptionDecryption) {
    std::vector<unsigned char> iv, tag;

    // Test encryption
    std::string ciphertext = encryptAES(testData_, key_, iv, tag);
    ASSERT_FALSE(ciphertext.empty());
    EXPECT_NE(ciphertext, testData_);
    EXPECT_EQ(iv.size(), 12);   // GCM IV size
    EXPECT_EQ(tag.size(), 16);  // GCM tag size

    // Test decryption
    std::string decrypted = decryptAES(ciphertext, key_, iv, tag);
    EXPECT_EQ(decrypted, testData_);
}

TEST_F(AESTest, EncryptionWithEmptyInput) {
    std::vector<unsigned char> iv, tag;
    EXPECT_THROW(encryptAES("", key_, iv, tag), std::invalid_argument);
}

TEST_F(AESTest, EncryptionWithInvalidKey) {
    std::vector<unsigned char> iv, tag;
    EXPECT_THROW(encryptAES(testData_, "", iv, tag), std::invalid_argument);
}

TEST_F(AESTest, DecryptionWithInvalidTag) {
    std::vector<unsigned char> iv, tag;
    std::string ciphertext = encryptAES(testData_, key_, iv, tag);

    // Corrupt tag
    tag[0] ^= 0xFF;
    EXPECT_THROW(decryptAES(ciphertext, key_, iv, tag), std::runtime_error);
}

TEST_F(AESTest, CompressionDecompression) {
    std::string compressed = compress(testData_);
    ASSERT_FALSE(compressed.empty());
    EXPECT_LT(compressed.size(), testData_.size());

    std::string decompressed = decompress(compressed);
    EXPECT_EQ(decompressed, testData_);
}

TEST_F(AESTest, CompressionWithEmptyInput) {
    EXPECT_THROW(compress(""), std::invalid_argument);
}

TEST_F(AESTest, DecompressionWithInvalidInput) {
    std::string invalidData = "Invalid compressed data";
    EXPECT_THROW(decompress(invalidData), std::runtime_error);
}

TEST_F(AESTest, SHA256FileHashing) {
    // Create temporary file
    std::string filename = "test_file.txt";
    std::ofstream file(filename);
    file << testData_;
    file.close();

    std::string hash = calculateSha256(filename);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64);  // SHA-256 produces 32 bytes = 64 hex chars

    // Cleanup
    std::filesystem::remove(filename);
}

TEST_F(AESTest, SHA256NonexistentFile) {
    std::string hash = calculateSha256("nonexistent_file.txt");
    EXPECT_TRUE(hash.empty());
}

TEST_F(AESTest, SHA224String) {
    std::string hash = calculateSha224(testData_);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 56);  // SHA-224 produces 28 bytes = 56 hex chars
}

TEST_F(AESTest, SHA384String) {
    std::string hash = calculateSha384(testData_);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 96);  // SHA-384 produces 48 bytes = 96 hex chars
}

TEST_F(AESTest, SHA512String) {
    std::string hash = calculateSha512(testData_);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 128);  // SHA-512 produces 64 bytes = 128 hex chars
}

TEST_F(AESTest, HashEmptyString) {
    EXPECT_TRUE(calculateSha224("").empty());
    EXPECT_TRUE(calculateSha384("").empty());
    EXPECT_TRUE(calculateSha512("").empty());
}

TEST_F(AESTest, LargeDataEncryption) {
    // Test with 1MB of data
    std::string largeData(1024 * 1024, 'A');
    std::vector<unsigned char> iv, tag;

    std::string ciphertext = encryptAES(largeData, key_, iv, tag);
    std::string decrypted = decryptAES(ciphertext, key_, iv, tag);

    EXPECT_EQ(decrypted, largeData);
}

TEST_F(AESTest, MultipleEncryptions) {
    std::vector<unsigned char> iv1, tag1;
    std::vector<unsigned char> iv2, tag2;

    std::string ciphertext1 = encryptAES(testData_, key_, iv1, tag1);
    std::string ciphertext2 = encryptAES(testData_, key_, iv2, tag2);

    // IVs should be different for each encryption
    EXPECT_NE(iv1, iv2);
    // Ciphertexts should be different due to different IVs
    EXPECT_NE(ciphertext1, ciphertext2);

    // Both should decrypt to the same plaintext
    EXPECT_EQ(decryptAES(ciphertext1, key_, iv1, tag1),
              decryptAES(ciphertext2, key_, iv2, tag2));
}

TEST_F(AESTest, CompressionRatio) {
    // Create highly compressible data
    std::string compressibleData(1000, 'A');
    std::string compressed = compress(compressibleData);

    // Expect significant compression
    EXPECT_LT(compressed.size(), compressibleData.size() / 2);
}

// Test edge cases for encryption/decryption
TEST_F(AESTest, EncryptionEdgeCases) {
    std::vector<unsigned char> iv, tag;

    // Test with minimum key size (16 bytes for AES-128)
    std::string shortKey = "0123456789abcdef";
    EXPECT_NO_THROW(encryptAES(testData_, shortKey, iv, tag));

    // Test with maximum practical data size
    std::string largeData(10 * 1024 * 1024, 'X'); // 10MB
    EXPECT_NO_THROW(encryptAES(largeData, key_, iv, tag));

    // Test with data containing null bytes
    std::string nullData = "Hello\0World\0Test";
    nullData.resize(15); // Ensure null bytes are included
    std::string ciphertext = encryptAES(nullData, key_, iv, tag);
    std::string decrypted = decryptAES(ciphertext, key_, iv, tag);
    EXPECT_EQ(decrypted, nullData);

    // Test with binary data
    std::string binaryData;
    for (int i = 0; i < 256; ++i) {
        binaryData += static_cast<char>(i);
    }
    ciphertext = encryptAES(binaryData, key_, iv, tag);
    decrypted = decryptAES(ciphertext, key_, iv, tag);
    EXPECT_EQ(decrypted, binaryData);
}

// Test error handling for various invalid inputs
TEST_F(AESTest, ErrorHandlingComprehensive) {
    std::vector<unsigned char> iv, tag;

    // Test with various invalid key sizes
    EXPECT_THROW(encryptAES(testData_, "short", iv, tag), std::invalid_argument);
    EXPECT_THROW(encryptAES(testData_, "toolongkey123456789012345678901234567890", iv, tag), std::invalid_argument);

    // Test decryption with wrong key
    std::string ciphertext = encryptAES(testData_, key_, iv, tag);
    std::string wrongKey = "wrongkey0123456789abcdef0123456";
    EXPECT_THROW(decryptAES(ciphertext, wrongKey, iv, tag), std::runtime_error);

    // Test decryption with corrupted ciphertext
    std::string corruptedCiphertext = ciphertext;
    if (!corruptedCiphertext.empty()) {
        corruptedCiphertext[0] ^= 0xFF;
        EXPECT_THROW(decryptAES(corruptedCiphertext, key_, iv, tag), std::runtime_error);
    }

    // Test decryption with wrong IV
    std::vector<unsigned char> wrongIV = iv;
    if (!wrongIV.empty()) {
        wrongIV[0] ^= 0xFF;
        EXPECT_THROW(decryptAES(ciphertext, key_, wrongIV, tag), std::runtime_error);
    }
}

// Test compression edge cases
TEST_F(AESTest, CompressionEdgeCases) {
    // Test with already compressed data (should not compress well)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string randomData;
    randomData.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        randomData += static_cast<char>(dis(gen));
    }

    std::string compressed = compress(randomData);
    // Random data typically doesn't compress well
    EXPECT_GE(compressed.size(), randomData.size() * 0.8); // Allow some compression

    // Test with very small data
    std::string smallData = "Hi";
    EXPECT_NO_THROW(compress(smallData));

    // Test with repetitive patterns
    std::string pattern = "ABCD";
    std::string repetitiveData;
    for (int i = 0; i < 250; ++i) {
        repetitiveData += pattern;
    }
    std::string compressedPattern = compress(repetitiveData);
    EXPECT_LT(compressedPattern.size(), repetitiveData.size() / 4); // Should compress very well
}

// Test hash function edge cases
TEST_F(AESTest, HashEdgeCases) {
    // Test with very large file (create temporary large file)
    std::string largeFilename = "large_test_file.txt";
    std::ofstream largeFile(largeFilename, std::ios::binary);
    std::string chunk(1024, 'A');
    for (int i = 0; i < 1024; ++i) { // 1MB file
        largeFile << chunk;
    }
    largeFile.close();

    std::string hash = calculateSha256(largeFilename);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64);

    // Cleanup
    std::filesystem::remove(largeFilename);

    // Test hash consistency
    std::string data = "Consistent test data";
    std::string hash1 = calculateSha224(data);
    std::string hash2 = calculateSha224(data);
    EXPECT_EQ(hash1, hash2);

    // Test different hash algorithms on same data
    std::string sha224 = calculateSha224(data);
    std::string sha384 = calculateSha384(data);
    std::string sha512 = calculateSha512(data);

    EXPECT_NE(sha224, sha384);
    EXPECT_NE(sha384, sha512);
    EXPECT_NE(sha224, sha512);

    // Verify hash lengths
    EXPECT_EQ(sha224.length(), 56);
    EXPECT_EQ(sha384.length(), 96);
    EXPECT_EQ(sha512.length(), 128);
}

// Test performance with various data sizes
TEST_F(AESTest, PerformanceTest) {
    std::vector<size_t> dataSizes = {1024, 10240, 102400, 1048576}; // 1KB, 10KB, 100KB, 1MB

    for (size_t size : dataSizes) {
        std::string data(size, 'P'); // Fill with 'P' for performance test
        std::vector<unsigned char> iv, tag;

        auto start = std::chrono::high_resolution_clock::now();

        // Encryption
        std::string ciphertext = encryptAES(data, key_, iv, tag);

        // Decryption
        std::string decrypted = decryptAES(ciphertext, key_, iv, tag);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Performance should be reasonable (adjust thresholds as needed)
        EXPECT_LT(duration.count(), 1000 * (size / 1024)); // Roughly 1 second per MB
        EXPECT_EQ(decrypted, data);
    }
}

// Test thread safety
TEST_F(AESTest, ThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 10;
    std::vector<std::future<bool>> futures;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [this, operationsPerThread, t]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    std::string data = "Thread" + std::to_string(t) + "Data" + std::to_string(i);
                    std::vector<unsigned char> iv, tag;

                    // Test encryption/decryption
                    std::string ciphertext = encryptAES(data, key_, iv, tag);
                    std::string decrypted = decryptAES(ciphertext, key_, iv, tag);

                    if (decrypted != data) {
                        return false;
                    }

                    // Test compression/decompression
                    std::string compressed = compress(data);
                    std::string decompressed = decompress(compressed);

                    if (decompressed != data) {
                        return false;
                    }

                    // Test hashing
                    std::string hash = calculateSha256(data);
                    if (hash.empty() || hash.length() != 64) {
                        return false;
                    }

                } catch (...) {
                    return false;
                }
            }
            return true;
        }));
    }

    // Wait for all threads and check results
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }
}

// Test memory management and cleanup
TEST_F(AESTest, MemoryManagement) {
    // Test multiple allocations and deallocations
    for (int i = 0; i < 100; ++i) {
        std::string data(1024 * i + 1, 'M'); // Varying sizes
        std::vector<unsigned char> iv, tag;

        std::string ciphertext = encryptAES(data, key_, iv, tag);
        std::string decrypted = decryptAES(ciphertext, key_, iv, tag);

        EXPECT_EQ(decrypted, data);

        // Force cleanup by clearing vectors
        iv.clear();
        tag.clear();
        ciphertext.clear();
        decrypted.clear();
    }
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_AES_HPP
