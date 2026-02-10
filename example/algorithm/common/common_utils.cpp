/**
 * @file common_utils.cpp
 * @brief Example demonstrating the common algorithm utilities
 *
 * This example shows how to use the common utility modules:
 * - endian.hpp: Byte order conversion utilities
 * - hex.hpp: Hexadecimal string conversion
 * - parallel.hpp: Parallel processing utilities
 * - concepts.hpp: Type constraints for algorithm interfaces
 */

#include <iostream>
#include <numeric>
#include <vector>

#include "atom/algorithm/common/concepts.hpp"
#include "atom/algorithm/common/endian.hpp"
#include "atom/algorithm/common/hex.hpp"
#include "atom/algorithm/common/parallel.hpp"

using namespace atom::algorithm;

void demonstrateEndianUtils() {
    std::cout << "=== Endian Utilities ===" << std::endl;

    // Byte swapping
    u32 value = 0x12345678;
    u32 swapped = endian::byteSwap32(value);
    std::cout << "Original: 0x" << std::hex << value << std::endl;
    std::cout << "Swapped:  0x" << swapped << std::dec << std::endl;

    // Reading big-endian bytes
    std::array<u8, 4> big_endian_bytes = {0x12, 0x34, 0x56, 0x78};
    u32 native_value = endian::readBig32(big_endian_bytes);
    std::cout << "Big-endian bytes [12 34 56 78] -> 0x" << std::hex
              << native_value << std::dec << std::endl;

    // Writing little-endian bytes
    std::array<u8, 4> output_bytes{};
    endian::writeLittle32(0xDEADBEEF, output_bytes);
    std::cout << "0xDEADBEEF as little-endian: ";
    for (u8 b : output_bytes) {
        std::cout << std::hex << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl << std::endl;
}

void demonstrateHexUtils() {
    std::cout << "=== Hex Utilities ===" << std::endl;

    // Convert bytes to hex string
    std::vector<u8> data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    std::string hex_lower = hex::toHexString(data);
    std::string hex_upper = hex::toHexString(data, true);
    std::cout << "Bytes to hex (lowercase): " << hex_lower << std::endl;
    std::cout << "Bytes to hex (uppercase): " << hex_upper << std::endl;

    // Convert hex string back to bytes
    auto bytes = hex::fromHexString("48656c6c6f");  // "Hello"
    std::cout << "Hex '48656c6c6f' to bytes: ";
    for (u8 b : bytes) {
        std::cout << static_cast<char>(b);
    }
    std::cout << std::endl;

    // Convert 32-bit words to hex (useful for hash output)
    std::vector<u32> words = {0x67452301, 0xEFCDAB89};
    std::cout << "Words to hex: " << hex::wordsToHexString(words) << std::endl;
    std::cout << std::endl;
}

void demonstrateParallelUtils() {
    std::cout << "=== Parallel Utilities ===" << std::endl;

    std::cout << "Default thread count: " << parallel::getDefaultThreadCount()
              << std::endl;

    // Parallel sum using parallelMapReduce
    std::vector<int> numbers(100000);
    std::iota(numbers.begin(), numbers.end(), 1);

    auto sum = parallel::parallelMapReduce<int, long long>(
        std::span<const int>(numbers),
        [](std::span<const int> chunk, usize) -> long long {
            long long partial = 0;
            for (int val : chunk) {
                partial += val;
            }
            return partial;
        },
        [](long long a, long long b) { return a + b; }, 0LL, 4);

    std::cout << "Parallel sum of 1..100000: " << sum << std::endl;
    std::cout << "Expected: " << (100000LL * 100001LL / 2) << std::endl;

    // Parallel transform using parallelForEach
    std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8};
    parallel::parallelForEach<int>(
        std::span<int>(data),
        [](std::span<int> chunk, usize) {
            for (int& val : chunk) {
                val *= 2;  // Double each value
            }
        },
        2, 2);

    std::cout << "After parallel doubling: ";
    for (int val : data) {
        std::cout << val << " ";
    }
    std::cout << std::endl << std::endl;
}

void demonstrateConcepts() {
    std::cout << "=== Concepts (Compile-time Type Constraints) ===" << std::endl;

    // These are compile-time checks - the code demonstrates usage patterns

    // StringLike concept
    auto processString = [](const StringLike auto& str) {
        std::cout << "String-like data, size: " << str.size() << std::endl;
    };
    processString(std::string("Hello"));
    processString(std::string_view("World"));

    // ByteContainer concept
    auto processBytes = [](const ByteContainer auto& bytes) {
        std::cout << "Byte container, size: " << bytes.size() << std::endl;
    };
    std::vector<u8> bytes = {0x01, 0x02, 0x03};
    std::array<u8, 4> byte_array = {0x04, 0x05, 0x06, 0x07};
    processBytes(bytes);
    processBytes(byte_array);

    // Numeric concept
    auto square = []<Numeric T>(T val) { return val * val; };
    std::cout << "Square of 5: " << square(5) << std::endl;
    std::cout << "Square of 3.14: " << square(3.14) << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "Atom Algorithm Common Utilities Example\n"
              << "========================================\n"
              << std::endl;

    demonstrateEndianUtils();
    demonstrateHexUtils();
    demonstrateParallelUtils();
    demonstrateConcepts();

    std::cout << "All demonstrations completed successfully!" << std::endl;
    return 0;
}
