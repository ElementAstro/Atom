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
#include "atom/utils/bit.hpp"

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

    // More than max digits should return max value
    EXPECT_EQ(createMask<uint8_t>(10), 0xFF);
    EXPECT_EQ(createMask<uint16_t>(20), 0xFFFF);

    // Negative bit count should throw exception
    EXPECT_THROW(createMask<uint8_t>(-1), BitManipulationException);
}

// Test countBytes function
TEST_F(BitManipulationTest, CountBytes) {
    // Count bits in various values
    EXPECT_EQ(countBytes<uint8_t>(0x00), 0);          // No bits set
    EXPECT_EQ(countBytes<uint8_t>(0xFF), 8);          // All bits set
    EXPECT_EQ(countBytes<uint8_t>(0x0F), 4);          // Half bits set
    EXPECT_EQ(countBytes<uint16_t>(0x5555), 8);       // Alternating bits
    EXPECT_EQ(countBytes<uint32_t>(0x12345678), 13);  // Random pattern
}

// Test reverseBits function
TEST_F(BitManipulationTest, ReverseBits) {
    // Basic reversal tests
    EXPECT_EQ(reverseBits<uint8_t>(0x01), 0x80);  // 00000001 -> 10000000
    EXPECT_EQ(reverseBits<uint8_t>(0x03), 0xC0);  // 00000011 -> 11000000
    EXPECT_EQ(reverseBits<uint8_t>(0xF0), 0x0F);  // 11110000 -> 00001111

    // Edge cases
    EXPECT_EQ(reverseBits<uint8_t>(0x00), 0x00);  // 00000000 -> 00000000
    EXPECT_EQ(reverseBits<uint8_t>(0xFF), 0xFF);  // 11111111 -> 11111111

    // 16-bit reversal
    EXPECT_EQ(reverseBits<uint16_t>(0x1234),
              0x2C48);  // 0001001000110100 -> 0010110001001000

    // Test symmetry: reversing twice should give the original value
    for (int i = 0; i < 10; ++i) {
        uint32_t value = generateRandom<uint32_t>();
        EXPECT_EQ(reverseBits(reverseBits(value)), value);
    }
}

// Test rotateLeft function
TEST_F(BitManipulationTest, RotateLeft) {
    // Basic rotation tests
    EXPECT_EQ(rotateLeft<uint8_t>(0x01, 1), 0x02);  // 00000001 -> 00000010
    EXPECT_EQ(rotateLeft<uint8_t>(0x80, 1),
              0x01);  // 10000000 -> 00000001 (wrapped)
    EXPECT_EQ(rotateLeft<uint8_t>(0x01, 7), 0x80);  // 00000001 -> 10000000

    // Full rotation should return the original value
    EXPECT_EQ(rotateLeft<uint8_t>(0xA5, 8), 0xA5);        // Full rotation
    EXPECT_EQ(rotateLeft<uint16_t>(0xABCD, 16), 0xABCD);  // Full rotation

    // Larger shifts are modulo bit width
    EXPECT_EQ(rotateLeft<uint8_t>(0x01, 9), 0x02);  // 9 % 8 = 1

    // Zero rotation should return the original value
    EXPECT_EQ(rotateLeft<uint8_t>(0x55, 0), 0x55);

    // Negative shift should throw
    EXPECT_THROW(rotateLeft<uint8_t>(0x01, -1), BitManipulationException);
}

// Test rotateRight function
TEST_F(BitManipulationTest, RotateRight) {
    // Basic rotation tests
    EXPECT_EQ(rotateRight<uint8_t>(0x02, 1), 0x01);  // 00000010 -> 00000001
    EXPECT_EQ(rotateRight<uint8_t>(0x01, 1),
              0x80);  // 00000001 -> 10000000 (wrapped)
    EXPECT_EQ(rotateRight<uint8_t>(0x80, 7), 0x01);  // 10000000 -> 00000001

    // Full rotation should return the original value
    EXPECT_EQ(rotateRight<uint8_t>(0xA5, 8), 0xA5);  // Full rotation

    // Larger shifts are modulo bit width
    EXPECT_EQ(rotateRight<uint8_t>(0x02, 9), 0x01);  // 9 % 8 = 1

    // Zero rotation should return the original value
    EXPECT_EQ(rotateRight<uint8_t>(0x55, 0), 0x55);

    // Negative shift should throw
    EXPECT_THROW(rotateRight<uint8_t>(0x01, -1), BitManipulationException);

    // Test symmetry: left rotation and right rotation should be inverses
    for (int i = 0; i < 5; ++i) {
        uint32_t value = generateRandom<uint32_t>();
        int shift = rand() % 32;
        EXPECT_EQ(rotateLeft(rotateRight(value, shift), shift), value);
        EXPECT_EQ(rotateRight(rotateLeft(value, shift), shift), value);
    }
}

// Test mergeMasks function
TEST_F(BitManipulationTest, MergeMasks) {
    // Basic merge tests
    EXPECT_EQ(mergeMasks<uint8_t>(0x0F, 0xF0),
              0xFF);  // 00001111 | 11110000 = 11111111
    EXPECT_EQ(mergeMasks<uint8_t>(0x55, 0xAA),
              0xFF);  // 01010101 | 10101010 = 11111111
    EXPECT_EQ(mergeMasks<uint8_t>(0x33, 0x33),
              0x33);  // 00110011 | 00110011 = 00110011

    // Merging with 0 should return the original mask
    EXPECT_EQ(mergeMasks<uint8_t>(0x55, 0x00), 0x55);

    // Merging with all 1s should return all 1s
    EXPECT_EQ(mergeMasks<uint8_t>(0x55, 0xFF), 0xFF);
}

// Test splitMask function
TEST_F(BitManipulationTest, SplitMask) {
    // Basic split tests
    auto [low, high] = splitMask<uint8_t>(0xFF, 4);
    EXPECT_EQ(low, 0x0F);   // Lower 4 bits: 00001111
    EXPECT_EQ(high, 0xF0);  // Upper 4 bits: 11110000

    auto [low2, high2] = splitMask<uint8_t>(0xA5, 4);
    EXPECT_EQ(low2, 0x05);   // Lower 4 bits: 00000101
    EXPECT_EQ(high2, 0xA0);  // Upper 4 bits: 10100000

    // Split at position 0 should return (0, original)
    auto [low3, high3] = splitMask<uint8_t>(0x55, 0);
    EXPECT_EQ(low3, 0x00);
    EXPECT_EQ(high3, 0x55);

    // Split at max position should return (original, 0)
    auto [low4, high4] = splitMask<uint8_t>(0x55, 8);
    EXPECT_EQ(low4, 0x55);
    EXPECT_EQ(high4, 0x00);

    // Invalid positions should throw
    EXPECT_THROW(splitMask<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(splitMask<uint8_t>(0xFF, 9), BitManipulationException);
}

// Test isBitSet, setBit, clearBit, toggleBit functions
TEST_F(BitManipulationTest, BitManipulation) {
    // Test isBitSet
    EXPECT_TRUE(isBitSet<uint8_t>(0x01, 0));   // Bit 0 of 00000001
    EXPECT_FALSE(isBitSet<uint8_t>(0x02, 0));  // Bit 0 of 00000010
    EXPECT_TRUE(isBitSet<uint8_t>(0x02, 1));   // Bit 1 of 00000010

    // Test out-of-range positions for isBitSet
    EXPECT_THROW(isBitSet<uint8_t>(0x01, -1), BitManipulationException);
    EXPECT_THROW(isBitSet<uint8_t>(0x01, 8), BitManipulationException);

    // Test setBit
    EXPECT_EQ(setBit<uint8_t>(0x00, 0), 0x01);  // Set bit 0
    EXPECT_EQ(setBit<uint8_t>(0x00, 7), 0x80);  // Set bit 7
    EXPECT_EQ(setBit<uint8_t>(0x01, 0), 0x01);  // Bit already set

    // Test out-of-range positions for setBit
    EXPECT_THROW(setBit<uint8_t>(0x00, -1), BitManipulationException);
    EXPECT_THROW(setBit<uint8_t>(0x00, 8), BitManipulationException);

    // Test clearBit
    EXPECT_EQ(clearBit<uint8_t>(0x01, 0), 0x00);  // Clear bit 0
    EXPECT_EQ(clearBit<uint8_t>(0x80, 7), 0x00);  // Clear bit 7
    EXPECT_EQ(clearBit<uint8_t>(0x00, 0), 0x00);  // Bit already cleared

    // Test out-of-range positions for clearBit
    EXPECT_THROW(clearBit<uint8_t>(0x01, -1), BitManipulationException);
    EXPECT_THROW(clearBit<uint8_t>(0x01, 8), BitManipulationException);

    // Test toggleBit
    EXPECT_EQ(toggleBit<uint8_t>(0x00, 0), 0x01);  // 0 -> 1
    EXPECT_EQ(toggleBit<uint8_t>(0x01, 0), 0x00);  // 1 -> 0
    EXPECT_EQ(toggleBit<uint8_t>(0x00, 7), 0x80);  // 0 -> 1 (msb)

    // Test out-of-range positions for toggleBit
    EXPECT_THROW(toggleBit<uint8_t>(0x01, -1), BitManipulationException);
    EXPECT_THROW(toggleBit<uint8_t>(0x01, 8), BitManipulationException);

    // Test combinations
    uint8_t value = 0x00;
    value = setBit(value, 1);     // 00000010
    value = setBit(value, 3);     // 00001010
    value = toggleBit(value, 0);  // 00001011
    value = clearBit(value, 1);   // 00001001
    EXPECT_EQ(value, 0x09);
    EXPECT_TRUE(isBitSet(value, 0));
    EXPECT_FALSE(isBitSet(value, 1));
    EXPECT_TRUE(isBitSet(value, 3));
}

// Test findFirstSetBit and findLastSetBit functions
TEST_F(BitManipulationTest, FindSetBits) {
    // Test findFirstSetBit
    EXPECT_EQ(findFirstSetBit<uint8_t>(0x01), 0);   // 00000001
    EXPECT_EQ(findFirstSetBit<uint8_t>(0x02), 1);   // 00000010
    EXPECT_EQ(findFirstSetBit<uint8_t>(0x80), 7);   // 10000000
    EXPECT_EQ(findFirstSetBit<uint8_t>(0x00), -1);  // No bits set

    // Test findLastSetBit
    EXPECT_EQ(findLastSetBit<uint8_t>(0x01), 0);   // 00000001
    EXPECT_EQ(findLastSetBit<uint8_t>(0x03), 1);   // 00000011
    EXPECT_EQ(findLastSetBit<uint8_t>(0x80), 7);   // 10000000
    EXPECT_EQ(findLastSetBit<uint8_t>(0x00), -1);  // No bits set

    // Test with multiple bits set
    EXPECT_EQ(findFirstSetBit<uint8_t>(0x28), 3);  // 00101000 (bits 3 and 5)
    EXPECT_EQ(findLastSetBit<uint8_t>(0x28), 5);   // 00101000 (bits 3 and 5)
}

// Test parallelBitOp function
TEST_F(BitManipulationTest, ParallelBitOperation) {
    // Create a vector of test values
    std::vector<uint32_t> input = {0x01, 0x03, 0x07, 0x0F,
                                   0x1F, 0x3F, 0x7F, 0xFF};

    // Apply a simple transformation - count bits in each value
    auto results = parallelBitOp<uint32_t>(input, [](uint32_t value) {
        return static_cast<uint32_t>(std::popcount(value));
    });

    // Verify results
    std::vector<uint32_t> expected = {1, 2, 3, 4, 5, 6, 7, 8};
    EXPECT_EQ(results, expected);

    // Test with larger dataset
    std::vector<uint32_t> largeInput(2000);
    std::iota(largeInput.begin(), largeInput.end(),
              0);  // Fill with 0, 1, 2, ...

    auto largeResults = parallelBitOp<uint32_t>(largeInput, [](uint32_t value) {
        return reverseBits<uint32_t>(value);
    });

    // Verify some sample results
    EXPECT_EQ(largeResults[0], reverseBits<uint32_t>(0));
    EXPECT_EQ(largeResults[1], reverseBits<uint32_t>(1));
    EXPECT_EQ(largeResults[42], reverseBits<uint32_t>(42));
    EXPECT_EQ(largeResults[1000], reverseBits<uint32_t>(1000));

    // Test that our parallel results match serial results
    for (size_t i = 0; i < 20; ++i) {
        uint32_t randomIndex = rand() % largeInput.size();
        EXPECT_EQ(largeResults[randomIndex],
                  reverseBits<uint32_t>(largeInput[randomIndex]));
    }
}

#ifdef ATOM_SIMD_SUPPORT
// Test countBitsParallel function (only if SIMD support is available)
TEST_F(BitManipulationTest, CountBitsParallel) {
    // Test with small array (should use sequential processing)
    std::vector<uint8_t> smallData = {0x01, 0x03, 0x07, 0x0F, 0xFF};
    uint64_t smallCount = countBitsParallel(smallData.data(), smallData.size());
    EXPECT_EQ(smallCount, 20);  // 1 + 2 + 3 + 4 + 8 = 20 bits

    // Test with larger array (should use parallel processing)
    std::vector<uint8_t> largeData(10000,
                                   0x55);  // 0x55 = 01010101 (4 bits per byte)
    uint64_t largeCount = countBitsParallel(largeData.data(), largeData.size());
    EXPECT_EQ(largeCount, 40000);  // 4 bits * 10000 bytes = 40000 bits

    // Test with random data
    std::vector<uint8_t> randomData(5000);
    std::generate(randomData.begin(), randomData.end(),
                  []() { return static_cast<uint8_t>(rand()); });

    // Compare parallel count with manual count
    uint64_t manualCount = 0;
    for (auto byte : randomData) {
        manualCount += std::popcount(byte);
    }

    uint64_t parallelCount =
        countBitsParallel(randomData.data(), randomData.size());
    EXPECT_EQ(parallelCount, manualCount);

    // Test exception handling with a mock that would throw
    // This is hard to test directly, so we'll just verify the function exists
    EXPECT_NO_THROW(countBitsParallel(largeData.data(), largeData.size()));
}
#endif

// Test with various integral types
TEST_F(BitManipulationTest, VariousIntegralTypes) {
    // Test with uint8_t
    EXPECT_EQ(createMask<uint8_t>(3), 0x07);
    EXPECT_EQ(countBytes<uint8_t>(0x55), 4);

    // Test with uint16_t
    EXPECT_EQ(createMask<uint16_t>(8), 0x00FF);
    EXPECT_EQ(countBytes<uint16_t>(0x5555), 8);

    // Test with uint32_t
    EXPECT_EQ(createMask<uint32_t>(16), 0x0000FFFF);
    EXPECT_EQ(countBytes<uint32_t>(0x55555555), 16);

    // Test with uint64_t
    EXPECT_EQ(createMask<uint64_t>(32), 0x00000000FFFFFFFF);
    EXPECT_EQ(countBytes<uint64_t>(0x5555555555555555ULL), 32);
}

// Test constexpr behavior at compile time
TEST_F(BitManipulationTest, ConstexprBehavior) {
    // These values should be computed at compile time
    constexpr uint8_t mask8 = createMask<uint8_t>(4);
    EXPECT_EQ(mask8, 0x0F);

    constexpr uint32_t mask32 = createMask<uint32_t>(16);
    EXPECT_EQ(mask32, 0x0000FFFF);

    constexpr uint8_t bits8 = countBytes<uint8_t>(0x55);
    EXPECT_EQ(bits8, 4);

    constexpr uint8_t reversed = reverseBits<uint8_t>(0x0F);
    EXPECT_EQ(reversed, 0xF0);

    constexpr int firstBit = findFirstSetBit<uint16_t>(0x0100);
    EXPECT_EQ(firstBit, 8);

    constexpr int lastBit = findLastSetBit<uint16_t>(0x0100);
    EXPECT_EQ(lastBit, 8);
}

// Test with random values to increase coverage
TEST_F(BitManipulationTest, RandomValueTests) {
    // Run multiple random tests for better coverage
    for (int i = 0; i < 100; ++i) {
        uint32_t value = generateRandom<uint32_t>();
        int position = rand() % 32;

        // Test that toggling a bit twice returns the original value
        EXPECT_EQ(toggleBit(toggleBit(value, position), position), value);

        // Test that setting and then clearing a bit returns the original value
        // with that bit cleared
        uint32_t withSetBit = setBit(value, position);
        EXPECT_EQ(clearBit(withSetBit, position), clearBit(value, position));

        // Test that merging with itself is idempotent
        EXPECT_EQ(mergeMasks(value, value), value);

        // Test for bit count consistency
        uint32_t bits = countBytes(value);
        uint32_t calculatedBits = 0;
        for (int j = 0; j < 32; ++j) {
            if (isBitSet(value, j)) {
                calculatedBits++;
            }
        }
        EXPECT_EQ(bits, calculatedBits);
    }
}

// Test error handling in edge cases
TEST_F(BitManipulationTest, ErrorHandling) {
    // Test negative bits in createMask
    EXPECT_THROW(createMask<uint32_t>(-1), BitManipulationException);

    // Test negative shift in rotate functions
    EXPECT_THROW(rotateLeft<uint32_t>(0x01, -1), BitManipulationException);
    EXPECT_THROW(rotateRight<uint32_t>(0x01, -1), BitManipulationException);

    // Test out of range position in split mask
    EXPECT_THROW(splitMask<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(splitMask<uint8_t>(0xFF, 9), BitManipulationException);

    // Test out of range positions in bit manipulation functions
    EXPECT_THROW(isBitSet<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(isBitSet<uint8_t>(0xFF, 8), BitManipulationException);

    EXPECT_THROW(setBit<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(setBit<uint8_t>(0xFF, 8), BitManipulationException);

    EXPECT_THROW(clearBit<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(clearBit<uint8_t>(0xFF, 8), BitManipulationException);

    EXPECT_THROW(toggleBit<uint8_t>(0xFF, -1), BitManipulationException);
    EXPECT_THROW(toggleBit<uint8_t>(0xFF, 8), BitManipulationException);
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_BIT_HPP
