/**
 * @file async_simd_operations.cpp
 * @brief Demonstration of SIMD-optimized buffer operations
 *
 * This example demonstrates:
 * - SIMD buffer comparison (simdBufferCompare)
 * - SIMD byte searching (simdFindByte)
 * - SIMD memory initialization (simdMemorySet)
 * - Performance comparison with standard operations
 * - Edge cases and error handling
 */

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <numeric>
#include <span>
#include <string>
#include <vector>
#include "atom/io/async/async_simd.hpp"

using namespace atom::io::async;

/**
 * @brief Demonstrates SIMD buffer comparison
 */
void demonstrateBufferCompare() {
    std::cout << "\n=== SIMD Buffer Comparison ===" << std::endl;

    // Test 1: Identical buffers
    std::cout << "\n1. Comparing identical buffers..." << std::endl;
    {
        std::vector<char> buf1(1024, '\xAA');
        std::vector<char> buf2(1024, '\xAA');

        std::span<const char> s1(buf1);
        std::span<const char> s2(buf2);
        bool equal = simdBufferCompare(s1, s2);
        std::cout << "  Identical buffers (1024 bytes): "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;
    }

    // Test 2: Different buffers
    std::cout << "\n2. Comparing different buffers..." << std::endl;
    {
        std::vector<char> buf1(1024, '\xAA');
        std::vector<char> buf2(1024, '\xAA');
        buf2[512] = '\xBB';

        std::span<const char> s1(buf1);
        std::span<const char> s2(buf2);
        bool equal = simdBufferCompare(s1, s2);
        std::cout << "  Buffers differ at byte 512: "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;
    }

    // Test 3: Different at the last byte
    std::cout << "\n3. Buffers differ at last byte..." << std::endl;
    {
        std::vector<char> buf1(256, '\x55');
        std::vector<char> buf2(256, '\x55');
        buf2[255] = '\x56';

        std::span<const char> s1(buf1);
        std::span<const char> s2(buf2);
        bool equal = simdBufferCompare(s1, s2);
        std::cout << "  Differ at byte 255: "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;
    }

    // Test 4: Small buffers
    std::cout << "\n4. Small buffer comparison..." << std::endl;
    {
        char buf1[] = {1, 2, 3, 4, 5};
        char buf2[] = {1, 2, 3, 4, 5};

        bool equal = simdBufferCompare(
            std::span<const char>(buf1, 5), std::span<const char>(buf2, 5));
        std::cout << "  5-byte identical buffers: "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;

        buf2[3] = 99;
        equal = simdBufferCompare(
            std::span<const char>(buf1, 5), std::span<const char>(buf2, 5));
        std::cout << "  5-byte different buffers: "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;
    }
}

/**
 * @brief Demonstrates SIMD byte searching
 */
void demonstrateFindByte() {
    std::cout << "\n=== SIMD Byte Search ===" << std::endl;

    // Test 1: Find byte at known position
    std::cout << "\n1. Finding byte at known position..." << std::endl;
    {
        std::vector<char> data(1024, '\x00');
        data[500] = '\xFF';

        auto pos = simdFindByte(std::span<const char>(data), '\xFF');
        std::cout << "  Searching for 0xFF in 1024-byte buffer" << std::endl;
        std::cout << "  Found at position: " << pos
                  << " (expected: 500)" << std::endl;
    }

    // Test 2: Find byte at start
    std::cout << "\n2. Finding byte at start..." << std::endl;
    {
        std::vector<char> data(100, '\x00');
        data[0] = '\x42';

        auto pos = simdFindByte(std::span<const char>(data), '\x42');
        std::cout << "  Found 0x42 at position: " << pos
                  << " (expected: 0)" << std::endl;
    }

    // Test 3: Find byte at end
    std::cout << "\n3. Finding byte at end..." << std::endl;
    {
        std::vector<char> data(100, '\x00');
        data[99] = '\x42';

        auto pos = simdFindByte(std::span<const char>(data), '\x42');
        std::cout << "  Found 0x42 at position: " << pos
                  << " (expected: 99)" << std::endl;
    }

    // Test 4: Byte not found
    std::cout << "\n4. Searching for non-existent byte..." << std::endl;
    {
        std::vector<char> data(256, '\xAA');

        auto pos = simdFindByte(std::span<const char>(data), '\xBB');
        if (pos == std::string::npos) {
            std::cout << "  Byte not found (npos), as expected" << std::endl;
        } else {
            std::cout << "  Found at: " << pos << std::endl;
        }
    }

    // Test 5: Multiple occurrences (finds first)
    std::cout << "\n5. Multiple occurrences (finds first)..." << std::endl;
    {
        std::vector<char> data(100, '\x00');
        data[10] = '\x42';
        data[50] = '\x42';
        data[90] = '\x42';

        auto pos = simdFindByte(std::span<const char>(data), '\x42');
        std::cout << "  First 0x42 found at: " << pos
                  << " (expected: 10)" << std::endl;
    }
}

/**
 * @brief Demonstrates SIMD memory initialization
 */
void demonstrateMemorySet() {
    std::cout << "\n=== SIMD Memory Set ===" << std::endl;

    // Test 1: Fill buffer with a value
    std::cout << "\n1. Filling 4096-byte buffer with 0xCC..." << std::endl;
    {
        std::vector<char> data(4096, '\x00');

        simdMemorySet(std::span<char>(data), '\xCC');

        bool allSet = std::all_of(data.begin(), data.end(),
                                  [](char b) { return b == '\xCC'; });
        std::cout << "  All bytes set correctly: "
                  << (allSet ? "YES" : "NO") << std::endl;
    }

    // Test 2: Fill with zero
    std::cout << "\n2. Zeroing 2048-byte buffer..." << std::endl;
    {
        std::vector<char> data(2048, '\xFF');

        simdMemorySet(std::span<char>(data), '\x00');

        bool allZero = std::all_of(data.begin(), data.end(),
                                   [](char b) { return b == '\x00'; });
        std::cout << "  All bytes zeroed: "
                  << (allZero ? "YES" : "NO") << std::endl;
    }

    // Test 3: Small buffer
    std::cout << "\n3. Filling 7-byte buffer (sub-vector size)..." << std::endl;
    {
        char data[7] = {0};
        simdMemorySet(std::span<char>(data, 7), '\x55');

        bool allSet = true;
        for (int i = 0; i < 7; ++i) {
            if (data[i] != '\x55') {
                allSet = false;
                break;
            }
        }
        std::cout << "  Small buffer filled correctly: "
                  << (allSet ? "YES" : "NO") << std::endl;
    }

    // Test 4: Large buffer
    std::cout << "\n4. Filling 1 MB buffer..." << std::endl;
    {
        const size_t size = 1024 * 1024;
        std::vector<char> data(size, '\x00');

        auto start = std::chrono::high_resolution_clock::now();
        simdMemorySet(std::span<char>(data), '\xAB');
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        bool allSet = std::all_of(data.begin(), data.end(),
                                  [](char b) { return b == '\xAB'; });
        std::cout << "  1 MB filled in " << duration.count() << " us"
                  << std::endl;
        std::cout << "  All bytes correct: "
                  << (allSet ? "YES" : "NO") << std::endl;
    }
}

/**
 * @brief Performance comparison between SIMD and standard operations
 */
void demonstratePerformanceComparison() {
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    const size_t bufferSize = 4 * 1024 * 1024;  // 4 MB
    const int iterations = 10;

    std::vector<char> buf1(bufferSize);
    std::vector<char> buf2(bufferSize);

    // Fill with identical data
    std::iota(buf1.begin(), buf1.end(), 0);
    std::iota(buf2.begin(), buf2.end(), 0);

    // Compare: SIMD vs memcmp
    std::cout << "\n1. Buffer comparison (" << (bufferSize / 1024 / 1024)
              << " MB, " << iterations << " iterations):" << std::endl;
    {
        std::span<const char> s1(buf1);
        std::span<const char> s2(buf2);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            simdBufferCompare(s1, s2);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto simdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            std::memcmp(buf1.data(), buf2.data(), bufferSize);
        }
        end = std::chrono::high_resolution_clock::now();
        auto stdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  SIMD compare: " << simdDuration.count() << " us"
                  << std::endl;
        std::cout << "  std::memcmp:  " << stdDuration.count() << " us"
                  << std::endl;
    }

    // Find byte: SIMD vs memchr
    std::cout << "\n2. Byte search (" << (bufferSize / 1024 / 1024)
              << " MB, " << iterations << " iterations):" << std::endl;
    {
        buf1[bufferSize - 1] = '\xFF';
        std::span<const char> s1(buf1);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            simdFindByte(s1, '\xFF');
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto simdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            std::memchr(buf1.data(), '\xFF', bufferSize);
        }
        end = std::chrono::high_resolution_clock::now();
        auto stdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  SIMD find:   " << simdDuration.count() << " us"
                  << std::endl;
        std::cout << "  std::memchr: " << stdDuration.count() << " us"
                  << std::endl;
    }

    // Memory set: SIMD vs memset
    std::cout << "\n3. Memory set (" << (bufferSize / 1024 / 1024)
              << " MB, " << iterations << " iterations):" << std::endl;
    {
        std::span<char> s1(buf1);
        std::span<char> s2_span(buf2);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            simdMemorySet(s1, '\xAA');
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto simdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            std::memset(buf2.data(), '\xAA', bufferSize);
        }
        end = std::chrono::high_resolution_clock::now();
        auto stdDuration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "  SIMD set:    " << simdDuration.count() << " us"
                  << std::endl;
        std::cout << "  std::memset: " << stdDuration.count() << " us"
                  << std::endl;
    }
}

/**
 * @brief Demonstrates edge cases
 */
void demonstrateEdgeCases() {
    std::cout << "\n=== Edge Cases ===" << std::endl;

    // Zero-length operations
    std::cout << "\n1. Zero-length operations..." << std::endl;
    {
        std::span<const char> empty;
        bool equal = simdBufferCompare(empty, empty);
        std::cout << "  Compare 0 bytes: "
                  << (equal ? "EQUAL" : "DIFFERENT") << std::endl;

        auto pos = simdFindByte(empty, '\x42');
        std::cout << "  Find in 0 bytes: "
                  << (pos == std::string::npos ? "npos" : std::to_string(pos))
                  << std::endl;
    }

    // Single-byte operations
    std::cout << "\n2. Single-byte operations..." << std::endl;
    {
        char a = '\x42', b = '\x42', c = '\x43';

        bool equal1 = simdBufferCompare(
            std::span<const char>(&a, 1), std::span<const char>(&b, 1));
        bool equal2 = simdBufferCompare(
            std::span<const char>(&a, 1), std::span<const char>(&c, 1));
        std::cout << "  Compare same byte: "
                  << (equal1 ? "EQUAL" : "DIFFERENT") << std::endl;
        std::cout << "  Compare diff byte: "
                  << (equal2 ? "EQUAL" : "DIFFERENT") << std::endl;

        auto pos = simdFindByte(std::span<const char>(&a, 1), '\x42');
        std::cout << "  Find in single byte: " << pos << std::endl;
    }

    // Unaligned sizes (not multiple of SIMD register width)
    std::cout << "\n3. Unaligned buffer sizes..." << std::endl;
    {
        std::vector<size_t> sizes = {1, 3, 7, 15, 17, 31, 33, 63, 65};
        for (size_t sz : sizes) {
            std::vector<char> data(sz, '\xBB');
            simdMemorySet(std::span<char>(data), '\xCC');

            bool allSet = std::all_of(data.begin(), data.end(),
                                      [](char b) { return b == '\xCC'; });
            if (!allSet) {
                std::cout << "  Size " << sz << ": FAILED" << std::endl;
            }
        }
        std::cout << "  All unaligned sizes passed" << std::endl;
    }
}

int main() {
    try {
        std::cout << "  Atom I/O SIMD Buffer Operations Examples" << std::endl;
        std::cout << "===========================================" << std::endl;

        demonstrateBufferCompare();
        demonstrateFindByte();
        demonstrateMemorySet();
        demonstratePerformanceComparison();
        demonstrateEdgeCases();

        std::cout
            << "\n  All SIMD operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "  Fatal exception: " << e.what() << std::endl;
        return 1;
    }
}
