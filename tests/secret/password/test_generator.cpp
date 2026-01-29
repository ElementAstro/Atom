/*
 * test_generator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/password/generator.hpp"

namespace atom::secret::test {

class PasswordGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PasswordGeneratorTest, GenerateDefaultPassword) {
    auto result = PasswordGenerator::generate();
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    EXPECT_GE(result.value().length(), 16);
}

TEST_F(PasswordGeneratorTest, GenerateWithLength) {
    PasswordGeneratorConfig config;
    config.length = 32;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 32);
}

TEST_F(PasswordGeneratorTest, GenerateWithMinLength) {
    PasswordGeneratorConfig config;
    config.length = 4;  // Very short

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 4);
}

TEST_F(PasswordGeneratorTest, GenerateLowercaseOnly) {
    PasswordGeneratorConfig config;
    config.length = 20;
    config.includeLowercase = true;
    config.includeUppercase = false;
    config.includeDigits = false;
    config.includeSpecial = false;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    for (char c : result.value()) {
        EXPECT_TRUE(std::islower(static_cast<unsigned char>(c)));
    }
}

TEST_F(PasswordGeneratorTest, GenerateUppercaseOnly) {
    PasswordGeneratorConfig config;
    config.length = 20;
    config.includeLowercase = false;
    config.includeUppercase = true;
    config.includeDigits = false;
    config.includeSpecial = false;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    for (char c : result.value()) {
        EXPECT_TRUE(std::isupper(static_cast<unsigned char>(c)));
    }
}

TEST_F(PasswordGeneratorTest, GenerateDigitsOnly) {
    PasswordGeneratorConfig config;
    config.length = 20;
    config.includeLowercase = false;
    config.includeUppercase = false;
    config.includeDigits = true;
    config.includeSpecial = false;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    for (char c : result.value()) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
    }
}

TEST_F(PasswordGeneratorTest, GenerateWithSpecialChars) {
    PasswordGeneratorConfig config;
    config.length = 50;
    config.includeLowercase = false;
    config.includeUppercase = false;
    config.includeDigits = false;
    config.includeSpecial = true;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    for (char c : result.value()) {
        EXPECT_TRUE(std::ispunct(static_cast<unsigned char>(c)));
    }
}

TEST_F(PasswordGeneratorTest, GenerateExcludeAmbiguous) {
    PasswordGeneratorConfig config;
    config.length = 100;
    config.excludeAmbiguous = true;

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    std::string ambiguous = "0O1lI";
    for (char c : result.value()) {
        EXPECT_EQ(ambiguous.find(c), std::string::npos);
    }
}

TEST_F(PasswordGeneratorTest, GenerateWithCustomChars) {
    PasswordGeneratorConfig config;
    config.length = 20;
    config.includeLowercase = false;
    config.includeUppercase = false;
    config.includeDigits = false;
    config.includeSpecial = false;
    config.customCharacters = "ABC123";

    auto result = PasswordGenerator::generate(config);
    ASSERT_TRUE(result.isSuccess());

    for (char c : result.value()) {
        EXPECT_NE(config.customCharacters.find(c), std::string::npos);
    }
}

TEST_F(PasswordGeneratorTest, GenerateNoCharactersFails) {
    PasswordGeneratorConfig config;
    config.includeLowercase = false;
    config.includeUppercase = false;
    config.includeDigits = false;
    config.includeSpecial = false;
    config.customCharacters = "";

    auto result = PasswordGenerator::generate(config);
    EXPECT_TRUE(result.isError());
}

TEST_F(PasswordGeneratorTest, GeneratePassphrase) {
    auto result = PasswordGenerator::generatePassphrase(4);
    ASSERT_TRUE(result.isSuccess()) << result.errorMessage();

    // Count separators (should be 3 for 4 words)
    int separatorCount = 0;
    for (char c : result.value()) {
        if (c == '-')
            ++separatorCount;
    }
    EXPECT_EQ(separatorCount, 3);
}

TEST_F(PasswordGeneratorTest, GeneratePassphraseCustomSeparator) {
    auto result = PasswordGenerator::generatePassphrase(3, "_");
    ASSERT_TRUE(result.isSuccess());

    int separatorCount = 0;
    for (char c : result.value()) {
        if (c == '_')
            ++separatorCount;
    }
    EXPECT_EQ(separatorCount, 2);
}

TEST_F(PasswordGeneratorTest, GeneratePin) {
    auto result = PasswordGenerator::generatePin(6);
    ASSERT_TRUE(result.isSuccess());

    EXPECT_EQ(result.value().length(), 6);
    for (char c : result.value()) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(c)));
    }
}

TEST_F(PasswordGeneratorTest, GenerateUnique) {
    std::set<std::string> passwords;

    for (int i = 0; i < 100; ++i) {
        auto result = PasswordGenerator::generate();
        ASSERT_TRUE(result.isSuccess());
        passwords.insert(result.value());
    }

    // All passwords should be unique
    EXPECT_EQ(passwords.size(), 100);
}

}  // namespace atom::secret::test
