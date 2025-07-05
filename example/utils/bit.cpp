/**
 * @file bit_example.cpp
 * @brief Comprehensive examples demonstrating the bit manipulation utilities
 *
 * This example demonstrates all functions available in atom::utils::bit.hpp:
 * - Basic bit operations (create masks, count bits, etc.)
 * - Bit rotation and reversing
 * - Bit manipulation (set, clear, toggle, check bits)
 * - Mask operations (merge, split)
 * - SIMD-accelerated bit operations when available
 * - Parallel bit operations for large datasets
 */

#include "atom/utils/bit.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Helper function to display binary representation of an integer
template <typename T>
std::string toBinaryString(T value) {
    constexpr int bits = std::numeric_limits<T>::digits;
    std::string result(bits, '0');

    for (int i = 0; i < bits; ++i) {
        if ((value >> i) & 1) {
            result[bits - 1 - i] = '1';
        }
    }

    return result;
}

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==============================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "===============================================" << std::endl;
}

// Helper function to print binary representations with labels
template <typename T>
void printBinary(const std::string& label, T value) {
    std::cout << std::left << std::setw(20) << label << ": ";
    std::cout << "0b" << toBinaryString(value) << " (";
    std::cout << std::dec << static_cast<uint64_t>(value) << ")" << std::endl;
}

// Helper function to time function execution
template <typename Func>
auto measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    auto result = func();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "Execution time: " << duration.count() << " ms" << std::endl;

    return result;
}

// Main function showcasing the bit utilities
int main() {
    try {
        std::cout << "Atom Bit Manipulation Utilities Demo" << std::endl;

        // ===================================================
        // Example 1: Basic Bit Mask Creation and Bit Counting
        // ===================================================
        printSection("1. Basic Bit Mask Creation and Bit Counting");

        // Create masks with different bit lengths
        uint8_t mask8 = atom::utils::createMask<uint8_t>(3);
        uint16_t mask16 = atom::utils::createMask<uint16_t>(5);
        uint32_t mask32 = atom::utils::createMask<uint32_t>(10);
        uint64_t mask64 = atom::utils::createMask<uint64_t>(20);

        std::cout << "Created bit masks:" << std::endl;
        printBinary("3-bit mask (8-bit)", mask8);
        printBinary("5-bit mask (16-bit)", mask16);
        printBinary("10-bit mask (32-bit)", mask32);
        printBinary("20-bit mask (64-bit)", mask64);

        // Count set bits in various values
        uint8_t value8 = 0b10101010;
        uint16_t value16 = 0b1010101010101010;
        uint32_t value32 = 0xAAAAAAAA;  // 0b10101010... pattern
        uint64_t value64 = 0xAAAAAAAAAAAAAAAAULL;

        std::cout << "\nBit counting demonstration:" << std::endl;
        printBinary("8-bit value", value8);
        std::cout << "Bit count: " << atom::utils::countBytes(value8)
                  << std::endl;

        printBinary("16-bit value", value16);
        std::cout << "Bit count: " << atom::utils::countBytes(value16)
                  << std::endl;

        printBinary("32-bit value (first 8 bits)",
                    static_cast<uint8_t>(value32));
        std::cout << "Bit count: " << atom::utils::countBytes(value32)
                  << std::endl;

        std::cout << "64-bit value bit count: "
                  << atom::utils::countBytes(value64) << std::endl;

        // ===================================================
        // Example 2: Bit Rotation and Bit Reversing
        // ===================================================
        printSection("2. Bit Rotation and Bit Reversing");

        // Bit rotation examples
        uint8_t rotValue = 0b10000001;  // 129 decimal

        std::cout << "Bit rotation demonstration:" << std::endl;
        printBinary("Original value", rotValue);

        // Left rotations
        printBinary("Rotate left by 1", atom::utils::rotateLeft(rotValue, 1));
        printBinary("Rotate left by 2", atom::utils::rotateLeft(rotValue, 2));
        printBinary("Rotate left by 4", atom::utils::rotateLeft(rotValue, 4));
        printBinary("Rotate left by 7", atom::utils::rotateLeft(rotValue, 7));

        // Right rotations
        std::cout << std::endl;
        printBinary("Rotate right by 1", atom::utils::rotateRight(rotValue, 1));
        printBinary("Rotate right by 2", atom::utils::rotateRight(rotValue, 2));
        printBinary("Rotate right by 4", atom::utils::rotateRight(rotValue, 4));
        printBinary("Rotate right by 7", atom::utils::rotateRight(rotValue, 7));

        // Bit reversing examples
        std::cout << "\nBit reversing demonstration:" << std::endl;

        uint8_t revValue8 = 0b00000001;            // 1 decimal
        uint16_t revValue16 = 0b0000000000000001;  // 1 decimal
        uint32_t revValue32 = 0x00000001;          // 1 decimal

        printBinary("Original 8-bit", revValue8);
        printBinary("Reversed 8-bit", atom::utils::reverseBits(revValue8));

        printBinary("Original 16-bit", revValue16);
        printBinary("Reversed 16-bit", atom::utils::reverseBits(revValue16));

        std::cout << "Original 32-bit: 0x" << std::hex << revValue32 << std::dec
                  << std::endl;
        std::cout << "Reversed 32-bit: 0x" << std::hex
                  << atom::utils::reverseBits(revValue32) << std::dec
                  << std::endl;

        // More complex bit reversal example
        uint8_t complexPattern = 0b10101100;  // 172 decimal
        printBinary("\nComplex pattern", complexPattern);
        printBinary("Reversed pattern",
                    atom::utils::reverseBits(complexPattern));

        // ===================================================
        // Example 3: Bit Manipulation Operations
        // ===================================================
        printSection("3. Bit Manipulation Operations");

        uint8_t bitValue = 0b00100010;  // 34 decimal

        std::cout << "Initial value:" << std::endl;
        printBinary("Value", bitValue);

        std::cout << "\nChecking if bits are set:" << std::endl;
        for (int i = 0; i < 8; ++i) {
            std::cout << "Bit " << i << " is "
                      << (atom::utils::isBitSet(bitValue, i) ? "set"
                                                             : "not set")
                      << std::endl;
        }

        std::cout << "\nSetting individual bits:" << std::endl;
        printBinary("Set bit 0", atom::utils::setBit(bitValue, 0));
        printBinary("Set bit 3", atom::utils::setBit(bitValue, 3));
        printBinary("Set bit 7", atom::utils::setBit(bitValue, 7));

        std::cout << "\nClearing individual bits:" << std::endl;
        printBinary("Clear bit 1", atom::utils::clearBit(bitValue, 1));
        printBinary("Clear bit 5", atom::utils::clearBit(bitValue, 5));
        printBinary("Clear bit 6", atom::utils::clearBit(bitValue, 6));

        std::cout << "\nToggling individual bits:" << std::endl;
        printBinary("Toggle bit 1", atom::utils::toggleBit(bitValue, 1));
        printBinary("Toggle bit 5", atom::utils::toggleBit(bitValue, 5));
        printBinary("Toggle bit 6", atom::utils::toggleBit(bitValue, 6));

        // ===================================================
        // Example 4: Mask Operations (Merge and Split)
        // ===================================================
        printSection("4. Mask Operations (Merge and Split)");

        uint16_t mask1 = 0b0000000011110000;  // 240 decimal
        uint16_t mask2 = 0b0000111100000000;  // 3840 decimal

        std::cout << "Mask merging demonstration:" << std::endl;
        printBinary("Mask 1", mask1);
        printBinary("Mask 2", mask2);
        printBinary("Merged mask", atom::utils::mergeMasks(mask1, mask2));

        std::cout << "\nMask splitting demonstration:" << std::endl;
        uint16_t complexMask = 0b0101010111110000;  // 21872 decimal
        printBinary("Complex mask", complexMask);

        // Split at different positions
        auto [lower4, upper4] = atom::utils::splitMask(complexMask, 4);
        auto [lower8, upper8] = atom::utils::splitMask(complexMask, 8);
        auto [lower12, upper12] = atom::utils::splitMask(complexMask, 12);

        std::cout << "Split at position 4:" << std::endl;
        printBinary("  Lower part", lower4);
        printBinary("  Upper part", upper4);

        std::cout << "Split at position 8:" << std::endl;
        printBinary("  Lower part", lower8);
        printBinary("  Upper part", upper8);

        std::cout << "Split at position 12:" << std::endl;
        printBinary("  Lower part", lower12);
        printBinary("  Upper part", upper12);

        // ===================================================
        // Example 5: Finding First and Last Set Bits
        // ===================================================
        printSection("5. Finding First and Last Set Bits");

        std::vector<uint32_t> testValues = {
            0b00000000000000000000000000000001,  // First bit set
            0b00000000000000000000000000010010,  // Bits 1 and 4 set
            0b01000000000000000000000000000000,  // Bit 30 set
            0b10000000000000000000000000000000,  // Bit 31 set
            0b10100000000000000000000000010010,  // Multiple bits set
            0                                    // No bits set
        };

        for (size_t i = 0; i < testValues.size(); ++i) {
            std::cout << "Value " << (i + 1) << ": " << std::endl;
            if (testValues[i] == 0) {
                std::cout << "  0 (no bits set)" << std::endl;
            } else {
                std::cout << "  0b" << toBinaryString(testValues[i])
                          << std::endl;
            }

            int firstBit = atom::utils::findFirstSetBit(testValues[i]);
            int lastBit = atom::utils::findLastSetBit(testValues[i]);

            std::cout << "  First set bit: "
                      << (firstBit == -1 ? "none" : std::to_string(firstBit))
                      << std::endl;
            std::cout << "  Last set bit: "
                      << (lastBit == -1 ? "none" : std::to_string(lastBit))
                      << std::endl;
            std::cout << std::endl;
        }

// ===================================================
// Example 6: SIMD-Accelerated Bit Counting
// ===================================================
#ifdef ATOM_SIMD_SUPPORT
        printSection("6. SIMD-Accelerated Bit Counting");

        // Generate random data for bit counting
        const size_t dataSize = 100000;
        std::vector<uint8_t> randomData(dataSize);

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, 255);

        for (size_t i = 0; i < dataSize; ++i) {
            randomData[i] = static_cast<uint8_t>(dist(gen));
        }

        std::cout << "Counting bits in " << dataSize << " random bytes..."
                  << std::endl;

        // Sequential counting for comparison
        uint64_t seqCount = 0;
        auto seqTime = measureTime([&]() {
            for (size_t i = 0; i < dataSize; ++i) {
                seqCount += std::popcount(randomData[i]);
            }
            return seqCount;
        });

        std::cout << "Sequential count result: " << seqCount << std::endl;

        // Parallel counting with SIMD
        uint64_t parallelCount = 0;
        auto parallelTime = measureTime([&]() {
            parallelCount = atom::utils::countBitsParallel(randomData.data(),
                                                           randomData.size());
            return parallelCount;
        });

        std::cout << "Parallel count result: " << parallelCount << std::endl;

        if (seqCount != parallelCount) {
            std::cout << "ERROR: Count results don't match!" << std::endl;
        }
#endif

        // ===================================================
        // Example 7: Parallel Bit Operations
        // ===================================================
        printSection("7. Parallel Bit Operations");

        // Generate a large array of values for parallel processing
        const size_t arraySize = 100000;
        std::vector<uint32_t> largeArray(arraySize);

        std::mt19937 gen2(42);  // Fixed seed for reproducibility
        std::uniform_int_distribution<uint32_t> dist2(0, UINT32_MAX);

        for (size_t i = 0; i < arraySize; ++i) {
            largeArray[i] = dist2(gen2);
        }

        std::cout << "Performing bit operations on " << arraySize
                  << " values..." << std::endl;

        // Example 1: Count leading zeros in parallel
        std::cout << "\n1. Counting leading zeros:" << std::endl;
        auto leadingZeros = measureTime([&]() {
            return atom::utils::parallelBitOperation(
                largeArray.begin(), largeArray.end(),
                [](uint32_t val) { return std::countl_zero(val); });
        });

        // Sample output
        std::cout << "Sample results (first 5 elements):" << std::endl;
        for (size_t i = 0; i < 5 && i < leadingZeros.size(); ++i) {
            std::cout << "  Value: 0x" << std::hex << largeArray[i]
                      << ", Leading zeros: " << std::dec << leadingZeros[i]
                      << std::endl;
        }

        // Example 2: Reversing bits in parallel
        std::cout << "\n2. Reversing bits:" << std::endl;
        auto reversedBits = measureTime([&]() {
            return atom::utils::parallelBitOperation(
                largeArray.begin(), largeArray.end(),
                [](uint32_t val) { return atom::utils::reverseBits(val); });
        });

        // Sample output
        std::cout << "Sample results (first 5 elements):" << std::endl;
        for (size_t i = 0; i < 5 && i < reversedBits.size(); ++i) {
            std::cout << "  Original: 0x" << std::hex << largeArray[i]
                      << ", Reversed: 0x" << reversedBits[i] << std::dec
                      << std::endl;
        }

        // Example 3: Creating masks with specified lengths
        std::cout << "\n3. Creating masks with specific lengths:" << std::endl;
        auto masks = measureTime([&]() {
            return atom::utils::parallelBitOperation(
                largeArray.begin(), largeArray.end(), [](uint32_t val) {
                    // Use the value mod 32 as the mask length
                    return atom::utils::createMask<uint32_t>(val % 32);
                });
        });

        // Sample output
        std::cout << "Sample results (first 5 elements):" << std::endl;
        for (size_t i = 0; i < 5 && i < masks.size(); ++i) {
            std::cout << "  Value mod 32: " << (largeArray[i] % 32)
                      << ", Mask: 0x" << std::hex << masks[i] << std::dec
                      << std::endl;
        }

        // ===================================================
        // Example 8: Error Handling
        // ===================================================
        printSection("8. Error Handling");

        std::cout << "Demonstrating error handling:" << std::endl;

        // Try to create mask with negative bits
        try {
            std::cout << "Attempting to create mask with negative bits..."
                      << std::endl;
            [[maybe_unused]] auto invalidMask =
                atom::utils::createMask<uint32_t>(-5);
        } catch (const atom::utils::BitManipulationException& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Try to rotate with negative shift
        try {
            std::cout << "\nAttempting left rotation with negative shift..."
                      << std::endl;
            [[maybe_unused]] auto invalidRotation =
                atom::utils::rotateLeft<uint32_t>(0x12345678, -3);
        } catch (const atom::utils::BitManipulationException& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Try to access bit position out of range
        try {
            std::cout << "\nAttempting to access bit position out of range..."
                      << std::endl;
            [[maybe_unused]] auto invalidBitCheck =
                atom::utils::isBitSet<uint8_t>(0x42, 8);
        } catch (const atom::utils::BitManipulationException& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        // Try to split mask at invalid position
        try {
            std::cout << "\nAttempting to split mask at invalid position..."
                      << std::endl;
            [[maybe_unused]] auto invalidSplit =
                atom::utils::splitMask<uint16_t>(0xFFFF, -1);
        } catch (const atom::utils::BitManipulationException& e) {
            std::cout << "Caught exception: " << e.what() << std::endl;
        }

        std::cout << "\nAll examples completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "ERROR: Unhandled exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
