#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include "atom/algorithm/graphics/simplex.hpp"

using namespace atom::algorithm;

class SimplexNoiseTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        noise_ = std::make_unique<SimplexNoise>(
            12345);  // Fixed seed for reproducible tests
    }

    std::unique_ptr<SimplexNoise> noise_;
};

TEST_F(SimplexNoiseTest, Constructor) {
    EXPECT_NO_THROW(SimplexNoise());
    EXPECT_NO_THROW(SimplexNoise(42));
    EXPECT_NO_THROW(SimplexNoise(std::default_random_engine::default_seed));
}

TEST_F(SimplexNoiseTest, Noise2DRange) {
    // Test that 2D noise values are in expected range
    for (double x = 0.0; x < 10.0; x += 1.0) {
        for (double y = 0.0; y < 10.0; y += 1.0) {
            double value = noise_->noise2D(x, y);
            EXPECT_GE(value, -1.0)
                << "Value at (" << x << ", " << y << ") is below -1.0";
            EXPECT_LE(value, 1.0)
                << "Value at (" << x << ", " << y << ") is above 1.0";
        }
    }
}

TEST_F(SimplexNoiseTest, Noise2DConsistency) {
    // Test that same coordinates produce same values
    double value1 = noise_->noise2D(5.3, 2.7);
    double value2 = noise_->noise2D(5.3, 2.7);

    EXPECT_DOUBLE_EQ(value1, value2);
}

TEST_F(SimplexNoiseTest, Noise3DRange) {
    // Test that 3D noise values are in expected range
    for (double x = 0.0; x < 5.0; x += 1.0) {
        for (double y = 0.0; y < 5.0; y += 1.0) {
            for (double z = 0.0; z < 5.0; z += 1.0) {
                double value = noise_->noise3D(x, y, z);
                EXPECT_GE(value, -1.0) << "Value at (" << x << ", " << y << ", "
                                       << z << ") is below -1.0";
                EXPECT_LE(value, 1.0) << "Value at (" << x << ", " << y << ", "
                                      << z << ") is above 1.0";
            }
        }
    }
}

TEST_F(SimplexNoiseTest, Noise3DConsistency) {
    // Test that same coordinates produce same values
    double value1 = noise_->noise3D(1.2, 3.4, 5.6);
    double value2 = noise_->noise3D(1.2, 3.4, 5.6);

    EXPECT_DOUBLE_EQ(value1, value2);
}

// NOTE: noise4D is not implemented in SimplexNoise class
// TEST_F(SimplexNoiseTest, Noise4DRange) - DISABLED
// TEST_F(SimplexNoiseTest, Noise4DConsistency) - DISABLED

TEST_F(SimplexNoiseTest, Fractal2D) {
    // Test fractal noise functionality (using correct method name: fractal2D)
    double value = noise_->fractal2D(5.0, 3.0, 4, 0.5, 2.0);

    // Fractal noise should be normalized to [-1, 1] range
    EXPECT_GE(value, -1.0);
    EXPECT_LE(value, 1.0);
}

// NOTE: fractalNoise3D and fractalNoise4D are not implemented in SimplexNoise
// class TEST_F(SimplexNoiseTest, FractalNoise3D) - DISABLED
// TEST_F(SimplexNoiseTest, FractalNoise4D) - DISABLED

TEST_F(SimplexNoiseTest, DifferentSeeds) {
    SimplexNoise noise1(123);
    SimplexNoise noise2(456);

    // Same coordinates should produce different values with different seeds
    double value1 = noise1.noise2D(10.5, 20.5);
    double value2 = noise2.noise2D(10.5, 20.5);

    EXPECT_NE(value1, value2);
}

TEST_F(SimplexNoiseTest, ZeroCoordinates) {
    // Test noise at origin
    double value2d = noise_->noise2D(0.0, 0.0);
    double value3d = noise_->noise3D(0.0, 0.0, 0.0);

    EXPECT_GE(value2d, -1.0);
    EXPECT_LE(value2d, 1.0);
    EXPECT_GE(value3d, -1.0);
    EXPECT_LE(value3d, 1.0);
}

TEST_F(SimplexNoiseTest, NegativeCoordinates) {
    // Test noise with negative coordinates
    double value2d = noise_->noise2D(-5.0, -3.0);
    double value3d = noise_->noise3D(-2.0, -4.0, -1.0);

    EXPECT_GE(value2d, -1.0);
    EXPECT_LE(value2d, 1.0);
    EXPECT_GE(value3d, -1.0);
    EXPECT_LE(value3d, 1.0);
}

TEST_F(SimplexNoiseTest, LargeCoordinates) {
    // Test noise with large coordinates
    double value2d = noise_->noise2D(10000.5, 20000.5);
    double value3d = noise_->noise3D(5000.0, 10000.0, 15000.0);

    EXPECT_GE(value2d, -1.0);
    EXPECT_LE(value2d, 1.0);
    EXPECT_GE(value3d, -1.0);
    EXPECT_LE(value3d, 1.0);
}

TEST_F(SimplexNoiseTest, VerySmallCoordinates) {
    // Test noise with very small coordinates
    double value2d = noise_->noise2D(0.001, 0.002);
    double value3d = noise_->noise3D(0.0001, 0.0002, 0.0003);

    EXPECT_GE(value2d, -1.0);
    EXPECT_LE(value2d, 1.0);
    EXPECT_GE(value3d, -1.0);
    EXPECT_LE(value3d, 1.0);
}

TEST_F(SimplexNoiseTest, Continuity) {
    // Test that noise is reasonably continuous
    double dx = 0.01;
    double x = 10.0;
    double y = 5.0;

    double value1 = noise_->noise2D(x, y);
    double value2 = noise_->noise2D(x + dx, y);
    double value3 = noise_->noise2D(x, y + dx);

    // Differences should be small for small coordinate changes
    EXPECT_LT(std::abs(value1 - value2), 0.5) << "Change in x should be small";
    EXPECT_LT(std::abs(value1 - value3), 0.5) << "Change in y should be small";
}

TEST_F(SimplexNoiseTest, FractalNoiseParameters) {
    // Test different fractal noise parameters (using correct method name:
    // fractal2D)
    double base_value = noise_->fractal2D(5.0, 5.0, 1, 0.5, 2.0);

    // More octaves should generally create more variation
    double more_octaves = noise_->fractal2D(5.0, 5.0, 8, 0.5, 2.0);

    // Test with zero persistence (should return basic noise)
    double zero_persistence = noise_->fractal2D(5.0, 5.0, 4, 0.0, 2.0);

    EXPECT_NE(base_value, more_octaves);
    EXPECT_NE(base_value, zero_persistence);
}

// NOTE: turbulence2D and turbulence3D are not implemented in SimplexNoise class
// TEST_F(SimplexNoiseTest, Turbulence2D) - DISABLED
// TEST_F(SimplexNoiseTest, Turbulence3D) - DISABLED

TEST_F(SimplexNoiseTest, Performance) {
    // Test performance with many noise evaluations
    const size_t num_evaluations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < num_evaluations; ++i) {
        double x = static_cast<double>(i % 100) * 0.1;
        double y = static_cast<double>(i / 100) * 0.1;
        double value = noise_->noise2D(x, y);
        EXPECT_GE(value, -1.0);
        EXPECT_LE(value, 1.0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    spdlog::info("Evaluated {} 2D noise samples in {} ms", num_evaluations,
                 duration.count());
}

TEST_F(SimplexNoiseTest, Reproducibility) {
    // Same seed should produce identical results
    SimplexNoise noise_a(42);
    SimplexNoise noise_b(42);

    std::vector<std::pair<double, double>> test_coords = {
        {0.0, 0.0}, {1.5, 2.3}, {10.0, 5.0}, {-3.2, 4.1}, {100.5, -50.2}};

    for (const auto& [x, y] : test_coords) {
        double value_a = noise_a.noise2D(x, y);
        double value_b = noise_b.noise2D(x, y);
        EXPECT_DOUBLE_EQ(value_a, value_b)
            << "Values differ at (" << x << ", " << y << ")";
    }
}

TEST_F(SimplexNoiseTest, EdgeCases) {
    // Test extreme values
    double max_double = std::numeric_limits<double>::max();
    double min_double = std::numeric_limits<double>::lowest();
    double inf_double = std::numeric_limits<double>::infinity();
    double nan_double = std::numeric_limits<double>::quiet_NaN();

    // These should not crash the implementation
    double result1, result2, result3, result4;
    EXPECT_NO_THROW(result1 = noise_->noise2D(max_double, max_double));
    EXPECT_NO_THROW(result2 = noise_->noise2D(min_double, min_double));
    EXPECT_NO_THROW(result3 = noise_->noise2D(inf_double, inf_double));
    EXPECT_NO_THROW(result4 = noise_->noise2D(nan_double, nan_double));

    // Suppress unused variable warnings
    (void)result1;
    (void)result2;
    (void)result3;
    (void)result4;
}
