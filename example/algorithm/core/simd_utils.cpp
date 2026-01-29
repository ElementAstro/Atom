/*
 * simd_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating SIMD utilities from atom/algorithm/core/simd_utils.hpp
 */

#include "atom/algorithm/core/simd_utils.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using namespace atom::algorithm;
using namespace atom::algorithm::simd;

// Helper function to generate random float vector
std::vector<f32> generateRandomFloatVector(usize size, f32 min_val = 0.0f,
                                           f32 max_val = 100.0f) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<f32> dis(min_val, max_val);

    std::vector<f32> vec(size);
    for (auto& v : vec) {
        v = dis(gen);
    }
    return vec;
}

// Helper function to generate random double vector
std::vector<f64> generateRandomDoubleVector(usize size, f64 min_val = 0.0,
                                            f64 max_val = 100.0) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<f64> dis(min_val, max_val);

    std::vector<f64> vec(size);
    for (auto& v : vec) {
        v = dis(gen);
    }
    return vec;
}

// Demonstrate SIMD capabilities detection
void demonstrateSIMDCapabilities() {
    std::cout << "\n=== SIMD Capabilities Detection ===\n";

    std::cout << "SSE2 available: " << std::boolalpha
              << SIMDCapabilities::hasSSE2() << "\n";
    std::cout << "AVX2 available: " << std::boolalpha
              << SIMDCapabilities::hasAVX2() << "\n";
    std::cout << "AVX512 available: " << std::boolalpha
              << SIMDCapabilities::hasAVX512() << "\n";
    std::cout << "NEON available: " << std::boolalpha
              << SIMDCapabilities::hasNEON() << "\n";

    std::cout << "\nOptimal vector width for f32: "
              << getOptimalVectorWidth<f32>() << " elements\n";
    std::cout << "Optimal vector width for f64: "
              << getOptimalVectorWidth<f64>() << " elements\n";
}

// Demonstrate SIMD memory operations
void demonstrateMemoryOps() {
    std::cout << "\n=== SIMD Memory Operations ===\n";

    constexpr usize SIZE = 1024;

    // Allocate aligned memory
    alignas(32) u8 src[SIZE];
    alignas(32) u8 dst[SIZE];

    // Initialize source
    for (usize i = 0; i < SIZE; ++i) {
        src[i] = static_cast<u8>(i % 256);
    }

    // SIMD copy
    auto start = std::chrono::high_resolution_clock::now();
    MemoryOps::copy(dst, src, SIZE);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "SIMD copy of " << SIZE << " bytes took " << duration.count()
              << " ns\n";

    // Verify copy
    bool copy_ok = true;
    for (usize i = 0; i < SIZE; ++i) {
        if (dst[i] != src[i]) {
            copy_ok = false;
            break;
        }
    }
    std::cout << "Copy verification: " << (copy_ok ? "PASSED" : "FAILED")
              << "\n";

    // SIMD memset
    alignas(32) u8 buffer[SIZE];
    start = std::chrono::high_resolution_clock::now();
    MemoryOps::set(buffer, 0xAB, SIZE);
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "SIMD memset of " << SIZE << " bytes took " << duration.count()
              << " ns\n";

    // Verify memset
    bool set_ok = true;
    for (usize i = 0; i < SIZE; ++i) {
        if (buffer[i] != 0xAB) {
            set_ok = false;
            break;
        }
    }
    std::cout << "Memset verification: " << (set_ok ? "PASSED" : "FAILED")
              << "\n";
}

// Demonstrate SIMD vector addition
void demonstrateVectorAddition() {
    std::cout << "\n=== SIMD Vector Addition ===\n";

    constexpr usize SIZE = 10000;

    // Float vectors
    auto a_f32 = generateRandomFloatVector(SIZE);
    auto b_f32 = generateRandomFloatVector(SIZE);
    std::vector<f32> result_f32(SIZE);

    auto start = std::chrono::high_resolution_clock::now();
    MathOps::vectorAdd(a_f32.data(), b_f32.data(), result_f32.data(), SIZE);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "SIMD f32 vector addition (" << SIZE
              << " elements): " << duration.count() << " us\n";

    // Verify result
    bool verify_ok = true;
    for (usize i = 0; i < SIZE; ++i) {
        f32 expected = a_f32[i] + b_f32[i];
        if (std::abs(result_f32[i] - expected) > 1e-5f) {
            verify_ok = false;
            break;
        }
    }
    std::cout << "f32 addition verification: "
              << (verify_ok ? "PASSED" : "FAILED") << "\n";

    // Double vectors
    auto a_f64 = generateRandomDoubleVector(SIZE);
    auto b_f64 = generateRandomDoubleVector(SIZE);
    std::vector<f64> result_f64(SIZE);

    start = std::chrono::high_resolution_clock::now();
    MathOps::vectorAdd(a_f64.data(), b_f64.data(), result_f64.data(), SIZE);
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "SIMD f64 vector addition (" << SIZE
              << " elements): " << duration.count() << " us\n";

    // Verify result
    verify_ok = true;
    for (usize i = 0; i < SIZE; ++i) {
        f64 expected = a_f64[i] + b_f64[i];
        if (std::abs(result_f64[i] - expected) > 1e-10) {
            verify_ok = false;
            break;
        }
    }
    std::cout << "f64 addition verification: "
              << (verify_ok ? "PASSED" : "FAILED") << "\n";
}

// Demonstrate SIMD dot product
void demonstrateDotProduct() {
    std::cout << "\n=== SIMD Dot Product ===\n";

    constexpr usize SIZE = 10000;

    // Float dot product
    auto a_f32 = generateRandomFloatVector(SIZE, -10.0f, 10.0f);
    auto b_f32 = generateRandomFloatVector(SIZE, -10.0f, 10.0f);

    auto start = std::chrono::high_resolution_clock::now();
    f32 result_f32 = MathOps::dotProduct(a_f32.data(), b_f32.data(), SIZE);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "SIMD f32 dot product (" << SIZE
              << " elements): " << duration.count() << " us\n";
    std::cout << "Result: " << std::fixed << std::setprecision(4) << result_f32
              << "\n";

    // Verify with scalar computation
    f32 expected_f32 = 0.0f;
    for (usize i = 0; i < SIZE; ++i) {
        expected_f32 += a_f32[i] * b_f32[i];
    }
    f32 rel_error_f32 = std::abs(result_f32 - expected_f32) /
                        std::max(std::abs(expected_f32), 1e-10f);
    std::cout << "Relative error: " << std::scientific << rel_error_f32 << "\n";

    // Double dot product
    auto a_f64 = generateRandomDoubleVector(SIZE, -10.0, 10.0);
    auto b_f64 = generateRandomDoubleVector(SIZE, -10.0, 10.0);

    start = std::chrono::high_resolution_clock::now();
    f64 result_f64 = MathOps::dotProduct(a_f64.data(), b_f64.data(), SIZE);
    end = std::chrono::high_resolution_clock::now();

    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "\nSIMD f64 dot product (" << SIZE
              << " elements): " << duration.count() << " us\n";
    std::cout << "Result: " << std::fixed << std::setprecision(8) << result_f64
              << "\n";

    // Verify with scalar computation
    f64 expected_f64 = 0.0;
    for (usize i = 0; i < SIZE; ++i) {
        expected_f64 += a_f64[i] * b_f64[i];
    }
    f64 rel_error_f64 = std::abs(result_f64 - expected_f64) /
                        std::max(std::abs(expected_f64), 1e-15);
    std::cout << "Relative error: " << std::scientific << rel_error_f64 << "\n";
}

// Benchmark SIMD vs scalar operations
void benchmarkSIMDvsScalar() {
    std::cout << "\n=== SIMD vs Scalar Benchmark ===\n";

    constexpr usize SIZE = 100000;
    constexpr int ITERATIONS = 100;

    auto a = generateRandomFloatVector(SIZE);
    auto b = generateRandomFloatVector(SIZE);
    std::vector<f32> result(SIZE);

    // SIMD benchmark
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < ITERATIONS; ++iter) {
        MathOps::vectorAdd(a.data(), b.data(), result.data(), SIZE);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto simd_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Scalar benchmark
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < ITERATIONS; ++iter) {
        for (usize i = 0; i < SIZE; ++i) {
            result[i] = a[i] + b[i];
        }
    }
    end = std::chrono::high_resolution_clock::now();
    auto scalar_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Vector size: " << SIZE << ", Iterations: " << ITERATIONS
              << "\n";
    std::cout << "SIMD total time: " << simd_duration.count() << " us\n";
    std::cout << "Scalar total time: " << scalar_duration.count() << " us\n";
    std::cout << "Speedup: " << std::fixed << std::setprecision(2)
              << static_cast<f64>(scalar_duration.count()) /
                     simd_duration.count()
              << "x\n";
}

// Demonstrate vector width constants
void demonstrateVectorWidthConstants() {
    std::cout << "\n=== Vector Width Constants ===\n";

    std::cout << "AVX512 f32 width: " << VectorWidth::AVX512_F32
              << " elements (" << VectorWidth::AVX512_F32 * sizeof(f32) * 8
              << " bits)\n";
    std::cout << "AVX512 f64 width: " << VectorWidth::AVX512_F64
              << " elements (" << VectorWidth::AVX512_F64 * sizeof(f64) * 8
              << " bits)\n";
    std::cout << "AVX2 f32 width: " << VectorWidth::AVX2_F32 << " elements ("
              << VectorWidth::AVX2_F32 * sizeof(f32) * 8 << " bits)\n";
    std::cout << "AVX2 f64 width: " << VectorWidth::AVX2_F64 << " elements ("
              << VectorWidth::AVX2_F64 * sizeof(f64) * 8 << " bits)\n";
    std::cout << "SSE f32 width: " << VectorWidth::SSE_F32 << " elements ("
              << VectorWidth::SSE_F32 * sizeof(f32) * 8 << " bits)\n";
    std::cout << "SSE f64 width: " << VectorWidth::SSE_F64 << " elements ("
              << VectorWidth::SSE_F64 * sizeof(f64) * 8 << " bits)\n";
    std::cout << "NEON f32 width: " << VectorWidth::NEON_F32 << " elements ("
              << VectorWidth::NEON_F32 * sizeof(f32) * 8 << " bits)\n";
    std::cout << "NEON f64 width: " << VectorWidth::NEON_F64 << " elements ("
              << VectorWidth::NEON_F64 * sizeof(f64) * 8 << " bits)\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   SIMD Utilities Example\n";
    std::cout << "========================================\n";

    try {
        demonstrateSIMDCapabilities();
        demonstrateVectorWidthConstants();
        demonstrateMemoryOps();
        demonstrateVectorAddition();
        demonstrateDotProduct();
        benchmarkSIMDvsScalar();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
