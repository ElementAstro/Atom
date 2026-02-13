/*
 * test_hash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/hash.hpp"

namespace atom::secret::test {

class HashTest : public ::testing::Test {
protected:
    void SetUp() override {
        testData_ = "The quick brown fox jumps over the lazy dog";
    }

    void TearDown() override {}

    std::string testData_;
};

TEST_F(HashTest, Sha256String) {
    auto result = Hash::sha256(testData_);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    // Known SHA-256 hash for this string
    std::string expectedHex =
        "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592";
    std::string actualHex = bytesToHex(result.value());

    EXPECT_EQ(actualHex, expectedHex);
}

TEST_F(HashTest, Sha256Binary) {
    std::vector<uint8_t> data(testData_.begin(), testData_.end());
    auto result = Hash::sha256(data);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HashTest, Sha384String) {
    auto result = Hash::sha384(testData_);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 48);
}

TEST_F(HashTest, Sha512String) {
    auto result = Hash::sha512(testData_);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 64);
}

TEST_F(HashTest, Sha3_256String) {
    auto result = Hash::sha3_256(testData_);
    if (result.isError()) {
        GTEST_SKIP() << "SHA3-256 not available";
    }

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HashTest, Sha3_512String) {
    auto result = Hash::sha3_512(testData_);
    if (result.isError()) {
        GTEST_SKIP() << "SHA3-512 not available";
    }

    EXPECT_EQ(result.value().size(), 64);
}

TEST_F(HashTest, Blake2b256) {
    auto result = Hash::blake2b256(testData_);
    if (result.isError()) {
        GTEST_SKIP() << "BLAKE2b-256 not available";
    }

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HashTest, Blake2b512) {
    auto result = Hash::blake2b512(testData_);
    if (result.isError()) {
        GTEST_SKIP() << "BLAKE2b-512 not available";
    }

    EXPECT_EQ(result.value().size(), 64);
}

TEST_F(HashTest, HashEmpty) {
    auto result = Hash::sha256("");
    ASSERT_TRUE(result.isSuccess());

    // SHA-256 of empty string
    std::string expectedHex =
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    std::string actualHex = bytesToHex(result.value());

    EXPECT_EQ(actualHex, expectedHex);
}

TEST_F(HashTest, HashDeterministic) {
    auto result1 = Hash::sha256(testData_);
    auto result2 = Hash::sha256(testData_);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(HashTest, HashDifferentInputs) {
    auto result1 = Hash::sha256("input1");
    auto result2 = Hash::sha256("input2");

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(HashTest, GenericHash) {
    auto result = Hash::hash(testData_, HashAlgorithm::SHA256);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HashTest, GetDigestSize) {
    EXPECT_EQ(Hash::getDigestSize(HashAlgorithm::SHA256), 32);
    EXPECT_EQ(Hash::getDigestSize(HashAlgorithm::SHA384), 48);
    EXPECT_EQ(Hash::getDigestSize(HashAlgorithm::SHA512), 64);
    EXPECT_EQ(Hash::getDigestSize(HashAlgorithm::MD5), 16);
}

}  // namespace atom::secret::test
