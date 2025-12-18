/*
 * test_types.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/core/types.hpp"

namespace atom::secret::test {

class TypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TypesTest, BytesToHex) {
    ByteVector data = {0x00, 0x01, 0x02, 0x0A, 0x0F, 0xFF};
    std::string hex = bytesToHex(data);

    EXPECT_EQ(hex, "0001020a0fff");
}

TEST_F(TypesTest, BytesToHexUppercase) {
    ByteVector data = {0x00, 0x01, 0x02, 0x0A, 0x0F, 0xFF};
    std::string hex = bytesToHex(data, true);

    EXPECT_EQ(hex, "0001020A0FFF");
}

TEST_F(TypesTest, HexToBytes) {
    std::string hex = "0001020a0fff";
    ByteVector data = hexToBytes(hex);

    EXPECT_EQ(data.size(), 6);
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x01);
    EXPECT_EQ(data[2], 0x02);
    EXPECT_EQ(data[3], 0x0A);
    EXPECT_EQ(data[4], 0x0F);
    EXPECT_EQ(data[5], 0xFF);
}

TEST_F(TypesTest, HexToBytesUppercase) {
    std::string hex = "0001020A0FFF";
    ByteVector data = hexToBytes(hex);

    EXPECT_EQ(data.size(), 6);
    EXPECT_EQ(data[5], 0xFF);
}

TEST_F(TypesTest, HexToBytesInvalidLength) {
    std::string hex = "001";  // Odd length
    ByteVector data = hexToBytes(hex);

    EXPECT_TRUE(data.empty());
}

TEST_F(TypesTest, HexToBytesInvalidCharacter) {
    std::string hex = "00GG";  // Invalid hex character
    ByteVector data = hexToBytes(hex);

    EXPECT_TRUE(data.empty());
}

TEST_F(TypesTest, HexRoundTrip) {
    ByteVector original = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
    std::string hex = bytesToHex(original);
    ByteVector decoded = hexToBytes(hex);

    EXPECT_EQ(original, decoded);
}

TEST_F(TypesTest, EmptyHex) {
    ByteVector empty;
    std::string hex = bytesToHex(empty);
    EXPECT_TRUE(hex.empty());

    ByteVector decoded = hexToBytes("");
    EXPECT_TRUE(decoded.empty());
}

TEST_F(TypesTest, NowFunction) {
    auto before = std::chrono::system_clock::now();
    auto result = now();
    auto after = std::chrono::system_clock::now();

    EXPECT_GE(result, before);
    EXPECT_LE(result, after);
}

TEST_F(TypesTest, Constants) {
    EXPECT_EQ(constants::AES_128_KEY_SIZE, 16);
    EXPECT_EQ(constants::AES_256_KEY_SIZE, 32);
    EXPECT_EQ(constants::GCM_IV_SIZE, 12);
    EXPECT_EQ(constants::GCM_TAG_SIZE, 16);
    EXPECT_EQ(constants::PBKDF2_DEFAULT_ITERATIONS, 100000);
}

}  // namespace atom::secret::test
