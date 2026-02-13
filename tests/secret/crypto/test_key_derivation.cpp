/*
 * test_key_derivation.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/key_derivation.hpp"

namespace atom::secret::test {

class KeyDerivationTest : public ::testing::Test {
protected:
    void SetUp() override { testPassword_ = "SecurePassword123!"; }

    void TearDown() override {}

    std::string testPassword_;
};

// ============================================================================
// Salt Generation Tests
// ============================================================================

TEST_F(KeyDerivationTest, GenerateSalt) {
    auto result = KeyDerivation::generateSalt(32);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(KeyDerivationTest, GenerateSaltDifferent) {
    auto result1 = KeyDerivation::generateSalt(32);
    auto result2 = KeyDerivation::generateSalt(32);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    // Two random salts should be different
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyDerivationTest, GenerateRandomBytes) {
    auto result = KeyDerivation::generateRandomBytes(64);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 64);
}

// ============================================================================
// PBKDF2 Tests
// ============================================================================

TEST_F(KeyDerivationTest, Pbkdf2Sha256) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};

    auto result = KeyDerivation::pbkdf2Sha256(testPassword_, salt, 10000, 32);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(KeyDerivationTest, Pbkdf2Sha512) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};

    auto result = KeyDerivation::pbkdf2Sha512(testPassword_, salt, 10000, 64);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 64);
}

TEST_F(KeyDerivationTest, Pbkdf2Deterministic) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};

    auto result1 = KeyDerivation::pbkdf2Sha256(testPassword_, salt, 10000, 32);
    auto result2 = KeyDerivation::pbkdf2Sha256(testPassword_, salt, 10000, 32);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(KeyDerivationTest, Pbkdf2DifferentSalts) {
    std::vector<uint8_t> salt1 = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> salt2 = {0x05, 0x06, 0x07, 0x08};

    auto result1 = KeyDerivation::pbkdf2Sha256(testPassword_, salt1, 10000, 32);
    auto result2 = KeyDerivation::pbkdf2Sha256(testPassword_, salt2, 10000, 32);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyDerivationTest, Pbkdf2DifferentPasswords) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04};

    auto result1 = KeyDerivation::pbkdf2Sha256("password1", salt, 10000, 32);
    auto result2 = KeyDerivation::pbkdf2Sha256("password2", salt, 10000, 32);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(KeyDerivationTest, Pbkdf2DifferentIterations) {
    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04};

    auto result1 = KeyDerivation::pbkdf2Sha256(testPassword_, salt, 1000, 32);
    auto result2 = KeyDerivation::pbkdf2Sha256(testPassword_, salt, 2000, 32);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

// ============================================================================
// Argon2 Tests (if available)
// ============================================================================

TEST_F(KeyDerivationTest, Argon2id) {
    if (!KeyDerivation::isArgon2Available()) {
        GTEST_SKIP() << "Argon2 not available";
    }

    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
                                 0x0D, 0x0E, 0x0F, 0x10};

    Argon2Params params;
    params.memoryCost = 65536;
    params.timeCost = 3;
    params.parallelism = 1;

    auto result = KeyDerivation::argon2id(testPassword_, salt, 32, params);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 32);
}

// ============================================================================
// Scrypt Tests (if available)
// ============================================================================

TEST_F(KeyDerivationTest, Scrypt) {
    if (!KeyDerivation::isScryptAvailable()) {
        GTEST_SKIP() << "Scrypt not available";
    }

    std::vector<uint8_t> salt = {0x01, 0x02, 0x03, 0x04,
                                 0x05, 0x06, 0x07, 0x08};

    ScryptParams params;
    params.N = 16384;
    params.r = 8;
    params.p = 1;

    auto result = KeyDerivation::scrypt(testPassword_, salt, 32, params);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 32);
}

}  // namespace atom::secret::test
