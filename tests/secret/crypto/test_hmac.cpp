/*
 * test_hmac.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/types.hpp"
#include "atom/secret/crypto/hmac.hpp"

namespace atom::secret::test {

class HmacTest : public ::testing::Test {
protected:
    void SetUp() override {
        testData_ = "The quick brown fox jumps over the lazy dog";
        testKey_ = "secret_key";
    }

    void TearDown() override {}

    std::string testData_;
    std::string testKey_;
};

TEST_F(HmacTest, HmacSha256String) {
    auto result = Hmac::sha256(testData_, testKey_);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HmacTest, HmacSha256Binary) {
    std::vector<uint8_t> data(testData_.begin(), testData_.end());
    std::vector<uint8_t> key(testKey_.begin(), testKey_.end());

    auto result = Hmac::sha256(data, key);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HmacTest, HmacSha384) {
    auto result = Hmac::sha384(testData_, testKey_);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 48);
}

TEST_F(HmacTest, HmacSha512) {
    auto result = Hmac::sha512(testData_, testKey_);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 64);
}

TEST_F(HmacTest, HmacDeterministic) {
    auto result1 = Hmac::sha256(testData_, testKey_);
    auto result2 = Hmac::sha256(testData_, testKey_);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_EQ(result1.value(), result2.value());
}

TEST_F(HmacTest, HmacDifferentKeys) {
    auto result1 = Hmac::sha256(testData_, "key1");
    auto result2 = Hmac::sha256(testData_, "key2");

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(HmacTest, HmacDifferentData) {
    auto result1 = Hmac::sha256("data1", testKey_);
    auto result2 = Hmac::sha256("data2", testKey_);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(HmacTest, HmacEmptyData) {
    auto result = Hmac::sha256("", testKey_);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HmacTest, HmacEmptyKey) {
    auto result = Hmac::sha256(testData_, "");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HmacTest, GenericHmac) {
    auto result = Hmac::compute(testData_, testKey_, HashAlgorithm::SHA256);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().size(), 32);
}

TEST_F(HmacTest, VerifyHmac) {
    auto hmacResult = Hmac::sha256(testData_, testKey_);
    ASSERT_TRUE(hmacResult.isSuccess());

    bool valid = Hmac::verify(testData_, testKey_, hmacResult.value(),
                              HashAlgorithm::SHA256);
    EXPECT_TRUE(valid);

    // Tampered data should fail
    bool invalid = Hmac::verify("tampered data", testKey_, hmacResult.value(),
                                HashAlgorithm::SHA256);
    EXPECT_FALSE(invalid);
}

}  // namespace atom::secret::test
