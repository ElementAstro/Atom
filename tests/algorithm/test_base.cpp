#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include "atom/algorithm/base.hpp"

using namespace atom::algorithm;
using namespace testing;

std::vector<uint8_t> generateRandomBytes(size_t size) {
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for (size_t i = 0; i < size; i++) {
        data[i] = static_cast<uint8_t>(distrib(gen));
    }
    return data;
}

std::vector<uint8_t> stringToBytes(std::string_view str) {
    return std::vector<uint8_t>(str.begin(), str.end());
}

class Base64Test : public ::testing::Test {
protected:
    void SetUp() override {
        plainText = "Hello, World!";
        emptyText = "";
        longText = std::string(1000, 'A');
        binaryData = {0x00, 0xFF, 0x10, 0x20, 0x30};
        plainTextEncoded = "SGVsbG8sIFdvcmxkIQ==";
        emptyTextEncoded = "";
        binaryDataEncoded = "AP8QIDA=";
    }
    std::string plainText;
    std::string emptyText;
    std::string longText;
    std::vector<uint8_t> binaryData;
    std::string plainTextEncoded;
    std::string emptyTextEncoded;
    std::string binaryDataEncoded;
};

class Base32Test : public ::testing::Test {
protected:
    void SetUp() override {
        plainText = "Hello, World!";
        emptyText = "";
        binaryData = {0x00, 0xFF, 0x10, 0x20, 0x30};
        plainTextEncoded = "JBSWY3DPEBLW64TMMQQQ====";
        emptyTextEncoded = "";
        binaryDataEncoded = "APYQGA4Q====";
    }
    std::string plainText;
    std::string emptyText;
    std::vector<uint8_t> binaryData;
    std::string plainTextEncoded;
    std::string emptyTextEncoded;
    std::string binaryDataEncoded;
};

class XORTest : public ::testing::Test {
protected:
    void SetUp() override {
        plainText = "Hello, World!";
        emptyText = "";
        key = 0x42;
    }
    std::string plainText;
    std::string emptyText;
    uint8_t key;
};

TEST_F(Base64Test, EncodeBasicString) {
    auto result = base64Encode(plainText);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), plainTextEncoded);
}

TEST_F(Base64Test, EncodeEmptyString) {
    auto result = base64Encode(emptyText);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), emptyTextEncoded);
}

TEST_F(Base64Test, EncodeBinaryData) {
    auto result = base64Encode(std::string_view(
        reinterpret_cast<const char*>(binaryData.data()), binaryData.size()));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), binaryDataEncoded);
}

TEST_F(Base64Test, EncodeLongString) {
    auto result = base64Encode(longText);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), ((longText.size() + 2) / 3) * 4);
}

TEST_F(Base64Test, EncodeWithoutPadding) {
    auto result = base64Encode(plainText, false);
    ASSERT_TRUE(result.has_value());
    std::string expected = plainTextEncoded;
    expected.erase(std::remove(expected.begin(), expected.end(), '='),
                   expected.end());
    EXPECT_EQ(result.value(), expected);
}

TEST_F(Base64Test, DecodeBasicString) {
    auto result = base64Decode(plainTextEncoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), plainText);
}

TEST_F(Base64Test, DecodeEmptyString) {
    auto result = base64Decode(emptyTextEncoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), emptyText);
}

TEST_F(Base64Test, DecodeBinaryData) {
    auto result = base64Decode(binaryDataEncoded);
    ASSERT_TRUE(result.has_value());
    std::string decoded = result.value();
    std::vector<uint8_t> decodedBytes(decoded.begin(), decoded.end());
    EXPECT_EQ(decodedBytes, binaryData);
}

TEST_F(Base64Test, DecodeWithoutPadding) {
    std::string noPadding = plainTextEncoded;
    noPadding.erase(std::remove(noPadding.begin(), noPadding.end(), '='),
                    noPadding.end());
    auto result = base64Decode(noPadding);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), plainText);
}

TEST_F(Base64Test, DecodeWithWhitespace) {
    std::string withWhitespace = "SGVs bG8s\nIFdv\r\ncmxk\t\tIQ==";
    auto result = base64Decode(withWhitespace);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), plainText);
}

TEST_F(Base64Test, DecodeInvalidInput) {
    std::string invalidChars = "SGVsbG8sIFdvcmxkIQ=!";
    auto result = base64Decode(invalidChars);
    EXPECT_FALSE(result.has_value());
    std::string invalidLength = "SGVsbG";
    result = base64Decode(invalidLength);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Base64Test, RoundTrip) {
    for (size_t size : {0, 1, 2, 3, 4, 5, 10, 100, 1000}) {
        auto data = generateRandomBytes(size);
        std::string_view dataView(reinterpret_cast<const char*>(data.data()),
                                  data.size());
        auto encoded = base64Encode(dataView);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = base64Decode(encoded.value());
        ASSERT_TRUE(decoded.has_value());
        std::string_view decodedView = decoded.value();
        ASSERT_EQ(decodedView.size(), dataView.size());
        EXPECT_TRUE(
            std::equal(dataView.begin(), dataView.end(), decodedView.begin()));
    }
}

TEST_F(Base64Test, IsBase64Valid) {
    EXPECT_TRUE(isBase64(plainTextEncoded));
    EXPECT_TRUE(isBase64(""));
    EXPECT_FALSE(isBase64("SGVsbG8sIFdvcmxkIQ=!"));
    EXPECT_FALSE(isBase64("SGVsbG"));
}

TEST_F(Base32Test, EncodeBasicString) {
    auto result = encodeBase32(stringToBytes(plainText));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), plainTextEncoded);
}

TEST_F(Base32Test, EncodeEmptyString) {
    auto result = encodeBase32(stringToBytes(emptyText));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), emptyTextEncoded);
}

TEST_F(Base32Test, EncodeBinaryData) {
    auto result = encodeBase32(binaryData);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), binaryDataEncoded);
}

TEST_F(Base32Test, DecodeBasicString) {
    auto result = decodeBase32(plainTextEncoded);
    ASSERT_TRUE(result.has_value());
    std::vector<uint8_t> expected = stringToBytes(plainText);
    EXPECT_EQ(result.value(), expected);
}

TEST_F(Base32Test, DecodeEmptyString) {
    auto result = decodeBase32(emptyTextEncoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(Base32Test, DecodeBinaryData) {
    auto result = decodeBase32(binaryDataEncoded);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), binaryData);
}

TEST_F(Base32Test, DecodeInvalidCharacters) {
    std::string invalidChars = "JBSWY3DPEBLW64TMM!QQ====";
    auto result = decodeBase32(invalidChars);
    EXPECT_FALSE(result.has_value());
}

TEST_F(Base32Test, RoundTrip) {
    for (size_t size : {0, 1, 2, 3, 4, 5, 10, 100}) {
        auto data = generateRandomBytes(size);
        auto encoded = encodeBase32(data);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = decodeBase32(encoded.value());
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded.value().size(), data.size());
        EXPECT_EQ(decoded.value(), data);
    }
}

TEST_F(XORTest, EncryptBasicString) {
    auto encrypted = xorEncrypt(plainText, key);
    EXPECT_NE(encrypted, plainText);
    EXPECT_EQ(encrypted.size(), plainText.size());
    for (size_t i = 0; i < plainText.size(); i++) {
        EXPECT_EQ(static_cast<unsigned char>(encrypted[i]),
                  static_cast<unsigned char>(plainText[i]) ^ key);
    }
}

TEST_F(XORTest, EncryptEmptyString) {
    auto encrypted = xorEncrypt(emptyText, key);
    EXPECT_TRUE(encrypted.empty());
}

TEST_F(XORTest, DecryptBasicString) {
    auto encrypted = xorEncrypt(plainText, key);
    auto decrypted = xorDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, plainText);
}

TEST_F(XORTest, DecryptEmptyString) {
    auto decrypted = xorDecrypt(emptyText, key);
    EXPECT_TRUE(decrypted.empty());
}

TEST_F(XORTest, RoundTrip) {
    std::vector<size_t> sizes = {0, 1, 2, 10, 100, 1000};
    std::vector<uint8_t> keys = {0, 1, 42, 127, 128, 255};
    for (size_t size : sizes) {
        for (uint8_t testKey : keys) {
            auto randomData = generateRandomBytes(size);
            std::string data(reinterpret_cast<char*>(randomData.data()),
                             randomData.size());
            auto encrypted = xorEncrypt(data, testKey);
            auto decrypted = xorDecrypt(encrypted, testKey);
            EXPECT_EQ(decrypted, data);
        }
    }
}

TEST_F(XORTest, DoubleEncryptionCancelsOut) {
    auto encrypted = xorEncrypt(plainText, key);
    auto doubleEncrypted = xorEncrypt(encrypted, key);
    EXPECT_EQ(doubleEncrypted, plainText);
}

TEST_F(XORTest, DifferentKeyGivesDifferentResults) {
    uint8_t key1 = 0x42;
    uint8_t key2 = 0x43;
    auto encrypted1 = xorEncrypt(plainText, key1);
    auto encrypted2 = xorEncrypt(plainText, key2);
    EXPECT_NE(encrypted1, encrypted2);
}

/*
TODO: Fix compile-time tests
TEST(StaticBase64Test, CompileTimeEncode) {
    constexpr auto encoded = encode<"Test">();
    std::string_view encodedView(encoded.c_str());
    EXPECT_EQ(encodedView, "VGVzdA==");
}

TEST(StaticBase64Test, CompileTimeDecode) {
    constexpr auto decoded = decodeBase64<"VGVzdA==">();
    std::string_view decodedView(decoded.c_str());
    EXPECT_EQ(decodedView, "Test");
}
*/

TEST(PerformanceTest, Base64EncodePerformance) {
    auto largeData = generateRandomBytes(1000000);
    std::string_view dataView(reinterpret_cast<const char*>(largeData.data()),
                              largeData.size());
    auto start = std::chrono::high_resolution_clock::now();
    auto result = base64Encode(dataView);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Base64 encode of 1MB took: {}ms", duration);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), ((largeData.size() + 2) / 3) * 4);
}

TEST(EdgeCasesTest, Base64WithSpecialPatterns) {
    std::vector<std::string> patterns = {
        std::string(1, '\0'), std::string(2, '\0'), std::string(3, '\0'),
        std::string(4, '\0'), "\x00\xFF\x00\xFF",   "\xFF\x00\xFF\x00",
        "\x77\x88\x99"};
    for (const auto& pattern : patterns) {
        auto encoded = base64Encode(pattern);
        ASSERT_TRUE(encoded.has_value());
        auto decoded = base64Decode(encoded.value());
        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded.value().size(), pattern.size());
        EXPECT_TRUE(std::equal(pattern.begin(), pattern.end(),
                               decoded.value().begin()));
    }
}

TEST(ErrorHandlingTest, Base64InvalidInputs) {
    std::vector<std::string> invalidInputs = {
        "A", "A===", "A=A=", "====", "A=B=", "AB=CD"};
    for (const auto& input : invalidInputs) {
        auto result = base64Decode(input);
        EXPECT_FALSE(result.has_value())
            << "Input '" << input << "' should fail";
    }
}
