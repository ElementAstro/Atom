/**
 * @file simd_wrapper_example.cpp
 * @brief Comprehensive examples demonstrating SIMD wrapper utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::memory/simd_wrapper.hpp:
 * - Basic SIMD vector operations (load, store, arithmetic)
 * - Cross-platform SIMD support (SSE, AVX, NEON)
 * - Vector arithmetic operations (add, subtract, multiply, divide)
 * - Comparison and logical operations
 * - Horizontal operations (sum, min, max)
 * - Performance comparisons with scalar operations
 * - Real-world use cases (dot product, matrix operations)
 */

#include "atom/utils/memory/simd_wrapper.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to print vector contents
template <typename T>
void printVector(const std::vector<T>& vec, const std::string& label,
                 size_t maxElements = 8) {
    std::cout << label << ": [";
    size_t count = std::min(vec.size(), maxElements);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::fixed << std::setprecision(2) << vec[i];
        if (i < count - 1)
            std::cout << ", ";
    }
    if (vec.size() > maxElements) {
        std::cout << "...";
    }
    std::cout << "]" << std::endl;
}

// Generate random data for testing
template <typename T>
std::vector<T> generateRandomData(size_t size, T minVal = T(0),
                                  T maxVal = T(100)) {
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<T> data;
    data.reserve(size);

    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dis(minVal, maxVal);
        for (size_t i = 0; i < size; ++i) {
            data.push_back(dis(gen));
        }
    } else {
        std::uniform_int_distribution<T> dis(minVal, maxVal);
        for (size_t i = 0; i < size; ++i) {
            data.push_back(dis(gen));
        }
    }

    return data;
}

// Scalar dot product for comparison
template <typename T>
T scalarDotProduct(const std::vector<T>& a, const std::vector<T>& b) {
    T result = T(0);
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

// SIMD dot product implementation
template <typename T>
T simdDotProduct(const std::vector<T>& a, const std::vector<T>& b) {
    constexpr size_t vecSize = Vec<T>::size();
    T result = T(0);

    size_t i = 0;
    // Process SIMD-sized chunks
    for (; i + vecSize <= a.size(); i += vecSize) {
        Vec<T> va = Vec<T>::loadu(&a[i]);
        Vec<T> vb = Vec<T>::loadu(&b[i]);
        Vec<T> product = va * vb;
        result += product.horizontal_sum();
    }

    // Handle remaining elements
    for (; i < a.size(); ++i) {
        result += a[i] * b[i];
    }

    return result;
}

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  SIMD Wrapper Utilities Demo" << std::endl;
    std::cout << "==========================================" << std::endl;

    // ============================
    // Example 1: Basic SIMD Operations
    // ============================
    printSection("1. Basic SIMD Operations");

    printSubsection("Float Vector Operations");

    // Create test data
    constexpr size_t testSize = 16;
    auto floatData1 = generateRandomData<float>(testSize, 1.0f, 10.0f);
    auto floatData2 = generateRandomData<float>(testSize, 1.0f, 10.0f);

    printVector(floatData1, "Vector A", 8);
    printVector(floatData2, "Vector B", 8);

    // Demonstrate basic SIMD operations
    constexpr size_t vecSize = Vec<float>::size();
    std::cout << "SIMD vector size: " << vecSize << " floats" << std::endl;

    // Load vectors
    Vec<float> va = Vec<float>::loadu(floatData1.data());
    Vec<float> vb = Vec<float>::loadu(floatData2.data());

    // Arithmetic operations
    Vec<float> sum = va + vb;
    Vec<float> diff = va - vb;
    Vec<float> product = va * vb;
    Vec<float> quotient = va / vb;

    // Store results
    std::vector<float> sumResult(vecSize), diffResult(vecSize);
    std::vector<float> productResult(vecSize), quotientResult(vecSize);

    sum.storeu(sumResult.data());
    diff.storeu(diffResult.data());
    product.storeu(productResult.data());
    quotient.storeu(quotientResult.data());

    printVector(sumResult, "A + B");
    printVector(diffResult, "A - B");
    printVector(productResult, "A * B");
    printVector(quotientResult, "A / B");

    printSubsection("Integer Vector Operations");

    auto intData1 = generateRandomData<int>(testSize, 1, 100);
    auto intData2 = generateRandomData<int>(testSize, 1, 50);

    printVector(intData1, "Int Vector A", 8);
    printVector(intData2, "Int Vector B", 8);

    Vec<int> via = Vec<int>::loadu(intData1.data());
    Vec<int> vib = Vec<int>::loadu(intData2.data());

    Vec<int> isum = via + vib;
    Vec<int> idiff = via - vib;
    Vec<int> iproduct = via * vib;

    std::vector<int> isumResult(Vec<int>::size());
    std::vector<int> idiffResult(Vec<int>::size());
    std::vector<int> iproductResult(Vec<int>::size());

    isum.storeu(isumResult.data());
    idiff.storeu(idiffResult.data());
    iproduct.storeu(iproductResult.data());

    printVector(isumResult, "Int A + B");
    printVector(idiffResult, "Int A - B");
    printVector(iproductResult, "Int A * B");

    // ============================
    // Example 2: Horizontal Operations
    // ============================
    printSection("2. Horizontal Operations");

    printSubsection("Reduction Operations");

    Vec<float> testVec = Vec<float>::loadu(floatData1.data());

    float horizontalSum = testVec.horizontal_sum();
    float horizontalMax = testVec.horizontal_max();
    float horizontalMin = testVec.horizontal_min();

    std::cout << "Horizontal sum: " << std::fixed << std::setprecision(2)
              << horizontalSum << std::endl;
    std::cout << "Horizontal max: " << horizontalMax << std::endl;
    std::cout << "Horizontal min: " << horizontalMin << std::endl;

    // Verify with scalar operations
    float scalarSum =
        std::accumulate(floatData1.begin(), floatData1.begin() + vecSize, 0.0f);
    float scalarMax =
        *std::max_element(floatData1.begin(), floatData1.begin() + vecSize);
    float scalarMin =
        *std::min_element(floatData1.begin(), floatData1.begin() + vecSize);

    std::cout << "Scalar verification - Sum: " << scalarSum
              << ", Max: " << scalarMax << ", Min: " << scalarMin << std::endl;

    // ============================
    // Example 3: Performance Comparison
    // ============================
    printSection("3. Performance Comparison");

    printSubsection("Dot Product Performance");

    // Create larger datasets for performance testing
    constexpr size_t perfTestSize = 1000000;
    auto perfData1 = generateRandomData<float>(perfTestSize, -1.0f, 1.0f);
    auto perfData2 = generateRandomData<float>(perfTestSize, -1.0f, 1.0f);

    std::cout << "Testing with " << perfTestSize << " elements..." << std::endl;

    // Scalar implementation timing
    auto start = std::chrono::high_resolution_clock::now();
    float scalarResult = scalarDotProduct(perfData1, perfData2);
    auto scalarEnd = std::chrono::high_resolution_clock::now();

    // SIMD implementation timing
    auto simdStart = std::chrono::high_resolution_clock::now();
    float simdResult = simdDotProduct(perfData1, perfData2);
    auto simdEnd = std::chrono::high_resolution_clock::now();

    auto scalarTime =
        std::chrono::duration_cast<std::chrono::microseconds>(scalarEnd - start)
            .count();
    auto simdTime = std::chrono::duration_cast<std::chrono::microseconds>(
                        simdEnd - simdStart)
                        .count();

    std::cout << "Scalar result: " << std::fixed << std::setprecision(6)
              << scalarResult << std::endl;
    std::cout << "SIMD result:   " << simdResult << std::endl;
    std::cout << "Results match: "
              << (std::abs(scalarResult - simdResult) < 1e-3 ? "YES" : "NO")
              << std::endl;
    std::cout << "Scalar time:   " << scalarTime << " μs" << std::endl;
    std::cout << "SIMD time:     " << simdTime << " μs" << std::endl;
    std::cout << "Speedup:       " << std::fixed << std::setprecision(2)
              << (static_cast<double>(scalarTime) / simdTime) << "x"
              << std::endl;

    // ============================
    // Example 4: Vector Utilities
    // ============================
    printSection("4. Vector Utilities");

    printSubsection("Special Vector Creation");

    Vec<float> zeros = Vec<float>::zeros();
    Vec<float> ones = Vec<float>::ones();

    std::vector<float> zerosResult(vecSize), onesResult(vecSize);
    zeros.storeu(zerosResult.data());
    ones.storeu(onesResult.data());

    printVector(zerosResult, "Zeros vector");
    printVector(onesResult, "Ones vector");

    printSubsection("Broadcasting");

    Vec<float> broadcast = Vec<float>(42.0f);
    std::vector<float> broadcastResult(vecSize);
    broadcast.storeu(broadcastResult.data());
    printVector(broadcastResult, "Broadcast 42.0");

    std::cout << "\nAll SIMD wrapper examples completed successfully!"
              << std::endl;

    return 0;
}
