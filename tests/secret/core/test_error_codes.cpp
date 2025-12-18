/*
 * test_error_codes.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/error_codes.hpp"

namespace atom::secret::test {

class ErrorCodesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorCodesTest, ErrorCodeToString) {
    EXPECT_EQ(errorCodeToString(ErrorCode::Success), "Success");
    EXPECT_EQ(errorCodeToString(ErrorCode::Unknown), "Unknown error");
    EXPECT_EQ(errorCodeToString(ErrorCode::InvalidArgument),
              "Invalid argument");
    EXPECT_EQ(errorCodeToString(ErrorCode::EncryptionFailed),
              "Encryption failed");
    EXPECT_EQ(errorCodeToString(ErrorCode::DecryptionFailed),
              "Decryption failed");
    EXPECT_EQ(errorCodeToString(ErrorCode::PasswordTooShort),
              "Password too short");
    EXPECT_EQ(errorCodeToString(ErrorCode::StorageKeyNotFound),
              "Storage key not found");
    EXPECT_EQ(errorCodeToString(ErrorCode::SerializationFailed),
              "Serialization failed");
    EXPECT_EQ(errorCodeToString(ErrorCode::OtpGenerationFailed),
              "OTP generation failed");
    EXPECT_EQ(errorCodeToString(ErrorCode::HashFailed),
              "Hash operation failed");
}

TEST_F(ErrorCodesTest, IsSuccess) {
    EXPECT_TRUE(isSuccess(ErrorCode::Success));
    EXPECT_FALSE(isSuccess(ErrorCode::Unknown));
    EXPECT_FALSE(isSuccess(ErrorCode::InvalidArgument));
    EXPECT_FALSE(isSuccess(ErrorCode::EncryptionFailed));
}

TEST_F(ErrorCodesTest, IsError) {
    EXPECT_FALSE(isError(ErrorCode::Success));
    EXPECT_TRUE(isError(ErrorCode::Unknown));
    EXPECT_TRUE(isError(ErrorCode::InvalidArgument));
    EXPECT_TRUE(isError(ErrorCode::EncryptionFailed));
}

TEST_F(ErrorCodesTest, ErrorCodeValues) {
    // Verify error code ranges
    EXPECT_EQ(static_cast<uint32_t>(ErrorCode::Success), 0);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::EncryptionFailed), 100);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::EncryptionFailed), 200);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::PasswordTooShort), 200);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::PasswordTooShort), 300);
    EXPECT_GE(static_cast<uint32_t>(ErrorCode::StorageNotInitialized), 300);
    EXPECT_LT(static_cast<uint32_t>(ErrorCode::StorageNotInitialized), 400);
}

}  // namespace atom::secret::test
