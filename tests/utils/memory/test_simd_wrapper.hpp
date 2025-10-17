/*
 * test_simd_wrapper.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Comprehensive tests for SIMD wrapper utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_SIMD_WRAPPER_HPP
#define ATOM_UTILS_TEST_SIMD_WRAPPER_HPP

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <future>
#include <numeric>
#include <random>
#include <thread>
#include <vector>
#include "atom/utils/memory/simd_wrapper.hpp"

namespace atom::utils::test {

template <typename T>
class SIMDWrapperTest : public ::testing::Test {
protected:
    static constexpr size_t N = 4;  // Default vector size
    using Vec = simd::Vec<T, N>;
    using Traits = simd::VecTraits<T, N>;
    using scalar_t = typename Traits::scalar_t;
    using vector_t = typename Traits::vector_t;
    using mask_t = typename Traits::mask_t;

    void SetUp() override {
        // Initialize test data
        for (size_t i = 0; i < N; ++i) {
            testData1[i] = static_cast<T>(i + 1);
            testData2[i] = static_cast<T>((i + 1) * 2);
            testData3[i] = static_cast<T>((i + 1) * 0.5);
        }

        // Initialize random data
        std::random_device rd;
        std::mt19937 gen(rd());
        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> dis(1, 100);
            for (size_t i = 0; i < N; ++i) {
                randomData1[i] = dis(gen);
                randomData2[i] = dis(gen);
            }
        } else {
            std::uniform_real_distribution<T> dis(0.1, 100.0);
            for (size_t i = 0; i < N; ++i) {
                randomData1[i] = dis(gen);
                randomData2[i] = dis(gen);
            }
        }

        // Initialize aligned memory for testing
#ifdef _WIN32
        alignedData1 = static_cast<T*>(_aligned_malloc(N * sizeof(T), 32));
        alignedData2 = static_cast<T*>(_aligned_malloc(N * sizeof(T), 32));
        alignedResult = static_cast<T*>(_aligned_malloc(N * sizeof(T), 32));
#else
        alignedData1 = static_cast<T*>(std::aligned_alloc(32, N * sizeof(T)));
        alignedData2 = static_cast<T*>(std::aligned_alloc(32, N * sizeof(T)));
        alignedResult = static_cast<T*>(std::aligned_alloc(32, N * sizeof(T)));
#endif

        std::copy(testData1.begin(), testData1.end(), alignedData1);
        std::copy(testData2.begin(), testData2.end(), alignedData2);
    }

    void TearDown() override {
#ifdef _WIN32
        _aligned_free(alignedData1);
        _aligned_free(alignedData2);
        _aligned_free(alignedResult);
#else
        std::free(alignedData1);
        std::free(alignedData2);
        std::free(alignedResult);
#endif
    }

    // Helper function to compare arrays with tolerance for floating point
    bool compareArrays(const T* a, const T* b, size_t size,
                       T tolerance = T(1e-6)) {
        for (size_t i = 0; i < size; ++i) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::abs(a[i] - b[i]) > tolerance) {
                    return false;
                }
            } else {
                if (a[i] != b[i]) {
                    return false;
                }
            }
        }
        return true;
    }

    // Test data
    std::array<T, N> testData1;
    std::array<T, N> testData2;
    std::array<T, N> testData3;
    std::array<T, N> randomData1;
    std::array<T, N> randomData2;

    // Aligned memory for testing
    T* alignedData1;
    T* alignedData2;
    T* alignedResult;
};

// Test basic vector construction and access
TYPED_TEST_SUITE_P(SIMDWrapperTest);

TYPED_TEST_P(SIMDWrapperTest, BasicConstruction) {
    using Vec = typename TestFixture::Vec;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    // Test default construction
    Vec defaultVec;

    // Test construction from scalar (broadcast)
    Vec scalarVec(static_cast<T>(42));
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(scalarVec[i], static_cast<T>(42));
    }

    // Test construction from array
    Vec arrayVec(this->testData1);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(arrayVec[i], this->testData1[i]);
    }
}

TYPED_TEST_P(SIMDWrapperTest, LoadStoreOperations) {
    using Vec = typename TestFixture::Vec;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    // Test aligned load/store
    Vec loadedVec = Vec::load(this->alignedData1);
    loadedVec.store(this->alignedResult);
    EXPECT_TRUE(
        this->compareArrays(this->alignedData1, this->alignedResult, N));

    // Test unaligned load/store
    Vec unalignedVec = Vec::loadu(this->testData1.data());
    unalignedVec.storeu(this->alignedResult);
    EXPECT_TRUE(
        this->compareArrays(this->testData1.data(), this->alignedResult, N));
}

TYPED_TEST_P(SIMDWrapperTest, ArithmeticOperations) {
    using Vec = typename TestFixture::Vec;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    Vec vec1(this->testData1);
    Vec vec2(this->testData2);

    // Test addition
    Vec addResult = vec1 + vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(addResult[i], this->testData1[i] + this->testData2[i]);
    }

    // Test subtraction
    Vec subResult = vec1 - vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(subResult[i], this->testData1[i] - this->testData2[i]);
    }

    // Test multiplication
    Vec mulResult = vec1 * vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(mulResult[i], this->testData1[i] * this->testData2[i]);
    }

    // Test division (avoid division by zero)
    if constexpr (std::is_floating_point_v<T>) {
        Vec divResult = vec1 / vec2;
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(divResult[i], this->testData1[i] / this->testData2[i],
                        T(1e-6));
        }
    }
}

TYPED_TEST_P(SIMDWrapperTest, CompoundAssignmentOperators) {
    using Vec = typename TestFixture::Vec;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    Vec vec1(this->testData1);
    Vec vec2(this->testData2);
    Vec original1(this->testData1);

    // Test +=
    vec1 += vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(vec1[i], original1[i] + this->testData2[i]);
    }

    // Reset and test -=
    vec1 = Vec(this->testData1);
    vec1 -= vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(vec1[i], original1[i] - this->testData2[i]);
    }

    // Reset and test *=
    vec1 = Vec(this->testData1);
    vec1 *= vec2;
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(vec1[i], original1[i] * this->testData2[i]);
    }
}

TYPED_TEST_P(SIMDWrapperTest, MathematicalFunctions) {
    using Vec = typename TestFixture::Vec;
    using Traits = typename TestFixture::Traits;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    if constexpr (std::is_floating_point_v<T>) {
        auto positiveData = this->testData3;  // Contains positive values

        // Test sqrt
        auto sqrtResult = Traits::sqrt(positiveData);
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(sqrtResult[i], std::sqrt(positiveData[i]), T(1e-6));
        }

        // Test sin
        auto sinResult = Traits::sin(positiveData);
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(sinResult[i], std::sin(positiveData[i]), T(1e-6));
        }

        // Test cos
        auto cosResult = Traits::cos(positiveData);
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(cosResult[i], std::cos(positiveData[i]), T(1e-6));
        }

        // Test exp
        auto expResult = Traits::exp(positiveData);
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(expResult[i], std::exp(positiveData[i]), T(1e-5));
        }

        // Test log
        auto logResult = Traits::log(positiveData);
        for (size_t i = 0; i < N; ++i) {
            EXPECT_NEAR(logResult[i], std::log(positiveData[i]), T(1e-6));
        }
    }

    // Test abs (works for both integer and floating point)
    auto absResult = Traits::abs(this->testData1);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(absResult[i], std::abs(this->testData1[i]));
    }
}

TYPED_TEST_P(SIMDWrapperTest, MinMaxOperations) {
    using Traits = typename TestFixture::Traits;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    // Test min
    auto minResult = Traits::min(this->testData1, this->testData2);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(minResult[i],
                  std::min(this->testData1[i], this->testData2[i]));
    }

    // Test max
    auto maxResult = Traits::max(this->testData1, this->testData2);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(maxResult[i],
                  std::max(this->testData1[i], this->testData2[i]));
    }

    // Test horizontal operations
    T expectedSum =
        std::accumulate(this->testData1.begin(), this->testData1.end(), T(0));
    T actualSum = Traits::horizontal_sum(this->testData1);
    EXPECT_EQ(actualSum, expectedSum);

    T expectedMax =
        *std::max_element(this->testData1.begin(), this->testData1.end());
    T actualMax = Traits::horizontal_max(this->testData1);
    EXPECT_EQ(actualMax, expectedMax);

    T expectedMin =
        *std::min_element(this->testData1.begin(), this->testData1.end());
    T actualMin = Traits::horizontal_min(this->testData1);
    EXPECT_EQ(actualMin, expectedMin);
}

TYPED_TEST_P(SIMDWrapperTest, ComparisonOperations) {
    using Traits = typename TestFixture::Traits;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    // Test equality comparison
    auto eqResult = Traits::cmpeq(this->testData1, this->testData1);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(eqResult[i]);
    }

    // Test inequality comparison
    auto neResult = Traits::cmpne(this->testData1, this->testData2);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(neResult[i], (this->testData1[i] != this->testData2[i]));
    }

    // Test less than comparison
    auto ltResult = Traits::cmplt(this->testData1, this->testData2);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(ltResult[i], (this->testData1[i] < this->testData2[i]));
    }

    // Test greater than comparison
    auto gtResult = Traits::cmpgt(this->testData1, this->testData2);
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(gtResult[i], (this->testData1[i] > this->testData2[i]));
    }
}

TYPED_TEST_P(SIMDWrapperTest, FusedOperations) {
    using Traits = typename TestFixture::Traits;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;

    // Test fused multiply-add: a * b + c
    auto fmaddResult =
        Traits::fmadd(this->testData1, this->testData2, this->testData3);
    for (size_t i = 0; i < N; ++i) {
        T expected =
            this->testData1[i] * this->testData2[i] + this->testData3[i];
        if constexpr (std::is_floating_point_v<T>) {
            EXPECT_NEAR(fmaddResult[i], expected, T(1e-6));
        } else {
            EXPECT_EQ(fmaddResult[i], expected);
        }
    }

    // Test fused multiply-subtract: a * b - c
    auto fmsubResult =
        Traits::fmsub(this->testData1, this->testData2, this->testData3);
    for (size_t i = 0; i < N; ++i) {
        T expected =
            this->testData1[i] * this->testData2[i] - this->testData3[i];
        if constexpr (std::is_floating_point_v<T>) {
            EXPECT_NEAR(fmsubResult[i], expected, T(1e-6));
        } else {
            EXPECT_EQ(fmsubResult[i], expected);
        }
    }
}

TYPED_TEST_P(SIMDWrapperTest, PerformanceTest) {
    using Vec = typename TestFixture::Vec;
    using T = TypeParam;
    constexpr size_t N = TestFixture::Vec::width;
    constexpr size_t iterations = 10000;

    Vec vec1(this->randomData1);
    Vec vec2(this->randomData2);

    auto start = std::chrono::high_resolution_clock::now();

    Vec result = vec1;
    for (size_t i = 0; i < iterations; ++i) {
        result = result + vec2;
        result = result * vec1;
        result = result - vec2;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Store result to prevent optimization
    result.storeu(this->alignedResult);

    // Performance should be reasonable (adjust threshold as needed)
    EXPECT_LT(duration.count(), 100000)
        << "SIMD operations took too long: " << duration.count() << "μs";
}

// Register the test suite for different types and sizes
REGISTER_TYPED_TEST_SUITE_P(SIMDWrapperTest, BasicConstruction,
                            LoadStoreOperations, ArithmeticOperations,
                            CompoundAssignmentOperators, MathematicalFunctions,
                            MinMaxOperations, ComparisonOperations,
                            FusedOperations, PerformanceTest);

// Instantiate tests for different types
using SIMDTestTypes = ::testing::Types<float, double, int32_t>;
INSTANTIATE_TYPED_TEST_SUITE_P(SIMDTypes, SIMDWrapperTest, SIMDTestTypes);

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_SIMD_WRAPPER_HPP
