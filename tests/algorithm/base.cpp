#include "atom/algorithm/base.hpp"
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <vector>

TEST(Base64Test, Encode) {
    std::string data = "Hello, World!";
    std::string encoded = atom::algorithm::base64Encode(data);
    EXPECT_EQ(encoded, "SGVsbG8sIFdvcmxkIQ==");
}

TEST(Base64Test, Decode) {
    std::string encoded = "SGVsbG8sIFdvcmxkIQ==";
    std::string decoded = atom::algorithm::base64Decode(encoded);
    EXPECT_EQ(decoded, "Hello, World!");
}

TEST(Base64Test, EncodeDecode) {
    std::string data = "Hello, World!";
    std::string encoded = atom::algorithm::base64Encode(data);
    std::string decoded = atom::algorithm::base64Decode(encoded);
    EXPECT_EQ(decoded, data);
}

/*
TODO: Fix the following tests, they are not working as expected.

TEST(ConstBase64Test, Encode) {
    constexpr StaticString<13> DATA{"Hello, World!"};
    constexpr auto ENCODED = atom::algorithm::cbase64Encode(DATA);
    EXPECT_STREQ(ENCODED.cStr(), "SGVsbG8sIFdvcmxkIQ==");
}

TEST(ConstBase64Test, Decode) {
    constexpr StaticString<20> encoded("SGVsbG8sIFdvcmxkIQ==");
    constexpr auto decoded = atom::algorithm::cbase64Decode(encoded);
    EXPECT_STREQ(decoded.cStr(), "Hello, World!");
}

TEST(ConstBase64Test, EncodeDecode) {
    constexpr StaticString<13> data("Hello, World!");
    constexpr auto encoded = atom::algorithm::cbase64Encode(data);
    constexpr auto decoded = atom::algorithm::cbase64Decode(encoded);
    EXPECT_STREQ(decoded.cStr(), "Hello, World!");
}
*/

TEST(XORCipherTest, EncryptDecrypt) {
    std::string data = "Hello, World!";
    uint8_t key = 0xAA;
    std::string encrypted = atom::algorithm::xorEncrypt(data, key);
    std::string decrypted = atom::algorithm::xorDecrypt(encrypted, key);
    EXPECT_EQ(decrypted, data);
}

TEST(BaseAlgorithmTest, EncodeBase32) {
    std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    std::string expected = "JBSWY3DP";
    std::string result = atom::algorithm::encodeBase32(data);
    ASSERT_EQ(result, expected);
}

TEST(BaseAlgorithmTest, DecodeBase32) {
    std::string encoded = "JBSWY3DP";
    std::vector<uint8_t> expected = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    std::vector<uint8_t> result = atom::algorithm::decodeBase32(encoded);
    ASSERT_EQ(result, expected);
}

TEST(BaseAlgorithmTest, IsBase64Valid) {
    std::string validBase64 = "SGVsbG8sIFdvcmxkIQ==";
    ASSERT_TRUE(atom::algorithm::isBase64(validBase64));
}

TEST(BaseAlgorithmTest, IsBase64Invalid) {
    std::string invalidBase64 = "SGVsbG8sIFdvcmxkIQ";
    ASSERT_FALSE(atom::algorithm::isBase64(invalidBase64));
}
