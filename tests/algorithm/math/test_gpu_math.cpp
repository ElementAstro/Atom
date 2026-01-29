/*
 * test_gpu_math.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <vector>

#include "atom/algorithm/math/gpu_math.hpp"

namespace atom::algorithm::gpu::test {

class GPUMathTest : public ::testing::Test {
protected:
    void SetUp() override {
        gpu_.initialize();

        // Generate test data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-100.0f, 100.0f);

        vec_a_.resize(test_size_);
        vec_b_.resize(test_size_);
        for (size_t i = 0; i < test_size_; ++i) {
            vec_a_[i] = dis(gen);
            vec_b_[i] = dis(gen);
        }
    }

    GPUMath gpu_;
    static constexpr size_t test_size_ = 10000;
    std::vector<float> vec_a_;
    std::vector<float> vec_b_;
};

TEST_F(GPUMathTest, Initialize) {
    GPUMath gpu;
    bool result = gpu.initialize();
    // May fail if no GPU available, but shouldn't crash
    SUCCEED();
}

TEST_F(GPUMathTest, IsAvailable) {
    bool available = gpu_.isAvailable();
    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(GPUMathTest, VectorAdd) {
    auto result = gpu_.vectorAdd(vec_a_, vec_b_);

    EXPECT_EQ(result.size(), test_size_);

    for (size_t i = 0; i < test_size_; ++i) {
        float expected = vec_a_[i] + vec_b_[i];
        EXPECT_NEAR(result[i], expected, std::abs(expected) * 1e-5f + 1e-5f)
            << "Mismatch at index " << i;
    }
}

TEST_F(GPUMathTest, VectorMultiply) {
    auto result = gpu_.vectorMultiply(vec_a_, vec_b_);

    EXPECT_EQ(result.size(), test_size_);

    for (size_t i = 0; i < test_size_; ++i) {
        float expected = vec_a_[i] * vec_b_[i];
        EXPECT_NEAR(result[i], expected, std::abs(expected) * 1e-4f + 1e-5f)
            << "Mismatch at index " << i;
    }
}

TEST_F(GPUMathTest, DotProduct) {
    float result = gpu_.dotProduct(vec_a_, vec_b_);

    float expected = 0.0f;
    for (size_t i = 0; i < test_size_; ++i) {
        expected += vec_a_[i] * vec_b_[i];
    }

    EXPECT_NEAR(result, expected, std::abs(expected) * 1e-3f);
}

TEST_F(GPUMathTest, MatrixMultiply) {
    // Small matrix test
    std::vector<float> A = {1, 2, 3, 4, 5, 6};  // 2x3
    std::vector<float> B = {1, 2, 3, 4, 5, 6};  // 3x2

    auto C = gpu_.matrixMultiply(A, B, 2, 3, 2);

    EXPECT_EQ(C.size(), 4u);  // 2x2 result

    // Expected: [[22, 28], [49, 64]]
    EXPECT_NEAR(C[0], 22.0f, 1e-4f);
    EXPECT_NEAR(C[1], 28.0f, 1e-4f);
    EXPECT_NEAR(C[2], 49.0f, 1e-4f);
    EXPECT_NEAR(C[3], 64.0f, 1e-4f);
}

TEST_F(GPUMathTest, MatrixTranspose) {
    std::vector<float> matrix = {1, 2, 3, 4, 5, 6};  // 2x3

    auto transposed = gpu_.matrixTranspose(matrix, 2, 3);

    EXPECT_EQ(transposed.size(), 6u);  // 3x2

    // Expected: [[1, 4], [2, 5], [3, 6]]
    EXPECT_NEAR(transposed[0], 1.0f, 1e-5f);
    EXPECT_NEAR(transposed[1], 4.0f, 1e-5f);
    EXPECT_NEAR(transposed[2], 2.0f, 1e-5f);
    EXPECT_NEAR(transposed[3], 5.0f, 1e-5f);
    EXPECT_NEAR(transposed[4], 3.0f, 1e-5f);
    EXPECT_NEAR(transposed[5], 6.0f, 1e-5f);
}

TEST_F(GPUMathTest, GeneratePrimes) {
    auto primes = gpu_.generatePrimes(100);

    // Known primes up to 100
    std::vector<uint32_t> expected = {2,  3,  5,  7,  11, 13, 17, 19, 23,
                                      29, 31, 37, 41, 43, 47, 53, 59, 61,
                                      67, 71, 73, 79, 83, 89, 97};

    EXPECT_EQ(primes.size(), expected.size());
    for (size_t i = 0; i < std::min(primes.size(), expected.size()); ++i) {
        EXPECT_EQ(primes[i], expected[i]) << "Mismatch at index " << i;
    }
}

TEST_F(GPUMathTest, Mean) {
    float result = gpu_.mean(vec_a_);

    float expected = 0.0f;
    for (auto v : vec_a_) {
        expected += v;
    }
    expected /= vec_a_.size();

    EXPECT_NEAR(result, expected, std::abs(expected) * 1e-4f + 1e-4f);
}

TEST_F(GPUMathTest, EmptyVector) {
    std::vector<float> empty;

    auto result = gpu_.vectorAdd(empty, empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(GPUMathTest, MismatchedVectorSizes) {
    std::vector<float> a = {1, 2, 3};
    std::vector<float> b = {1, 2};

    // Should handle gracefully (either throw or return empty/truncated)
    EXPECT_NO_FATAL_FAILURE(gpu_.vectorAdd(a, b));
}

TEST_F(GPUMathTest, Variance) {
    // Test with known data
    std::vector<float> data = {2.0f, 4.0f, 4.0f, 4.0f, 5.0f, 5.0f, 7.0f, 9.0f};
    float result = gpu_.variance(data);

    // Calculate expected variance
    float mean = 0.0f;
    for (auto v : data) {
        mean += v;
    }
    mean /= data.size();

    float expected = 0.0f;
    for (auto v : data) {
        expected += (v - mean) * (v - mean);
    }
    expected /= data.size();

    EXPECT_NEAR(result, expected, 1e-4f);
}

TEST_F(GPUMathTest, VarianceLargeData) {
    float result = gpu_.variance(vec_a_);

    // Calculate expected variance
    float mean = 0.0f;
    for (auto v : vec_a_) {
        mean += v;
    }
    mean /= vec_a_.size();

    float expected = 0.0f;
    for (auto v : vec_a_) {
        expected += (v - mean) * (v - mean);
    }
    expected /= vec_a_.size();

    EXPECT_NEAR(result, expected, std::abs(expected) * 1e-3f);
}

TEST_F(GPUMathTest, SingleElementVector) {
    std::vector<float> single = {42.0f};

    auto add_result = gpu_.vectorAdd(single, single);
    ASSERT_EQ(add_result.size(), 1u);
    EXPECT_NEAR(add_result[0], 84.0f, 1e-5f);

    auto mul_result = gpu_.vectorMultiply(single, single);
    ASSERT_EQ(mul_result.size(), 1u);
    EXPECT_NEAR(mul_result[0], 1764.0f, 1e-3f);

    float dot_result = gpu_.dotProduct(single, single);
    EXPECT_NEAR(dot_result, 1764.0f, 1e-3f);

    float mean_result = gpu_.mean(single);
    EXPECT_NEAR(mean_result, 42.0f, 1e-5f);
}

TEST_F(GPUMathTest, IdentityMatrix) {
    // 3x3 identity matrix
    std::vector<float> identity = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    std::vector<float> matrix = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = gpu_.matrixMultiply(identity, matrix, 3, 3, 3);

    ASSERT_EQ(result.size(), 9u);
    for (size_t i = 0; i < 9; ++i) {
        EXPECT_NEAR(result[i], matrix[i], 1e-4f);
    }
}

TEST_F(GPUMathTest, ZeroMatrix) {
    std::vector<float> zeros(9, 0.0f);
    std::vector<float> matrix = {1, 2, 3, 4, 5, 6, 7, 8, 9};

    auto result = gpu_.matrixMultiply(zeros, matrix, 3, 3, 3);

    ASSERT_EQ(result.size(), 9u);
    for (size_t i = 0; i < 9; ++i) {
        EXPECT_NEAR(result[i], 0.0f, 1e-5f);
    }
}

TEST_F(GPUMathTest, LargeMatrixMultiply) {
    const size_t n = 64;
    std::vector<float> A(n * n);
    std::vector<float> B(n * n);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (size_t i = 0; i < n * n; ++i) {
        A[i] = dis(gen);
        B[i] = dis(gen);
    }

    auto C = gpu_.matrixMultiply(A, B, n, n, n);

    EXPECT_EQ(C.size(), n * n);

    // Verify a few random elements
    for (int test = 0; test < 5; ++test) {
        size_t i = gen() % n;
        size_t j = gen() % n;

        float expected = 0.0f;
        for (size_t k = 0; k < n; ++k) {
            expected += A[i * n + k] * B[k * n + j];
        }

        EXPECT_NEAR(C[i * n + j], expected, std::abs(expected) * 1e-3f + 1e-3f);
    }
}

TEST_F(GPUMathTest, TransposeSquareMatrix) {
    std::vector<float> matrix = {1, 2, 3, 4, 5, 6, 7, 8, 9};  // 3x3

    auto transposed = gpu_.matrixTranspose(matrix, 3, 3);

    ASSERT_EQ(transposed.size(), 9u);

    // Expected: [[1, 4, 7], [2, 5, 8], [3, 6, 9]]
    std::vector<float> expected = {1, 4, 7, 2, 5, 8, 3, 6, 9};
    for (size_t i = 0; i < 9; ++i) {
        EXPECT_NEAR(transposed[i], expected[i], 1e-5f);
    }
}

TEST_F(GPUMathTest, DoubleTransposeIsIdentity) {
    std::vector<float> matrix = {1, 2, 3, 4, 5, 6};  // 2x3

    auto transposed = gpu_.matrixTranspose(matrix, 2, 3);             // 3x2
    auto double_transposed = gpu_.matrixTranspose(transposed, 3, 2);  // 2x3

    ASSERT_EQ(double_transposed.size(), matrix.size());
    for (size_t i = 0; i < matrix.size(); ++i) {
        EXPECT_NEAR(double_transposed[i], matrix[i], 1e-5f);
    }
}

TEST_F(GPUMathTest, GeneratePrimesSmall) {
    auto primes = gpu_.generatePrimes(10);

    std::vector<uint32_t> expected = {2, 3, 5, 7};
    EXPECT_EQ(primes.size(), expected.size());
    for (size_t i = 0; i < std::min(primes.size(), expected.size()); ++i) {
        EXPECT_EQ(primes[i], expected[i]);
    }
}

TEST_F(GPUMathTest, GeneratePrimesEdgeCases) {
    // Primes up to 2
    auto primes2 = gpu_.generatePrimes(2);
    EXPECT_EQ(primes2.size(), 1u);
    if (!primes2.empty()) {
        EXPECT_EQ(primes2[0], 2u);
    }

    // Primes up to 1 (should be empty)
    auto primes1 = gpu_.generatePrimes(1);
    EXPECT_TRUE(primes1.empty());

    // Primes up to 0 (should be empty)
    auto primes0 = gpu_.generatePrimes(0);
    EXPECT_TRUE(primes0.empty());
}

TEST_F(GPUMathTest, NegativeValues) {
    std::vector<float> negative = {-1.0f, -2.0f, -3.0f, -4.0f, -5.0f};

    float mean_result = gpu_.mean(negative);
    EXPECT_NEAR(mean_result, -3.0f, 1e-5f);

    auto add_result = gpu_.vectorAdd(negative, negative);
    ASSERT_EQ(add_result.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_NEAR(add_result[i], negative[i] * 2, 1e-5f);
    }
}

TEST_F(GPUMathTest, CPUFallbackConsistency) {
    // This test verifies that results are consistent whether GPU or CPU is used
    // The implementation should fall back to CPU if GPU is not available

    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    std::vector<float> b = {5.0f, 4.0f, 3.0f, 2.0f, 1.0f};

    auto sum = gpu_.vectorAdd(a, b);
    ASSERT_EQ(sum.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_NEAR(sum[i], 6.0f, 1e-5f);  // All elements should be 6
    }

    auto product = gpu_.vectorMultiply(a, b);
    ASSERT_EQ(product.size(), 5u);
    std::vector<float> expected_product = {5.0f, 8.0f, 9.0f, 8.0f, 5.0f};
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_NEAR(product[i], expected_product[i], 1e-5f);
    }

    float dot = gpu_.dotProduct(a, b);
    EXPECT_NEAR(dot, 35.0f, 1e-4f);  // 5 + 8 + 9 + 8 + 5 = 35
}

}  // namespace atom::algorithm::gpu::test
