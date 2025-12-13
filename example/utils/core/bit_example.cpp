/**
 * @file bit_example.cpp
 * @brief Comprehensive examples for atom::utils bit manipulation utilities
 *
 * This example demonstrates all bit manipulation functions including:
 * - Mask creation and manipulation (createMask, mergeMasks, splitMask)
 * - Bit counting (countBytes)
 * - Bit reversal (reverseBits)
 * - Bit rotation (rotateLeft, rotateRight)
 * - Individual bit operations (isBitSet, setBit, clearBit, toggleBit)
 * - Bit searching (findFirstSetBit, findLastSetBit)
 * - Parallel operations (parallelBitOp)
 */

#include "atom/utils/core/bit.hpp"

#include <bitset>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// Helper to print binary representation
template <typename T>
void printBinary(const std::string& label, T value) {
    constexpr int bits = sizeof(T) * 8;
    std::cout << label << ": " << std::bitset<bits>(value) << " (0x"
              << std::hex << std::uppercase << static_cast<uint64_t>(value)
              << std::dec << ", " << static_cast<uint64_t>(value) << ")"
              << std::endl;
}

// ============================================
// 1. Mask Creation
// ============================================
void demonstrateMaskCreation() {
    printSection("1. Mask Creation");

    std::cout << "--- createMask ---" << std::endl;
    std::cout << "Creates a bitmask with n lower bits set to 1" << std::endl;

    for (int bits = 0; bits <= 8; ++bits) {
        auto mask = createMask<uint8_t>(bits);
        std::cout << "  createMask<uint8_t>(" << bits << "): ";
        printBinary("", mask);
    }

    std::cout << "\n--- 16-bit masks ---" << std::endl;
    for (int bits : {0, 4, 8, 12, 16}) {
        auto mask = createMask<uint16_t>(bits);
        std::cout << "  createMask<uint16_t>(" << bits << "): ";
        std::cout << std::bitset<16>(mask) << std::endl;
    }

    std::cout << "\n--- 32-bit masks ---" << std::endl;
    for (int bits : {0, 8, 16, 24, 32}) {
        auto mask = createMask<uint32_t>(bits);
        std::cout << "  createMask<uint32_t>(" << bits << "): 0x"
                  << std::hex << std::setw(8) << std::setfill('0') << mask
                  << std::dec << std::endl;
    }
}

// ============================================
// 2. Bit Counting
// ============================================
void demonstrateBitCounting() {
    printSection("2. Bit Counting");

    std::cout << "--- countBytes (popcount) ---" << std::endl;
    std::cout << "Counts the number of set bits (1s) in a value" << std::endl;

    std::vector<uint8_t> values8 = {0, 1, 0b01010101, 0b11110000, 0xFF};
    for (auto val : values8) {
        std::cout << "  countBytes(" << std::bitset<8>(val) << "): "
                  << countBytes(val) << std::endl;
    }

    std::cout << "\n--- 32-bit values ---" << std::endl;
    std::vector<uint32_t> values32 = {0, 1, 0xFFFF, 0xFFFFFFFF, 0xAAAAAAAA};
    for (auto val : values32) {
        std::cout << "  countBytes(0x" << std::hex << val << std::dec << "): "
                  << countBytes(val) << std::endl;
    }
}

// ============================================
// 3. Bit Reversal
// ============================================
void demonstrateBitReversal() {
    printSection("3. Bit Reversal");

    std::cout << "--- reverseBits ---" << std::endl;
    std::cout << "Reverses the order of bits in a value" << std::endl;

    std::vector<uint8_t> values = {0b00000001, 0b10000000, 0b11110000,
                                    0b10101010, 0b11001100};

    for (auto val : values) {
        auto reversed = reverseBits(val);
        std::cout << "  " << std::bitset<8>(val) << " -> "
                  << std::bitset<8>(reversed) << std::endl;
    }

    std::cout << "\n--- 16-bit reversal ---" << std::endl;
    uint16_t val16 = 0b1111000011110000;
    auto rev16 = reverseBits(val16);
    std::cout << "  " << std::bitset<16>(val16) << " -> "
              << std::bitset<16>(rev16) << std::endl;
}

// ============================================
// 4. Bit Rotation
// ============================================
void demonstrateBitRotation() {
    printSection("4. Bit Rotation");

    uint8_t value = 0b10110001;
    std::cout << "Original value: " << std::bitset<8>(value) << std::endl;

    std::cout << "\n--- rotateLeft ---" << std::endl;
    for (int shift = 1; shift <= 8; ++shift) {
        auto rotated = rotateLeft(value, shift);
        std::cout << "  rotateLeft(" << shift << "): " << std::bitset<8>(rotated)
                  << std::endl;
    }

    std::cout << "\n--- rotateRight ---" << std::endl;
    for (int shift = 1; shift <= 8; ++shift) {
        auto rotated = rotateRight(value, shift);
        std::cout << "  rotateRight(" << shift << "): "
                  << std::bitset<8>(rotated) << std::endl;
    }

    std::cout << "\n--- 32-bit rotation ---" << std::endl;
    uint32_t val32 = 0x12345678;
    std::cout << "Original: 0x" << std::hex << val32 << std::dec << std::endl;
    std::cout << "  rotateLeft(8): 0x" << std::hex << rotateLeft(val32, 8)
              << std::dec << std::endl;
    std::cout << "  rotateRight(8): 0x" << std::hex << rotateRight(val32, 8)
              << std::dec << std::endl;
}

// ============================================
// 5. Mask Merging and Splitting
// ============================================
void demonstrateMaskOperations() {
    printSection("5. Mask Merging and Splitting");

    std::cout << "--- mergeMasks ---" << std::endl;
    uint8_t mask1 = 0b11110000;
    uint8_t mask2 = 0b00001111;

    std::cout << "  Mask 1: " << std::bitset<8>(mask1) << std::endl;
    std::cout << "  Mask 2: " << std::bitset<8>(mask2) << std::endl;

    auto merged = mergeMasks(mask1, mask2);
    std::cout << "  Merged: " << std::bitset<8>(merged) << std::endl;

    std::cout << "\n--- splitMask ---" << std::endl;
    uint8_t fullMask = 0b11111111;
    std::cout << "  Full mask: " << std::bitset<8>(fullMask) << std::endl;

    for (int pos = 0; pos <= 8; pos += 2) {
        auto [lower, upper] = splitMask(fullMask, pos);
        std::cout << "  Split at " << pos << ": lower=" << std::bitset<8>(lower)
                  << ", upper=" << std::bitset<8>(upper) << std::endl;
    }

    std::cout << "\n--- Complex split example ---" << std::endl;
    uint16_t value16 = 0xABCD;
    std::cout << "  Value: 0x" << std::hex << value16 << std::dec << std::endl;

    auto [low8, high8] = splitMask(value16, 8);
    std::cout << "  Split at 8: low=0x" << std::hex << low8 << ", high=0x"
              << high8 << std::dec << std::endl;
}

// ============================================
// 6. Individual Bit Operations
// ============================================
void demonstrateIndividualBitOps() {
    printSection("6. Individual Bit Operations");

    uint8_t value = 0b10101010;
    std::cout << "Original value: " << std::bitset<8>(value) << std::endl;

    std::cout << "\n--- isBitSet ---" << std::endl;
    for (int pos = 0; pos < 8; ++pos) {
        std::cout << "  Bit " << pos << " is "
                  << (isBitSet(value, pos) ? "SET" : "CLEAR") << std::endl;
    }

    std::cout << "\n--- setBit ---" << std::endl;
    uint8_t val = 0;
    std::cout << "  Starting with: " << std::bitset<8>(val) << std::endl;
    for (int pos : {0, 2, 4, 6}) {
        val = setBit(val, pos);
        std::cout << "  After setBit(" << pos << "): " << std::bitset<8>(val)
                  << std::endl;
    }

    std::cout << "\n--- clearBit ---" << std::endl;
    val = 0xFF;
    std::cout << "  Starting with: " << std::bitset<8>(val) << std::endl;
    for (int pos : {1, 3, 5, 7}) {
        val = clearBit(val, pos);
        std::cout << "  After clearBit(" << pos << "): " << std::bitset<8>(val)
                  << std::endl;
    }

    std::cout << "\n--- toggleBit ---" << std::endl;
    val = 0b11110000;
    std::cout << "  Starting with: " << std::bitset<8>(val) << std::endl;
    for (int pos = 0; pos < 8; ++pos) {
        val = toggleBit(val, pos);
        std::cout << "  After toggleBit(" << pos << "): " << std::bitset<8>(val)
                  << std::endl;
    }
}

// ============================================
// 7. Bit Searching
// ============================================
void demonstrateBitSearching() {
    printSection("7. Bit Searching");

    std::cout << "--- findFirstSetBit ---" << std::endl;
    std::cout << "Finds the position of the least significant set bit"
              << std::endl;

    std::vector<uint8_t> values = {0b00000001, 0b00000010, 0b00001000,
                                    0b10000000, 0b01010100, 0b00000000};

    for (auto val : values) {
        int pos = findFirstSetBit(val);
        std::cout << "  " << std::bitset<8>(val) << " -> ";
        if (pos >= 0) {
            std::cout << "position " << pos << std::endl;
        } else {
            std::cout << "no bits set" << std::endl;
        }
    }

    std::cout << "\n--- findLastSetBit ---" << std::endl;
    std::cout << "Finds the position of the most significant set bit"
              << std::endl;

    for (auto val : values) {
        int pos = findLastSetBit(val);
        std::cout << "  " << std::bitset<8>(val) << " -> ";
        if (pos >= 0) {
            std::cout << "position " << pos << std::endl;
        } else {
            std::cout << "no bits set" << std::endl;
        }
    }
}

// ============================================
// 8. Parallel Bit Operations
// ============================================
void demonstrateParallelBitOps() {
    printSection("8. Parallel Bit Operations");

    std::cout << "--- parallelBitOp ---" << std::endl;
    std::cout << "Applies a bit operation to all elements in parallel"
              << std::endl;

    std::vector<uint8_t> input = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

    std::cout << "\nInput values:" << std::endl;
    for (size_t i = 0; i < input.size(); ++i) {
        std::cout << "  [" << i << "] " << std::bitset<8>(input[i]) << std::endl;
    }

    // Apply NOT operation
    std::cout << "\n--- NOT operation ---" << std::endl;
    auto notResult = parallelBitOp<uint8_t>(
        std::span<const uint8_t>(input), [](uint8_t x) { return ~x; });

    for (size_t i = 0; i < notResult.size(); ++i) {
        std::cout << "  [" << i << "] " << std::bitset<8>(input[i]) << " -> "
                  << std::bitset<8>(notResult[i]) << std::endl;
    }

    // Apply left shift
    std::cout << "\n--- Left shift by 1 ---" << std::endl;
    auto shiftResult = parallelBitOp<uint8_t>(
        std::span<const uint8_t>(input),
        [](uint8_t x) { return static_cast<uint8_t>(x << 1); });

    for (size_t i = 0; i < shiftResult.size(); ++i) {
        std::cout << "  [" << i << "] " << std::bitset<8>(input[i]) << " -> "
                  << std::bitset<8>(shiftResult[i]) << std::endl;
    }

    // Apply popcount
    std::cout << "\n--- Popcount (count set bits) ---" << std::endl;
    std::vector<uint8_t> mixedInput = {0x00, 0xFF, 0xAA, 0x55, 0x0F, 0xF0};
    auto countResult = parallelBitOp<uint8_t>(
        std::span<const uint8_t>(mixedInput),
        [](uint8_t x) { return static_cast<uint8_t>(std::popcount(x)); });

    for (size_t i = 0; i < countResult.size(); ++i) {
        std::cout << "  " << std::bitset<8>(mixedInput[i]) << " has "
                  << static_cast<int>(countResult[i]) << " bits set"
                  << std::endl;
    }
}

// ============================================
// 9. Complex Use Cases
// ============================================
void demonstrateComplexUseCases() {
    printSection("9. Complex Use Cases");

    // Use case 1: Extracting bit fields
    std::cout << "--- Extracting Bit Fields ---" << std::endl;
    uint16_t packedData = 0b1010011100001111;
    std::cout << "Packed data: " << std::bitset<16>(packedData) << std::endl;

    // Extract bits 0-3 (field A)
    uint16_t maskA = createMask<uint16_t>(4);
    uint16_t fieldA = packedData & maskA;
    std::cout << "  Field A (bits 0-3): " << std::bitset<4>(fieldA) << " = "
              << fieldA << std::endl;

    // Extract bits 4-7 (field B)
    uint16_t fieldB = (packedData >> 4) & maskA;
    std::cout << "  Field B (bits 4-7): " << std::bitset<4>(fieldB) << " = "
              << fieldB << std::endl;

    // Extract bits 8-11 (field C)
    uint16_t fieldC = (packedData >> 8) & maskA;
    std::cout << "  Field C (bits 8-11): " << std::bitset<4>(fieldC) << " = "
              << fieldC << std::endl;

    // Extract bits 12-15 (field D)
    uint16_t fieldD = (packedData >> 12) & maskA;
    std::cout << "  Field D (bits 12-15): " << std::bitset<4>(fieldD) << " = "
              << fieldD << std::endl;

    // Use case 2: Permission flags
    std::cout << "\n--- Permission Flags ---" << std::endl;
    enum Permission : uint8_t {
        READ = 0,
        WRITE = 1,
        EXECUTE = 2,
        DELETE = 3,
        ADMIN = 4
    };

    uint8_t userPerms = 0;
    std::cout << "Initial permissions: " << std::bitset<8>(userPerms)
              << std::endl;

    // Grant permissions
    userPerms = setBit(userPerms, READ);
    userPerms = setBit(userPerms, WRITE);
    userPerms = setBit(userPerms, EXECUTE);
    std::cout << "After granting R/W/X: " << std::bitset<8>(userPerms)
              << std::endl;

    // Check permissions
    std::cout << "  Can read: " << (isBitSet(userPerms, READ) ? "Yes" : "No")
              << std::endl;
    std::cout << "  Can write: " << (isBitSet(userPerms, WRITE) ? "Yes" : "No")
              << std::endl;
    std::cout << "  Can execute: "
              << (isBitSet(userPerms, EXECUTE) ? "Yes" : "No") << std::endl;
    std::cout << "  Can delete: "
              << (isBitSet(userPerms, DELETE) ? "Yes" : "No") << std::endl;
    std::cout << "  Is admin: " << (isBitSet(userPerms, ADMIN) ? "Yes" : "No")
              << std::endl;

    // Revoke write permission
    userPerms = clearBit(userPerms, WRITE);
    std::cout << "After revoking write: " << std::bitset<8>(userPerms)
              << std::endl;

    // Use case 3: Checksum calculation
    std::cout << "\n--- Simple Checksum ---" << std::endl;
    std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78, 0x9A};
    uint8_t checksum = 0;

    std::cout << "Data bytes:" << std::endl;
    for (auto byte : data) {
        std::cout << "  0x" << std::hex << static_cast<int>(byte) << std::dec
                  << " (" << std::bitset<8>(byte) << ")" << std::endl;
        checksum ^= byte;
    }
    std::cout << "XOR checksum: 0x" << std::hex << static_cast<int>(checksum)
              << std::dec << " (" << std::bitset<8>(checksum) << ")"
              << std::endl;
    std::cout << "Bit count in checksum: " << countBytes(checksum) << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Bit Manipulation Examples" << std::endl;
    std::cout << "  atom::utils::bit" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateMaskCreation();
        demonstrateBitCounting();
        demonstrateBitReversal();
        demonstrateBitRotation();
        demonstrateMaskOperations();
        demonstrateIndividualBitOps();
        demonstrateBitSearching();
        demonstrateParallelBitOps();
        demonstrateComplexUseCases();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All bit manipulation examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
