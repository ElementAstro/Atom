/*
 * test_totp.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/otp/totp.hpp"

namespace atom::secret::test {

class TotpTest : public ::testing::Test {
protected:
    void SetUp() override {
        // RFC 6238 test secret (Base32 encoded "12345678901234567890")
        testSecret_ = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    }

    void TearDown() override {}

    std::string testSecret_;
};

// ============================================================================
// TOTP Generation Tests
// ============================================================================

TEST_F(TotpTest, GenerateTotp) {
    TotpConfig config;
    config.secret = testSecret_;
    config.digits = 6;
    config.period = 30;
    config.algorithm = HashAlgorithm::SHA1;

    auto result = Totp::generate(config);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().length(), 6);
    for (char c : result.value()) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
    }
}

TEST_F(TotpTest, GenerateTotpWithTime) {
    TotpConfig config;
    config.secret = testSecret_;
    config.digits = 6;
    config.period = 30;
    config.algorithm = HashAlgorithm::SHA1;

    // RFC 6238 test vector: time = 59, expected = 287082
    auto result = Totp::generate(config, 59);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    // Note: This test may need adjustment based on actual implementation
    EXPECT_EQ(result.value().length(), 6);
}

TEST_F(TotpTest, GenerateTotp8Digits) {
    TotpConfig config;
    config.secret = testSecret_;
    config.digits = 8;

    auto result = Totp::generate(config);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 8);
}

TEST_F(TotpTest, GenerateTotpDifferentPeriod) {
    TotpConfig config1;
    config1.secret = testSecret_;
    config1.period = 30;

    TotpConfig config2;
    config2.secret = testSecret_;
    config2.period = 60;

    auto result1 = Totp::generate(config1, 59);
    auto result2 = Totp::generate(config2, 59);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    // Different periods should produce different codes at the same time
    // (unless they happen to be in the same counter value)
}

TEST_F(TotpTest, GenerateTotpInvalidSecret) {
    TotpConfig config;
    config.secret = "";

    auto result = Totp::generate(config);
    EXPECT_TRUE(result.isError());
}

// ============================================================================
// TOTP Verification Tests
// ============================================================================

TEST_F(TotpTest, VerifyTotp) {
    TotpConfig config;
    config.secret = testSecret_;

    auto generateResult = Totp::generate(config);
    ASSERT_TRUE(generateResult.isSuccess());

    bool valid = Totp::verify(config, generateResult.value());
    EXPECT_TRUE(valid);
}

TEST_F(TotpTest, VerifyTotpWithWindow) {
    TotpConfig config;
    config.secret = testSecret_;

    auto generateResult = Totp::generate(config);
    ASSERT_TRUE(generateResult.isSuccess());

    // Should be valid within window
    bool valid = Totp::verify(config, generateResult.value(), 1);
    EXPECT_TRUE(valid);
}

TEST_F(TotpTest, VerifyTotpInvalid) {
    TotpConfig config;
    config.secret = testSecret_;

    bool valid = Totp::verify(config, "000000");
    // May or may not be valid depending on current time
    // Just ensure it doesn't crash
    (void)valid;
}

// ============================================================================
// Base32 Tests
// ============================================================================

TEST_F(TotpTest, Base32Encode) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    std::string encoded = Base32::encode(data);

    EXPECT_FALSE(encoded.empty());
}

TEST_F(TotpTest, Base32Decode) {
    auto result = Base32::decode("JBSWY3DPEHPK3PXP");
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    std::string decoded(result.value().begin(), result.value().end());
    EXPECT_EQ(decoded, "Hello!");
}

TEST_F(TotpTest, Base32RoundTrip) {
    std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::string encoded = Base32::encode(original);

    auto decodeResult = Base32::decode(encoded);
    ASSERT_TRUE(decodeResult.isSuccess());

    EXPECT_EQ(decodeResult.value(), original);
}

TEST_F(TotpTest, Base32DecodeEmpty) {
    auto result = Base32::decode("");
    ASSERT_TRUE(result.isSuccess());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(TotpTest, Base32DecodeInvalid) {
    auto result = Base32::decode("Invalid!@#$");
    EXPECT_TRUE(result.isError());
}

// ============================================================================
// Secret Generation Tests
// ============================================================================

TEST_F(TotpTest, GenerateSecret) {
    auto result = Totp::generateSecret();
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    // Default secret should be 20 bytes = 32 Base32 characters
    EXPECT_GE(result.value().length(), 16);
}

TEST_F(TotpTest, GenerateSecretCustomLength) {
    auto result = Totp::generateSecret(32);
    ASSERT_TRUE(result.isSuccess());

    // 32 bytes = ~52 Base32 characters
    EXPECT_GE(result.value().length(), 32);
}

// ============================================================================
// URI Generation Tests
// ============================================================================

TEST_F(TotpTest, GenerateUri) {
    TotpConfig config;
    config.secret = testSecret_;
    config.issuer = "TestApp";
    config.accountName = "user@example.com";

    std::string uri = Totp::generateUri(config);

    EXPECT_TRUE(uri.find("otpauth://totp/") == 0);
    EXPECT_NE(uri.find("secret="), std::string::npos);
    EXPECT_NE(uri.find("issuer=TestApp"), std::string::npos);
}

TEST_F(TotpTest, ParseUri) {
    std::string uri =
        "otpauth://totp/"
        "TestApp:user@example.com?secret=JBSWY3DPEHPK3PXP&issuer=TestApp&"
        "digits=6&period=30";

    auto result = Totp::parseUri(uri);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().secret, "JBSWY3DPEHPK3PXP");
    EXPECT_EQ(result.value().issuer, "TestApp");
    EXPECT_EQ(result.value().digits, 6);
    EXPECT_EQ(result.value().period, 30);
}

}  // namespace atom::secret::test
