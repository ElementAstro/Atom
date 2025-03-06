#ifndef ATOM_UTILS_TEST_AES_HPP
#define ATOM_UTILS_TEST_AES_HPP

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <random>
#include "atom/utils/aes.hpp"

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

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_AES_HPP