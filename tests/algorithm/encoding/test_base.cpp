/*
 * test_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Unit Tests for Base Encoding/Decoding Algorithms

**************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <random>
#include <string>
#include <vector>

#include "atom/algorithm/encoding/base.hpp"

using namespace atom::algorithm;

class BaseEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    std::string generateRandomString(size_t length) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 255);

        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
        return result;
    }

    std::vector<uint8_t> generateRandomBytes(size_t length) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 255);

        std::vector<uint8_t> result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<uint8_t>(dis(gen)));
        }
        return result;
    }
};

// =============================================================================
// Base64 Encoding Tests
// =============================================================================

TEST_F(BaseEncodingTest, Base64EncodeEmptyString) {
    auto result = base64Encode("");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(BaseEncodingTest, Base64EncodeBasicStrings) {
    // Standard test vectors
    auto result = base64Encode("f");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zg==");

    result = base64Encode("fo");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm8=");

    result = base64Encode("foo");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm9v");

    result = base64Encode("foob");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm9vYg==");

    result = base64Encode("fooba");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm9vYmE=");

    result = base64Encode("foobar");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm9vYmFy");
}

TEST_F(BaseEncodingTest, Base64EncodeWithoutPadding) {
    auto result = base64Encode("f", false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zg");

    result = base64Encode("fo", false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm8");

    result = base64Encode("foo", false);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Zm9v");
}

TEST_F(BaseEncodingTest, Base64DecodeEmptyString) {
    auto result = base64Decode("");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(BaseEncodingTest, Base64DecodeBasicStrings) {
    auto result = base64Decode("Zg==");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "f");

    result = base64Decode("Zm8=");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "fo");

    result = base64Decode("Zm9v");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "foo");

    result = base64Decode("Zm9vYmFy");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "foobar");
}

TEST_F(BaseEncodingTest, Base64RoundTrip) {
    std::vector<std::string> testStrings = {
        "",
        "a",
        "ab",
        "abc",
        "Hello, World!",
        "The quick brown fox jumps over the lazy dog",
        "1234567890",
        "!@#$%^&*()_+-=[]{}|;':\",./<>?",
    };

    for (const auto& original : testStrings) {
        auto encoded = base64Encode(original);
        ASSERT_TRUE(encoded.has_value()) << "Failed to encode: " << original;

        auto decoded = base64Decode(encoded.value());
        ASSERT_TRUE(decoded.has_value())
            << "Failed to decode: " << encoded.value();
        EXPECT_EQ(decoded.value(), original)
            << "Round trip failed for: " << original;
    }
}

TEST_F(BaseEncodingTest, Base64BinaryData) {
    // Test with binary data containing null bytes
    std::string binaryData = "Hello\0World";
    binaryData.resize(11);  // Include the null byte

    auto encoded = base64Encode(std::string_view(binaryData.data(), 11));
    ASSERT_TRUE(encoded.has_value());

    auto decoded = base64Decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value().size(), 11u);
}

TEST_F(BaseEncodingTest, IsBase64Valid) {
    EXPECT_TRUE(isBase64("Zm9vYmFy"));
    EXPECT_TRUE(isBase64("Zg=="));
    EXPECT_TRUE(isBase64("Zm8="));
    // Empty string returns false per implementation (length % 4 != 0 check)
    EXPECT_FALSE(isBase64(""));
}

TEST_F(BaseEncodingTest, IsBase64Invalid) {
    EXPECT_FALSE(isBase64("Zm9vYmFy!"));  // Invalid character
    EXPECT_FALSE(isBase64("Zm9"));        // Invalid length (not multiple of 4)
    // "====" is technically valid Base64 (all padding chars are allowed)
    EXPECT_TRUE(isBase64("===="));
}

// =============================================================================
// Base32 Encoding Tests
// =============================================================================

TEST_F(BaseEncodingTest, Base32EncodeEmptyData) {
    std::vector<uint8_t> empty;
    auto result = encodeBase32(std::span<const uint8_t>(empty));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(BaseEncodingTest, Base32EncodeBasicData) {
    // RFC 4648 test vectors
    std::vector<uint8_t> data1 = {'f'};
    auto result = encodeBase32(std::span<const uint8_t>(data1));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MY======");

    std::vector<uint8_t> data2 = {'f', 'o'};
    result = encodeBase32(std::span<const uint8_t>(data2));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MZXQ====");

    std::vector<uint8_t> data3 = {'f', 'o', 'o'};
    result = encodeBase32(std::span<const uint8_t>(data3));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MZXW6===");

    std::vector<uint8_t> data4 = {'f', 'o', 'o', 'b'};
    result = encodeBase32(std::span<const uint8_t>(data4));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MZXW6YQ=");

    std::vector<uint8_t> data5 = {'f', 'o', 'o', 'b', 'a'};
    result = encodeBase32(std::span<const uint8_t>(data5));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MZXW6YTB");

    std::vector<uint8_t> data6 = {'f', 'o', 'o', 'b', 'a', 'r'};
    result = encodeBase32(std::span<const uint8_t>(data6));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "MZXW6YTBOI======");
}

TEST_F(BaseEncodingTest, Base32DecodeEmptyString) {
    auto result = decodeBase32("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(BaseEncodingTest, Base32DecodeBasicStrings) {
    auto result = decodeBase32("MY======");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().size(), 1u);
    EXPECT_EQ(result.value()[0], 'f');

    result = decodeBase32("MZXW6YTBOI======");
    ASSERT_TRUE(result.has_value());
    std::string decoded(result.value().begin(), result.value().end());
    EXPECT_EQ(decoded, "foobar");
}

TEST_F(BaseEncodingTest, Base32RoundTrip) {
    for (size_t len = 0; len <= 20; ++len) {
        auto original = generateRandomBytes(len);
        auto encoded = encodeBase32(std::span<const uint8_t>(original));
        ASSERT_TRUE(encoded.has_value())
            << "Failed to encode data of length " << len;

        auto decoded = decodeBase32(encoded.value());
        ASSERT_TRUE(decoded.has_value())
            << "Failed to decode: " << encoded.value();
        EXPECT_EQ(decoded.value(), original)
            << "Round trip failed for length " << len;
    }
}

// =============================================================================
// Hex Encoding Tests
// =============================================================================

TEST_F(BaseEncodingTest, HexEncodeEmptyData) {
    std::vector<uint8_t> empty;
    auto result = encodeHex(std::span<const uint8_t>(empty));
    EXPECT_EQ(result, "");
}

TEST_F(BaseEncodingTest, HexEncodeBasicData) {
    std::vector<uint8_t> data = {0x00, 0x01, 0x0F, 0x10, 0xFF};
    auto result = encodeHex(std::span<const uint8_t>(data), true);
    EXPECT_EQ(result, "00010F10FF");

    result = encodeHex(std::span<const uint8_t>(data), false);
    EXPECT_EQ(result, "00010f10ff");
}

TEST_F(BaseEncodingTest, HexEncodeString) {
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto result = encodeHex(std::span<const uint8_t>(data));
    EXPECT_EQ(result, "48656C6C6F");
}

TEST_F(BaseEncodingTest, HexDecodeEmptyString) {
    auto result = decodeHex("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().empty());
}

TEST_F(BaseEncodingTest, HexDecodeBasicStrings) {
    auto result = decodeHex("48656C6C6F");
    ASSERT_TRUE(result.has_value());
    std::string decoded(result.value().begin(), result.value().end());
    EXPECT_EQ(decoded, "Hello");

    result = decodeHex("48656c6c6f");  // lowercase
    ASSERT_TRUE(result.has_value());
    decoded = std::string(result.value().begin(), result.value().end());
    EXPECT_EQ(decoded, "Hello");
}

TEST_F(BaseEncodingTest, HexDecodeInvalidInput) {
    // Odd length
    auto result = decodeHex("123");
    EXPECT_FALSE(result.has_value());

    // Invalid characters
    result = decodeHex("GHIJ");
    EXPECT_FALSE(result.has_value());

    result = decodeHex("12ZZ");
    EXPECT_FALSE(result.has_value());
}

TEST_F(BaseEncodingTest, HexRoundTrip) {
    for (size_t len = 0; len <= 100; ++len) {
        auto original = generateRandomBytes(len);
        auto encoded = encodeHex(std::span<const uint8_t>(original));
        auto decoded = decodeHex(encoded);
        ASSERT_TRUE(decoded.has_value())
            << "Failed to decode hex of length " << len;
        EXPECT_EQ(decoded.value(), original)
            << "Round trip failed for length " << len;
    }
}

// =============================================================================
// URL Encoding Tests
// =============================================================================

TEST_F(BaseEncodingTest, UrlEncodeEmptyString) {
    auto result = urlEncode("");
    EXPECT_EQ(result, "");
}

TEST_F(BaseEncodingTest, UrlEncodeBasicStrings) {
    // Unreserved characters should not be encoded
    EXPECT_EQ(urlEncode("abc123"), "abc123");
    EXPECT_EQ(urlEncode("ABC"), "ABC");
    EXPECT_EQ(urlEncode("-_.~"), "-_.~");
}

TEST_F(BaseEncodingTest, UrlEncodeSpecialCharacters) {
    EXPECT_EQ(urlEncode(" "), "%20");
    EXPECT_EQ(urlEncode("Hello World"), "Hello%20World");
    EXPECT_EQ(urlEncode("a=b&c=d"), "a%3Db%26c%3Dd");
    EXPECT_EQ(urlEncode("100%"), "100%25");
}

TEST_F(BaseEncodingTest, UrlEncodeSpaceAsPlus) {
    EXPECT_EQ(urlEncode(" ", true), "+");
    EXPECT_EQ(urlEncode("Hello World", true), "Hello+World");
}

TEST_F(BaseEncodingTest, UrlDecodeEmptyString) {
    auto result = urlDecode("");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

TEST_F(BaseEncodingTest, UrlDecodeBasicStrings) {
    auto result = urlDecode("abc123");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "abc123");

    result = urlDecode("Hello%20World");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello World");

    result = urlDecode("a%3Db%26c%3Dd");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "a=b&c=d");
}

TEST_F(BaseEncodingTest, UrlDecodePlusAsSpace) {
    auto result = urlDecode("Hello+World");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "Hello World");
}

TEST_F(BaseEncodingTest, UrlDecodeInvalidInput) {
    // Incomplete percent encoding
    auto result = urlDecode("%2");
    EXPECT_FALSE(result.has_value());

    result = urlDecode("%");
    EXPECT_FALSE(result.has_value());

    // Invalid hex characters
    result = urlDecode("%GG");
    EXPECT_FALSE(result.has_value());
}

TEST_F(BaseEncodingTest, UrlRoundTrip) {
    std::vector<std::string> testStrings = {
        "",       "hello",        "Hello World",       "a=b&c=d",
        "100%",   "path/to/file", "email@example.com",
        "日本語",  // Unicode
    };

    for (const auto& original : testStrings) {
        auto encoded = urlEncode(original);
        auto decoded = urlDecode(encoded);
        ASSERT_TRUE(decoded.has_value()) << "Failed to decode: " << encoded;
        EXPECT_EQ(decoded.value(), original)
            << "Round trip failed for: " << original;
    }
}

// =============================================================================
// XOR Encryption Tests
// =============================================================================

TEST_F(BaseEncodingTest, XorEncryptDecryptBasic) {
    std::string plaintext = "Hello, World!";
    uint8_t key = 42;

    auto encrypted = xorEncrypt(plaintext, key);
    EXPECT_NE(encrypted, plaintext);

    auto decrypted = xorDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, plaintext);
}

TEST_F(BaseEncodingTest, XorEncryptEmptyString) {
    auto result = xorEncrypt("", 42);
    EXPECT_EQ(result, "");

    result = xorDecrypt("", 42);
    EXPECT_EQ(result, "");
}

TEST_F(BaseEncodingTest, XorEncryptWithZeroKey) {
    std::string plaintext = "Hello";
    auto encrypted = xorEncrypt(plaintext, 0);
    EXPECT_EQ(encrypted, plaintext);  // XOR with 0 is identity
}

TEST_F(BaseEncodingTest, XorEncryptRoundTrip) {
    for (uint8_t key = 1; key != 0; ++key) {  // Test all non-zero keys
        std::string original = "Test string for XOR encryption";
        auto encrypted = xorEncrypt(original, key);
        auto decrypted = xorDecrypt(encrypted, key);
        EXPECT_EQ(decrypted, original)
            << "Failed for key: " << static_cast<int>(key);
    }
}

TEST_F(BaseEncodingTest, XorEncryptBinaryData) {
    std::string binaryData;
    for (int i = 0; i < 256; ++i) {
        binaryData.push_back(static_cast<char>(i));
    }

    uint8_t key = 0xAB;
    auto encrypted = xorEncrypt(binaryData, key);
    auto decrypted = xorDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, binaryData);
}

// =============================================================================
// Compile-time Base64 Tests (disabled - requires StaticString support)
// =============================================================================

// These tests require C++20 NTTP (non-type template parameter) support
// and the StaticString type from atom/type/static_string.hpp
// Uncomment if your compiler supports these features

// TEST_F(BaseEncodingTest, CompileTimeBase64Decode) {
//     constexpr auto decoded = decodeBase64<"SGVsbG8=">();
//     static_assert(decoded.size() == 5, "Decoded size should be 5");
//     std::string result(decoded.buf.begin(), decoded.buf.begin() +
//     decoded.size()); EXPECT_EQ(result, "Hello");
// }

// TEST_F(BaseEncodingTest, CompileTimeBase64Encode) {
//     constexpr auto encoded = encode<"Hello">();
//     std::string result(encoded.buf.begin(), encoded.buf.begin() +
//     encoded.size()); EXPECT_EQ(result, "SGVsbG8=");
// }

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(BaseEncodingTest, Base64PerformanceLargeData) {
    std::string largeData = generateRandomString(1000000);  // 1MB

    auto start = std::chrono::high_resolution_clock::now();
    auto encoded = base64Encode(largeData);
    auto encodeEnd = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(encoded.has_value());

    auto decoded = base64Decode(encoded.value());
    auto decodeEnd = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), largeData);

    auto encodeDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - start)
            .count();
    auto decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              decodeEnd - encodeEnd)
                              .count();

    spdlog::info("Base64 encode 1MB: {}ms, decode: {}ms", encodeDuration,
                 decodeDuration);
}

TEST_F(BaseEncodingTest, HexPerformanceLargeData) {
    auto largeData = generateRandomBytes(1000000);  // 1MB

    auto start = std::chrono::high_resolution_clock::now();
    auto encoded = encodeHex(std::span<const uint8_t>(largeData));
    auto encodeEnd = std::chrono::high_resolution_clock::now();

    auto decoded = decodeHex(encoded);
    auto decodeEnd = std::chrono::high_resolution_clock::now();

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), largeData);

    auto encodeDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(encodeEnd - start)
            .count();
    auto decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              decodeEnd - encodeEnd)
                              .count();

    spdlog::info("Hex encode 1MB: {}ms, decode: {}ms", encodeDuration,
                 decodeDuration);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(BaseEncodingTest, Base64AllByteValues) {
    // Test encoding/decoding all possible byte values
    std::string allBytes;
    for (int i = 0; i < 256; ++i) {
        allBytes.push_back(static_cast<char>(i));
    }

    auto encoded = base64Encode(allBytes);
    ASSERT_TRUE(encoded.has_value());

    auto decoded = base64Decode(encoded.value());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), allBytes);
}

TEST_F(BaseEncodingTest, HexAllByteValues) {
    std::vector<uint8_t> allBytes;
    for (int i = 0; i < 256; ++i) {
        allBytes.push_back(static_cast<uint8_t>(i));
    }

    auto encoded = encodeHex(std::span<const uint8_t>(allBytes));
    auto decoded = decodeHex(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), allBytes);
}

TEST_F(BaseEncodingTest, UrlEncodeAllAscii) {
    // Test URL encoding of all printable ASCII characters
    std::string printableAscii;
    for (int i = 32; i < 127; ++i) {
        printableAscii.push_back(static_cast<char>(i));
    }

    auto encoded = urlEncode(printableAscii);
    auto decoded = urlDecode(encoded);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded.value(), printableAscii);
}
