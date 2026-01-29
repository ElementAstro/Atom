/*
 * test_encryption.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/crypto/encryption.hpp"

namespace atom::secret::test {

class EncryptionTest : public ::testing::Test {
protected:
    void SetUp() override {
        testPlaintext_ = "This is a test message for encryption testing.";
        testPassword_ = "SecurePassword123!";
    }

    void TearDown() override {}

    std::string testPlaintext_;
    std::string testPassword_;
};

// ============================================================================
// Password-based Encryption Tests
// ============================================================================

TEST_F(EncryptionTest, EncryptDecryptAes256Gcm) {
    EncryptionParams params;
    params.algorithm = EncryptionAlgorithm::AES_256_GCM;
    params.keyIterations = 10000;  // Lower for testing

    auto encryptResult =
        Encryption::encrypt(testPlaintext_, testPassword_, params);
    ASSERT_TRUE(encryptResult.isSuccess()) << encryptResult.errorMessage();

    auto decryptResult =
        Encryption::decrypt(encryptResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess()) << decryptResult.errorMessage();

    EXPECT_EQ(decryptResult.value(), testPlaintext_);
}

TEST_F(EncryptionTest, EncryptDecryptAes128Gcm) {
    EncryptionParams params;
    params.algorithm = EncryptionAlgorithm::AES_128_GCM;
    params.keyIterations = 10000;

    auto encryptResult =
        Encryption::encrypt(testPlaintext_, testPassword_, params);
    ASSERT_TRUE(encryptResult.isSuccess()) << encryptResult.errorMessage();

    auto decryptResult =
        Encryption::decrypt(encryptResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess()) << decryptResult.errorMessage();

    EXPECT_EQ(decryptResult.value(), testPlaintext_);
}

TEST_F(EncryptionTest, EncryptDecryptChaCha20Poly1305) {
    if (!Encryption::isAvailable(EncryptionAlgorithm::ChaCha20_Poly1305)) {
        GTEST_SKIP() << "ChaCha20-Poly1305 not available";
    }

    EncryptionParams params;
    params.algorithm = EncryptionAlgorithm::ChaCha20_Poly1305;
    params.keyIterations = 10000;

    auto encryptResult =
        Encryption::encrypt(testPlaintext_, testPassword_, params);
    ASSERT_TRUE(encryptResult.isSuccess()) << encryptResult.errorMessage();

    auto decryptResult =
        Encryption::decrypt(encryptResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess()) << decryptResult.errorMessage();

    EXPECT_EQ(decryptResult.value(), testPlaintext_);
}

TEST_F(EncryptionTest, EncryptDecryptAesCbc) {
    EncryptionParams params;
    params.algorithm = EncryptionAlgorithm::AES_256_CBC;
    params.keyIterations = 10000;

    auto encryptResult =
        Encryption::encrypt(testPlaintext_, testPassword_, params);
    ASSERT_TRUE(encryptResult.isSuccess()) << encryptResult.errorMessage();

    auto decryptResult =
        Encryption::decrypt(encryptResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess()) << decryptResult.errorMessage();

    EXPECT_EQ(decryptResult.value(), testPlaintext_);
}

TEST_F(EncryptionTest, WrongPasswordFails) {
    auto encryptResult = Encryption::encrypt(testPlaintext_, testPassword_);
    ASSERT_TRUE(encryptResult.isSuccess());

    auto decryptResult =
        Encryption::decrypt(encryptResult.value(), "WrongPassword");
    EXPECT_TRUE(decryptResult.isError());
}

TEST_F(EncryptionTest, EmptyPasswordFails) {
    auto result = Encryption::encrypt(testPlaintext_, "");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidArgument);
}

TEST_F(EncryptionTest, EmptyPlaintextFails) {
    auto result = Encryption::encrypt("", testPassword_);
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.errorCode(), ErrorCode::InvalidPlaintext);
}

// ============================================================================
// Binary Data Tests
// ============================================================================

TEST_F(EncryptionTest, EncryptDecryptBinaryData) {
    std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};

    auto encryptResult = Encryption::encrypt(binaryData, testPassword_);
    ASSERT_TRUE(encryptResult.isSuccess());

    auto decryptResult =
        Encryption::decryptBytes(encryptResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess());

    EXPECT_EQ(decryptResult.value(), binaryData);
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(EncryptionTest, EncryptedDataSerialization) {
    auto encryptResult = Encryption::encrypt(testPlaintext_, testPassword_);
    ASSERT_TRUE(encryptResult.isSuccess());

    auto serialized = encryptResult.value().serialize();
    EXPECT_FALSE(serialized.empty());

    auto deserializeResult = EncryptedData::deserialize(serialized);
    ASSERT_TRUE(deserializeResult.isSuccess());

    auto decryptResult =
        Encryption::decrypt(deserializeResult.value(), testPassword_);
    ASSERT_TRUE(decryptResult.isSuccess());

    EXPECT_EQ(decryptResult.value(), testPlaintext_);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(EncryptionTest, GetKeySize) {
    EXPECT_EQ(Encryption::getKeySize(EncryptionAlgorithm::AES_128_GCM), 16);
    EXPECT_EQ(Encryption::getKeySize(EncryptionAlgorithm::AES_256_GCM), 32);
    EXPECT_EQ(Encryption::getKeySize(EncryptionAlgorithm::AES_128_CBC), 16);
    EXPECT_EQ(Encryption::getKeySize(EncryptionAlgorithm::AES_256_CBC), 32);
    EXPECT_EQ(Encryption::getKeySize(EncryptionAlgorithm::ChaCha20_Poly1305),
              32);
}

TEST_F(EncryptionTest, GetIvSize) {
    EXPECT_EQ(Encryption::getIvSize(EncryptionAlgorithm::AES_128_GCM), 12);
    EXPECT_EQ(Encryption::getIvSize(EncryptionAlgorithm::AES_256_GCM), 12);
    EXPECT_EQ(Encryption::getIvSize(EncryptionAlgorithm::AES_128_CBC), 16);
    EXPECT_EQ(Encryption::getIvSize(EncryptionAlgorithm::AES_256_CBC), 16);
    EXPECT_EQ(Encryption::getIvSize(EncryptionAlgorithm::ChaCha20_Poly1305),
              12);
}

TEST_F(EncryptionTest, IsAead) {
    EXPECT_TRUE(Encryption::isAead(EncryptionAlgorithm::AES_128_GCM));
    EXPECT_TRUE(Encryption::isAead(EncryptionAlgorithm::AES_256_GCM));
    EXPECT_TRUE(Encryption::isAead(EncryptionAlgorithm::ChaCha20_Poly1305));
    EXPECT_FALSE(Encryption::isAead(EncryptionAlgorithm::AES_128_CBC));
    EXPECT_FALSE(Encryption::isAead(EncryptionAlgorithm::AES_256_CBC));
}

TEST_F(EncryptionTest, GetAlgorithmName) {
    EXPECT_EQ(Encryption::getAlgorithmName(EncryptionAlgorithm::AES_256_GCM),
              "AES-256-GCM");
    EXPECT_EQ(
        Encryption::getAlgorithmName(EncryptionAlgorithm::ChaCha20_Poly1305),
        "ChaCha20-Poly1305");
}

TEST_F(EncryptionTest, IsAvailable) {
    // AES should always be available with OpenSSL
    EXPECT_TRUE(Encryption::isAvailable(EncryptionAlgorithm::AES_256_GCM));
    EXPECT_TRUE(Encryption::isAvailable(EncryptionAlgorithm::AES_128_GCM));
}

}  // namespace atom::secret::test
