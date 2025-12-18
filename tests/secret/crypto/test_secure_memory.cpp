/*
 * test_secure_memory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/secret/crypto/secure_memory.hpp"

namespace atom::secret::test {

class SecureMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(SecureMemoryTest, SecureClearVector) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
    SecureMemory::secureClear(data);

    for (uint8_t byte : data) {
        EXPECT_EQ(byte, 0);
    }
}

TEST_F(SecureMemoryTest, SecureClearString) {
    std::string data = "sensitive data";
    SecureMemory::secureClear(data);

    for (char c : data) {
        EXPECT_EQ(c, '\0');
    }
}

TEST_F(SecureMemoryTest, SecureClearPointer) {
    uint8_t data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    SecureMemory::secureClear(data, sizeof(data));

    for (size_t i = 0; i < sizeof(data); ++i) {
        EXPECT_EQ(data[i], 0);
    }
}

TEST_F(SecureMemoryTest, SecureCompareEqual) {
    std::vector<uint8_t> a = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> b = {0x01, 0x02, 0x03, 0x04};

    EXPECT_TRUE(SecureMemory::secureCompare(a, b));
}

TEST_F(SecureMemoryTest, SecureCompareNotEqual) {
    std::vector<uint8_t> a = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> b = {0x01, 0x02, 0x03, 0x05};

    EXPECT_FALSE(SecureMemory::secureCompare(a, b));
}

TEST_F(SecureMemoryTest, SecureCompareDifferentLength) {
    std::vector<uint8_t> a = {0x01, 0x02, 0x03};
    std::vector<uint8_t> b = {0x01, 0x02, 0x03, 0x04};

    EXPECT_FALSE(SecureMemory::secureCompare(a, b));
}

TEST_F(SecureMemoryTest, SecureCompareEmpty) {
    std::vector<uint8_t> a;
    std::vector<uint8_t> b;

    EXPECT_TRUE(SecureMemory::secureCompare(a, b));
}

TEST_F(SecureMemoryTest, IsMemoryLockingAvailable) {
    // Just check that the function doesn't crash
    bool available = SecureMemory::isMemoryLockingAvailable();
    (void)available;  // May or may not be available depending on platform
}

TEST_F(SecureMemoryTest, SecureBuffer) {
    SecureBuffer buffer(32);

    EXPECT_EQ(buffer.size(), 32);
    EXPECT_NE(buffer.data(), nullptr);

    // Write some data
    std::memset(buffer.data(), 0xAB, buffer.size());

    // Verify data was written
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer.data()[i], 0xAB);
    }
}

TEST_F(SecureMemoryTest, SecureBufferClear) {
    SecureBuffer buffer(32);
    std::memset(buffer.data(), 0xAB, buffer.size());

    buffer.clear();

    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer.data()[i], 0);
    }
}

TEST_F(SecureMemoryTest, SecureBufferMove) {
    SecureBuffer buffer1(32);
    std::memset(buffer1.data(), 0xAB, buffer1.size());

    SecureBuffer buffer2 = std::move(buffer1);

    EXPECT_EQ(buffer2.size(), 32);
    for (size_t i = 0; i < buffer2.size(); ++i) {
        EXPECT_EQ(buffer2.data()[i], 0xAB);
    }
}

}  // namespace atom::secret::test
