// filepath: /home/max/Atom-1/atom/utils/test_bit.hpp
/*
 * test_bit.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for bit manipulation utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_BIT_HPP
#define ATOM_UTILS_TEST_BIT_HPP

#include <gtest/gtest.h>
#include <climits>
#include <numeric>
#include <random>
#include <vector>
#include "atom/utils/core/bit.hpp"

namespace atom::utils::test {

class BitManipulationTest : public ::testing::Test {
protected:
    // Generate random unsigned integers for testing
    template <UnsignedIntegral T>
    static T generateRandom() {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<T> dist(
            0, std::numeric_limits<T>::max());
        return dist(rng);
    }
};

// Test createMask function
TEST_F(BitManipulationTest, CreateMask) {
    // Basic mask creation
    EXPECT_EQ(createMask<uint8_t>(3), 0x07);
    EXPECT_EQ(createMask<uint16_t>(8), 0x00FF);
    EXPECT_EQ(createMask<uint32_t>(16), 0x0000FFFF);

    // Edge cases
    EXPECT_EQ(createMask<uint8_t>(0), 0x00);      // No bits set
    EXPECT_EQ(createMask<uint8_t>(8), 0xFF);      // All bits set
    EXPECT_EQ(createMask<uint16_t>(16), 0xFFFF);  // All bits set
    EXPECT_EQ(createMask<uint32_t>(32), 0xFFFFFFFF);  // All bits set

    // Test with different types
    EXPECT_EQ(createMask<uint64_t>(4), 0x0F);
    EXPECT_EQ(createMask<uint64_t>(32), 0xFFFFFFFF);
}

TEST_F(BitManipulationTest, CreateMaskErrorHandling) {
    // Test negative bits parameter
    EXPECT_THROW(createMask<uint8_t>(-1), BitManipulationException);
    EXPECT_THROW(createMask<uint16_t>(-5), BitManipulationException);
    EXPECT_THROW(createMask<uint32_t>(-100), BitManipulationException);
}

TEST_F(BitManipulationTest, CreateMaskBoundaryConditions) {
    // Test boundary conditions for different types
    EXPECT_EQ(createMask<uint8_t>(7), 0x7F);
    EXPECT_EQ(createMask<uint8_t>(9), 0xFF);   // Should cap at max value
    EXPECT_EQ(createMask<uint8_t>(100), 0xFF); // Should cap at max value

    EXPECT_EQ(createMask<uint16_t>(15), 0x7FFF);
    EXPECT_EQ(createMask<uint16_t>(17), 0xFFFF); // Should cap at max value

    EXPECT_EQ(createMask<uint32_t>(31), 0x7FFFFFFF);
    EXPECT_EQ(createMask<uint32_t>(33), 0xFFFFFFFF); // Should cap at max value
}

// Test countBytes function
TEST_F(BitManipulationTest, CountBytes) {
    // Test known values
    EXPECT_EQ(countBytes<uint8_t>(0x00), 0);
    EXPECT_EQ(countBytes<uint8_t>(0x01), 1);
    EXPECT_EQ(countBytes<uint8_t>(0x03), 2);
    EXPECT_EQ(countBytes<uint8_t>(0x07), 3);
    EXPECT_EQ(countBytes<uint8_t>(0x0F), 4);
    EXPECT_EQ(countBytes<uint8_t>(0xFF), 8);

    // Test 16-bit values
    EXPECT_EQ(countBytes<uint16_t>(0x0000), 0);
    EXPECT_EQ(countBytes<uint16_t>(0x0001), 1);
    EXPECT_EQ(countBytes<uint16_t>(0x00FF), 8);
    EXPECT_EQ(countBytes<uint16_t>(0xFFFF), 16);

    // Test 32-bit values
    EXPECT_EQ(countBytes<uint32_t>(0x00000000), 0);
    EXPECT_EQ(countBytes<uint32_t>(0x00000001), 1);
    EXPECT_EQ(countBytes<uint32_t>(0x0000FFFF), 16);
    EXPECT_EQ(countBytes<uint32_t>(0xFFFFFFFF), 32);
}

TEST_F(BitManipulationTest, CountBytesRandomValues) {
    // Test with random values
    for (int i = 0; i < 100; ++i) {
        uint32_t value = generateRandom<uint32_t>();
        uint32_t expected = __builtin_popcountl(value);
        EXPECT_EQ(countBytes(value), expected);
    }
}

// Test isPowerOfTwo function
TEST_F(BitManipulationTest, IsPowerOfTwo) {
    // Test powers of two
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(1));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(2));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(4));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(8));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(16));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(32));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(64));
    EXPECT_TRUE(isPowerOfTwo<uint8_t>(128));

    // Test non-powers of two
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(0));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(3));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(5));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(6));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(7));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(9));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(15));
    EXPECT_FALSE(isPowerOfTwo<uint8_t>(255));

    // Test larger values
    EXPECT_TRUE(isPowerOfTwo<uint32_t>(1024));
    EXPECT_TRUE(isPowerOfTwo<uint32_t>(65536));
    EXPECT_FALSE(isPowerOfTwo<uint32_t>(1023));
    EXPECT_FALSE(isPowerOfTwo<uint32_t>(65535));
}

// Test nextPowerOfTwo function
TEST_F(BitManipulationTest, NextPowerOfTwo) {
    // Test basic cases
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(1), 1);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(2), 2);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(3), 4);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(5), 8);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(9), 16);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(17), 32);

    // Test edge cases
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(0), 1);
    EXPECT_EQ(nextPowerOfTwo<uint8_t>(128), 128);

    // Test larger values
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(1000), 1024);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(65536), 65536);
    EXPECT_EQ(nextPowerOfTwo<uint32_t>(65537), 131072);
}

// Test setBit function
TEST_F(BitManipulationTest, SetBit) {
    uint8_t value = 0x00;
    
    // Set individual bits
    EXPECT_EQ(setBit(value, 0), 0x01);
    EXPECT_EQ(setBit(value, 1), 0x02);
    EXPECT_EQ(setBit(value, 2), 0x04);
    EXPECT_EQ(setBit(value, 7), 0x80);

    // Set bit that's already set
    value = 0x01;
    EXPECT_EQ(setBit(value, 0), 0x01);

    // Set multiple bits
    value = 0x05; // 0101
    EXPECT_EQ(setBit(value, 1), 0x07); // 0111
}

// Test clearBit function
TEST_F(BitManipulationTest, ClearBit) {
    uint8_t value = 0xFF;
    
    // Clear individual bits
    EXPECT_EQ(clearBit(value, 0), 0xFE);
    EXPECT_EQ(clearBit(value, 1), 0xFD);
    EXPECT_EQ(clearBit(value, 7), 0x7F);

    // Clear bit that's already clear
    value = 0xFE;
    EXPECT_EQ(clearBit(value, 0), 0xFE);

    // Clear multiple bits
    value = 0x07; // 0111
    EXPECT_EQ(clearBit(value, 1), 0x05); // 0101
}

// Test toggleBit function
TEST_F(BitManipulationTest, ToggleBit) {
    uint8_t value = 0x00;
    
    // Toggle bits from 0
    EXPECT_EQ(toggleBit(value, 0), 0x01);
    EXPECT_EQ(toggleBit(value, 1), 0x02);
    EXPECT_EQ(toggleBit(value, 7), 0x80);

    // Toggle bits back to 0
    value = 0x01;
    EXPECT_EQ(toggleBit(value, 0), 0x00);

    value = 0x80;
    EXPECT_EQ(toggleBit(value, 7), 0x00);

    // Toggle multiple times
    value = 0x05; // 0101
    EXPECT_EQ(toggleBit(value, 1), 0x07); // 0111
    EXPECT_EQ(toggleBit(toggleBit(value, 1), 1), value); // Should return to original
}

// Test getBit function
TEST_F(BitManipulationTest, GetBit) {
    uint8_t value = 0x55; // 01010101
    
    // Test individual bits
    EXPECT_TRUE(getBit(value, 0));
    EXPECT_FALSE(getBit(value, 1));
    EXPECT_TRUE(getBit(value, 2));
    EXPECT_FALSE(getBit(value, 3));
    EXPECT_TRUE(getBit(value, 4));
    EXPECT_FALSE(getBit(value, 5));
    EXPECT_TRUE(getBit(value, 6));
    EXPECT_FALSE(getBit(value, 7));

    // Test all zeros
    value = 0x00;
    for (int i = 0; i < 8; ++i) {
        EXPECT_FALSE(getBit(value, i));
    }

    // Test all ones
    value = 0xFF;
    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(getBit(value, i));
    }
}

// Test bit manipulation with different integer types
TEST_F(BitManipulationTest, DifferentIntegerTypes) {
    // Test with uint16_t
    uint16_t val16 = 0x1234;
    EXPECT_EQ(setBit(val16, 15), 0x9234);
    EXPECT_EQ(clearBit(val16, 4), 0x1224);
    EXPECT_TRUE(getBit(val16, 5));
    EXPECT_FALSE(getBit(val16, 15));

    // Test with uint32_t
    uint32_t val32 = 0x12345678;
    EXPECT_EQ(setBit(val32, 31), 0x92345678);
    EXPECT_EQ(clearBit(val32, 3), 0x12345670);
    EXPECT_TRUE(getBit(val32, 3));
    EXPECT_FALSE(getBit(val32, 31));

    // Test with uint64_t
    uint64_t val64 = 0x123456789ABCDEF0;
    EXPECT_EQ(setBit(val64, 63), 0x923456789ABCDEF0);
    EXPECT_EQ(clearBit(val64, 4), 0x123456789ABCDEE0);
    EXPECT_TRUE(getBit(val64, 4));
    EXPECT_FALSE(getBit(val64, 63));
}

// Test error handling for bit position out of range
TEST_F(BitManipulationTest, BitPositionErrorHandling) {
    uint8_t value = 0x55;

    // Test invalid bit positions (should throw or handle gracefully)
    EXPECT_THROW(setBit(value, 8), std::out_of_range);
    EXPECT_THROW(clearBit(value, 8), std::out_of_range);
    EXPECT_THROW(toggleBit(value, 8), std::out_of_range);
    EXPECT_THROW(getBit(value, 8), std::out_of_range);

    // Test with larger types
    uint16_t value16 = 0x1234;
    EXPECT_THROW(setBit(value16, 16), std::out_of_range);
    EXPECT_THROW(getBit(value16, 16), std::out_of_range);

    uint32_t value32 = 0x12345678;
    EXPECT_THROW(setBit(value32, 32), std::out_of_range);
    EXPECT_THROW(getBit(value32, 32), std::out_of_range);
}

// Test rotateLeft function
TEST_F(BitManipulationTest, RotateLeft) {
    // Test 8-bit rotation
    uint8_t val8 = 0x81; // 10000001
    EXPECT_EQ(rotateLeft(val8, 1), 0x03); // 00000011
    EXPECT_EQ(rotateLeft(val8, 2), 0x06); // 00000110
    EXPECT_EQ(rotateLeft(val8, 8), val8); // Full rotation returns original

    // Test 16-bit rotation
    uint16_t val16 = 0x8001;
    EXPECT_EQ(rotateLeft(val16, 1), 0x0003);
    EXPECT_EQ(rotateLeft(val16, 16), val16); // Full rotation returns original

    // Test rotation by 0
    EXPECT_EQ(rotateLeft(val8, 0), val8);
    EXPECT_EQ(rotateLeft(val16, 0), val16);
}

// Test rotateRight function
TEST_F(BitManipulationTest, RotateRight) {
    // Test 8-bit rotation
    uint8_t val8 = 0x81; // 10000001
    EXPECT_EQ(rotateRight(val8, 1), 0xC0); // 11000000
    EXPECT_EQ(rotateRight(val8, 2), 0x60); // 01100000
    EXPECT_EQ(rotateRight(val8, 8), val8); // Full rotation returns original

    // Test 16-bit rotation
    uint16_t val16 = 0x8001;
    EXPECT_EQ(rotateRight(val16, 1), 0xC000);
    EXPECT_EQ(rotateRight(val16, 16), val16); // Full rotation returns original

    // Test rotation by 0
    EXPECT_EQ(rotateRight(val8, 0), val8);
    EXPECT_EQ(rotateRight(val16, 0), val16);
}

// Test reverseBits function
TEST_F(BitManipulationTest, ReverseBits) {
    // Test 8-bit reversal
    EXPECT_EQ(reverseBits<uint8_t>(0x00), 0x00);
    EXPECT_EQ(reverseBits<uint8_t>(0xFF), 0xFF);
    EXPECT_EQ(reverseBits<uint8_t>(0x01), 0x80);
    EXPECT_EQ(reverseBits<uint8_t>(0x80), 0x01);
    EXPECT_EQ(reverseBits<uint8_t>(0x0F), 0xF0);
    EXPECT_EQ(reverseBits<uint8_t>(0xF0), 0x0F);

    // Test 16-bit reversal
    EXPECT_EQ(reverseBits<uint16_t>(0x0001), 0x8000);
    EXPECT_EQ(reverseBits<uint16_t>(0x8000), 0x0001);
    EXPECT_EQ(reverseBits<uint16_t>(0x00FF), 0xFF00);

    // Test that double reversal returns original
    uint8_t original = 0x5A;
    EXPECT_EQ(reverseBits(reverseBits(original)), original);
}

// Test extractBits function
TEST_F(BitManipulationTest, ExtractBits) {
    uint8_t value = 0xAB; // 10101011

    // Extract single bits
    EXPECT_EQ(extractBits(value, 0, 1), 0x01); // Bit 0
    EXPECT_EQ(extractBits(value, 1, 1), 0x01); // Bit 1
    EXPECT_EQ(extractBits(value, 2, 1), 0x00); // Bit 2
    EXPECT_EQ(extractBits(value, 7, 1), 0x01); // Bit 7

    // Extract multiple bits
    EXPECT_EQ(extractBits(value, 0, 4), 0x0B); // Lower 4 bits
    EXPECT_EQ(extractBits(value, 4, 4), 0x0A); // Upper 4 bits
    EXPECT_EQ(extractBits(value, 2, 3), 0x02); // Bits 2-4

    // Extract all bits
    EXPECT_EQ(extractBits(value, 0, 8), value);
}

// Test insertBits function
TEST_F(BitManipulationTest, InsertBits) {
    uint8_t target = 0x00;

    // Insert single bits
    EXPECT_EQ(insertBits(target, 0x01, 0, 1), 0x01);
    EXPECT_EQ(insertBits(target, 0x01, 7, 1), 0x80);

    // Insert multiple bits
    target = 0x00;
    EXPECT_EQ(insertBits(target, 0x0F, 0, 4), 0x0F);
    EXPECT_EQ(insertBits(target, 0x0F, 4, 4), 0xF0);

    // Insert into existing value
    target = 0xAA; // 10101010
    EXPECT_EQ(insertBits(target, 0x05, 1, 3), 0xAA); // Should replace bits 1-3
}

// Test performance with large datasets
TEST_F(BitManipulationTest, PerformanceTest) {
    const size_t testSize = 10000;
    std::vector<uint32_t> testData;
    testData.reserve(testSize);

    // Generate test data
    for (size_t i = 0; i < testSize; ++i) {
        testData.push_back(generateRandom<uint32_t>());
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Perform operations on all test data
    uint32_t totalBits = 0;
    for (const auto& value : testData) {
        totalBits += countBytes(value);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 10000); // 10ms max
    EXPECT_GT(totalBits, 0); // Should have counted some bits
}

// Test thread safety
TEST_F(BitManipulationTest, ThreadSafety) {
    const int numThreads = 4;
    const int operationsPerThread = 1000;
    std::vector<std::future<bool>> futures;

    for (int t = 0; t < numThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [operationsPerThread]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                uint32_t value = generateRandom<uint32_t>();

                // Perform various operations
                uint32_t bits = countBytes(value);
                bool isPow2 = isPowerOfTwo(value);
                uint32_t nextPow2 = nextPowerOfTwo(value);
                uint32_t mask = createMask<uint32_t>(bits % 32);

                // Basic sanity checks
                if (bits > 32 || (isPow2 && value == 0) || nextPow2 < value) {
                    return false;
                }
            }
            return true;
        }));
    }

    // Wait for all threads and check results
    for (auto& future : futures) {
        EXPECT_TRUE(future.get());
    }
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_BIT_HPP
