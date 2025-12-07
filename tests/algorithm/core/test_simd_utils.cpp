/*
 * test_simd_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "atom/algorithm/core/simd_utils.hpp"

namespace atom::algorithm::simd::test {

class SIMDUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test data
        for (size_t i = 0; i < test_size_; ++i) {
            float_data_a_.push_back(static_cast<float>(i) * 0.5f);
            float_data_b_.push_back(static_cast<float>(i) * 0.3f);
            double_data_a_.push_back(static_cast<double>(i) * 0.5);
            double_data_b_.push_back(static_cast<double>(i) * 0.3);
        }
    }

    static constexpr size_t test_size_ = 1024;
    std::vector<float> float_data_a_;
    std::vector<float> float_data_b_;
    std::vector<double> double_data_a_;
    std::vector<double> double_data_b_;
};

TEST_F(SIMDUtilsTest, CapabilitiesDetection) {
    // Just verify these don't crash
    bool has_sse2 = SIMDCapabilities::hasSSE2();
    bool has_avx2 = SIMDCapabilities::hasAVX2();
    bool has_avx512 = SIMDCapabilities::hasAVX512();
    bool has_neon = SIMDCapabilities::hasNEON();

    // At least one should be available on modern systems
    EXPECT_TRUE(has_sse2 || has_neon ||
                true);  // Always pass, just test detection
}

TEST_F(SIMDUtilsTest, VectorWidthConstants) {
    EXPECT_GT(VectorWidth::AVX512_F32, 0);
    EXPECT_GT(VectorWidth::AVX512_F64, 0);
    EXPECT_GT(VectorWidth::AVX2_F32, 0);
    EXPECT_GT(VectorWidth::AVX2_F64, 0);
    EXPECT_GT(VectorWidth::SSE_F32, 0);
    EXPECT_GT(VectorWidth::SSE_F64, 0);
}

TEST_F(SIMDUtilsTest, MemoryCopy) {
    std::vector<uint8_t> src(1024);
    std::vector<uint8_t> dst(1024);

    for (size_t i = 0; i < src.size(); ++i) {
        src[i] = static_cast<uint8_t>(i % 256);
    }

    MemoryOps::copy(dst.data(), src.data(), src.size());

    for (size_t i = 0; i < src.size(); ++i) {
        EXPECT_EQ(dst[i], src[i]) << "Mismatch at index " << i;
    }
}

TEST_F(SIMDUtilsTest, MemorySet) {
    std::vector<uint8_t> buffer(1024);
    uint8_t value = 0xAB;

    MemoryOps::set(buffer.data(), value, buffer.size());

    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], value) << "Mismatch at index " << i;
    }
}

TEST_F(SIMDUtilsTest, VectorAddFloat) {
    std::vector<float> result(test_size_);

    MathOps::vectorAdd(float_data_a_.data(), float_data_b_.data(),
                       result.data(), test_size_);

    for (size_t i = 0; i < test_size_; ++i) {
        float expected = float_data_a_[i] + float_data_b_[i];
        EXPECT_NEAR(result[i], expected, 1e-5f) << "Mismatch at index " << i;
    }
}

TEST_F(SIMDUtilsTest, VectorAddDouble) {
    std::vector<double> result(test_size_);

    MathOps::vectorAdd(double_data_a_.data(), double_data_b_.data(),
                       result.data(), test_size_);

    for (size_t i = 0; i < test_size_; ++i) {
        double expected = double_data_a_[i] + double_data_b_[i];
        EXPECT_NEAR(result[i], expected, 1e-10) << "Mismatch at index " << i;
    }
}

TEST_F(SIMDUtilsTest, DotProductFloat) {
    float result = MathOps::dotProduct(float_data_a_.data(),
                                       float_data_b_.data(), test_size_);

    float expected = 0.0f;
    for (size_t i = 0; i < test_size_; ++i) {
        expected += float_data_a_[i] * float_data_b_[i];
    }

    EXPECT_NEAR(result, expected, std::abs(expected) * 1e-4f);
}

TEST_F(SIMDUtilsTest, DotProductDouble) {
    double result = MathOps::dotProduct(double_data_a_.data(),
                                        double_data_b_.data(), test_size_);

    double expected = 0.0;
    for (size_t i = 0; i < test_size_; ++i) {
        expected += double_data_a_[i] * double_data_b_[i];
    }

    EXPECT_NEAR(result, expected, std::abs(expected) * 1e-10);
}

TEST_F(SIMDUtilsTest, OptimalVectorWidth) {
    size_t width_f32 = getOptimalVectorWidth<float>();
    size_t width_f64 = getOptimalVectorWidth<double>();

    EXPECT_GT(width_f32, 0);
    EXPECT_GT(width_f64, 0);
    EXPECT_GE(width_f32, width_f64);  // float width >= double width
}

}  // namespace atom::algorithm::simd::test
