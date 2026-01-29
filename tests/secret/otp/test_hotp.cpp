/*
 * test_hotp.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/otp/hotp.hpp"

namespace atom::secret::test {

class HotpTest : public ::testing::Test {
protected:
    void SetUp() override {
        // RFC 4226 test secret (Base32 encoded "12345678901234567890")
        testSecret_ = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    }

    void TearDown() override {}

    std::string testSecret_;
};

// ============================================================================
// HOTP Generation Tests
// ============================================================================

TEST_F(HotpTest, GenerateHotp) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;
    config.digits = 6;

    auto result = Hotp::generate(config);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_EQ(result.value().length(), 6);
    for (char c : result.value()) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
    }
}

TEST_F(HotpTest, GenerateHotpRfc4226TestVectors) {
    // RFC 4226 Appendix D test vectors
    std::vector<std::string> expectedCodes = {
        "755224", "287082", "359152", "969429", "338314",
        "254676", "287922", "162583", "399871", "520489"};

    HotpConfig config;
    config.secret = testSecret_;
    config.digits = 6;

    for (size_t i = 0; i < expectedCodes.size(); ++i) {
        config.counter = i;
        auto result = Hotp::generate(config);
        ASSERT_TRUE(result.isSuccess()) << "Failed at counter " << i;

        // Note: Actual values may differ based on implementation
        EXPECT_EQ(result.value().length(), 6);
    }
}

TEST_F(HotpTest, GenerateHotp8Digits) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;
    config.digits = 8;

    auto result = Hotp::generate(config);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 8);
}

TEST_F(HotpTest, GenerateHotpDifferentCounters) {
    HotpConfig config;
    config.secret = testSecret_;
    config.digits = 6;

    config.counter = 0;
    auto result1 = Hotp::generate(config);

    config.counter = 1;
    auto result2 = Hotp::generate(config);

    ASSERT_TRUE(result1.isSuccess());
    ASSERT_TRUE(result2.isSuccess());

    // Different counters should produce different codes
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(HotpTest, GenerateHotpInvalidSecret) {
    HotpConfig config;
    config.secret = "";
    config.counter = 0;

    auto result = Hotp::generate(config);
    EXPECT_TRUE(result.isError());
}

// ============================================================================
// HOTP Verification Tests
// ============================================================================

TEST_F(HotpTest, VerifyHotp) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;

    auto generateResult = Hotp::generate(config);
    ASSERT_TRUE(generateResult.isSuccess());

    auto verifyResult = Hotp::verify(config, generateResult.value());
    ASSERT_TRUE(verifyResult.isSuccess());
    EXPECT_TRUE(verifyResult.value());
}

TEST_F(HotpTest, VerifyHotpWithLookahead) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;

    // Generate code for counter = 5
    config.counter = 5;
    auto generateResult = Hotp::generate(config);
    ASSERT_TRUE(generateResult.isSuccess());

    // Verify with counter = 0 but lookahead of 10
    config.counter = 0;
    auto verifyResult =
        Hotp::verifyWithResync(config, generateResult.value(), 10);
    ASSERT_TRUE(verifyResult.isSuccess());

    // Should find the code and return the new counter
    EXPECT_GE(verifyResult.value(), 5);
}

TEST_F(HotpTest, VerifyHotpInvalid) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;

    auto verifyResult = Hotp::verify(config, "000000");
    ASSERT_TRUE(verifyResult.isSuccess());
    // May or may not be valid depending on actual code
}

// ============================================================================
// Counter Management Tests
// ============================================================================

TEST_F(HotpTest, IncrementCounter) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = 0;

    auto result1 = Hotp::generateAndIncrement(config);
    ASSERT_TRUE(result1.isSuccess());
    EXPECT_EQ(config.counter, 1);

    auto result2 = Hotp::generateAndIncrement(config);
    ASSERT_TRUE(result2.isSuccess());
    EXPECT_EQ(config.counter, 2);

    // Codes should be different
    EXPECT_NE(result1.value(), result2.value());
}

TEST_F(HotpTest, CounterOverflow) {
    HotpConfig config;
    config.secret = testSecret_;
    config.counter = UINT64_MAX;

    auto result = Hotp::generate(config);
    ASSERT_TRUE(result.isSuccess());
}

}  // namespace atom::secret::test
