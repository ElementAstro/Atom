/*
 * test_validator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/password/validator.hpp"

namespace atom::secret::test {

class PasswordValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PasswordValidatorTest, ValidateStrongPassword) {
    auto result = PasswordValidator::validate("SecureP@ssw0rd123!");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_TRUE(result.value().isValid);
    EXPECT_GE(result.value().score, 60);
}

TEST_F(PasswordValidatorTest, ValidateWeakPassword) {
    auto result = PasswordValidator::validate("password");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
    EXPECT_LT(result.value().score, 40);
}

TEST_F(PasswordValidatorTest, ValidateTooShort) {
    PasswordPolicy policy;
    policy.minLength = 8;

    auto result = PasswordValidator::validate("short", policy);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
    EXPECT_FALSE(result.value().issues.empty());
}

TEST_F(PasswordValidatorTest, ValidateMissingUppercase) {
    PasswordPolicy policy;
    policy.requireUppercase = true;

    auto result = PasswordValidator::validate("lowercase123!", policy);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
}

TEST_F(PasswordValidatorTest, ValidateMissingLowercase) {
    PasswordPolicy policy;
    policy.requireLowercase = true;

    auto result = PasswordValidator::validate("UPPERCASE123!", policy);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
}

TEST_F(PasswordValidatorTest, ValidateMissingDigit) {
    PasswordPolicy policy;
    policy.requireDigit = true;

    auto result = PasswordValidator::validate("NoDigitsHere!", policy);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
}

TEST_F(PasswordValidatorTest, ValidateMissingSpecial) {
    PasswordPolicy policy;
    policy.requireSpecial = true;

    auto result = PasswordValidator::validate("NoSpecialChars123", policy);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
}

TEST_F(PasswordValidatorTest, CalculateStrength) {
    EXPECT_EQ(PasswordValidator::calculateStrength(10),
              PasswordStrength::VeryWeak);
    EXPECT_EQ(PasswordValidator::calculateStrength(30), PasswordStrength::Weak);
    EXPECT_EQ(PasswordValidator::calculateStrength(50),
              PasswordStrength::Medium);
    EXPECT_EQ(PasswordValidator::calculateStrength(70),
              PasswordStrength::Strong);
    EXPECT_EQ(PasswordValidator::calculateStrength(90),
              PasswordStrength::VeryStrong);
}

TEST_F(PasswordValidatorTest, CalculateEntropy) {
    // Longer passwords should have higher entropy
    double entropy1 = PasswordValidator::calculateEntropy("short");
    double entropy2 = PasswordValidator::calculateEntropy("muchlongerpassword");

    EXPECT_GT(entropy2, entropy1);
}

TEST_F(PasswordValidatorTest, CalculateEntropyMixedCharset) {
    // Mixed charset should have higher entropy
    double entropy1 = PasswordValidator::calculateEntropy("aaaaaaaaaa");
    double entropy2 = PasswordValidator::calculateEntropy("aA1!bB2@cC");

    EXPECT_GT(entropy2, entropy1);
}

TEST_F(PasswordValidatorTest, HasRepeatingCharacters) {
    EXPECT_TRUE(PasswordValidator::hasRepeatingCharacters("aaa", 3));
    EXPECT_TRUE(PasswordValidator::hasRepeatingCharacters("password111", 3));
    EXPECT_FALSE(PasswordValidator::hasRepeatingCharacters("abcdef", 3));
}

TEST_F(PasswordValidatorTest, HasSequentialCharacters) {
    EXPECT_TRUE(PasswordValidator::hasSequentialCharacters("abc", 3));
    EXPECT_TRUE(PasswordValidator::hasSequentialCharacters("123", 3));
    EXPECT_TRUE(PasswordValidator::hasSequentialCharacters("xyz", 3));
    EXPECT_FALSE(PasswordValidator::hasSequentialCharacters("adf", 3));
}

TEST_F(PasswordValidatorTest, IsCommonPassword) {
    EXPECT_TRUE(PasswordValidator::isCommonPassword("password"));
    EXPECT_TRUE(PasswordValidator::isCommonPassword("123456"));
    EXPECT_TRUE(PasswordValidator::isCommonPassword("qwerty"));
    EXPECT_FALSE(PasswordValidator::isCommonPassword("xK9#mP2$vL7@nQ4"));
}

TEST_F(PasswordValidatorTest, ValidateWithHistory) {
    std::vector<std::string> history = {"OldPassword1!", "OldPassword2!"};

    PasswordPolicy policy;
    policy.checkHistory = true;

    auto result = PasswordValidator::validate("OldPassword1!", policy, history);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
}

TEST_F(PasswordValidatorTest, GetSuggestions) {
    auto result = PasswordValidator::validate("weak");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().suggestions.empty());
}

TEST_F(PasswordValidatorTest, EmptyPassword) {
    auto result = PasswordValidator::validate("");
    ASSERT_TRUE(result.isSuccess());

    EXPECT_FALSE(result.value().isValid);
    EXPECT_EQ(result.value().score, 0);
}

}  // namespace atom::secret::test
