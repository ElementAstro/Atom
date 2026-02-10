/*
 * test_noise_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Unit Tests for NoiseBase Class

**************************************************/

#include <gtest/gtest.h>
#include "atom/algorithm/graphics/noise_base.hpp"

using namespace atom::algorithm;

class NoiseBaseTest : public ::testing::Test {
protected:
    // Concrete test class to access protected members
    class TestableNoiseBase : public NoiseBase {
    public:
        using NoiseBase::NoiseBase;
        using NoiseBase::perm_;
        using NoiseBase::fade;
        using NoiseBase::lerp;
        using NoiseBase::grad2D;
        using NoiseBase::grad3D;
    };
};

TEST_F(NoiseBaseTest, ConstructorWithSeed) {
    EXPECT_NO_THROW(TestableNoiseBase noise(12345));
    EXPECT_NO_THROW(TestableNoiseBase noise(0));
    EXPECT_NO_THROW(TestableNoiseBase noise(std::numeric_limits<u32>::max()));
}

TEST_F(NoiseBaseTest, PermutationTableInitialized) {
    TestableNoiseBase noise(42);

    // Check permutation table is properly sized
    EXPECT_EQ(noise.perm_.size(), 512u);

    // Check that first 256 values are duplicated in second half
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_EQ(noise.perm_[i], noise.perm_[i + 256]);
    }
}

TEST_F(NoiseBaseTest, ReseedChangesPermutation) {
    TestableNoiseBase noise(42);
    auto original_perm = noise.perm_;

    noise.reseed(12345);

    // Permutation should be different after reseed
    bool different = false;
    for (size_t i = 0; i < 256; ++i) {
        if (noise.perm_[i] != original_perm[i]) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(NoiseBaseTest, FadeFunction) {
    // fade(0) = 0, fade(1) = 1
    EXPECT_DOUBLE_EQ(TestableNoiseBase::fade(0.0), 0.0);
    EXPECT_DOUBLE_EQ(TestableNoiseBase::fade(1.0), 1.0);

    // fade should be monotonically increasing in [0,1]
    double prev = 0.0;
    for (double t = 0.1; t <= 1.0; t += 0.1) {
        double current = TestableNoiseBase::fade(t);
        EXPECT_GE(current, prev);
        prev = current;
    }
}

TEST_F(NoiseBaseTest, LerpFunction) {
    EXPECT_DOUBLE_EQ(TestableNoiseBase::lerp(0.0, 0.0, 10.0), 0.0);
    EXPECT_DOUBLE_EQ(TestableNoiseBase::lerp(1.0, 0.0, 10.0), 10.0);
    EXPECT_DOUBLE_EQ(TestableNoiseBase::lerp(0.5, 0.0, 10.0), 5.0);
    EXPECT_DOUBLE_EQ(TestableNoiseBase::lerp(0.25, 0.0, 100.0), 25.0);
}

TEST_F(NoiseBaseTest, Grad2DValues) {
    // grad2D should return values based on hash
    for (int hash = 0; hash < 16; ++hash) {
        double result = TestableNoiseBase::grad2D(hash, 1.0, 1.0);
        // Result should be finite
        EXPECT_TRUE(std::isfinite(result));
    }
}

TEST_F(NoiseBaseTest, Grad3DValues) {
    // grad3D should return values based on hash
    for (int hash = 0; hash < 16; ++hash) {
        double result = TestableNoiseBase::grad3D(hash, 1.0, 1.0, 1.0);
        // Result should be finite
        EXPECT_TRUE(std::isfinite(result));
    }
}

TEST_F(NoiseBaseTest, DifferentSeedsProduceDifferentResults) {
    TestableNoiseBase noise1(100);
    TestableNoiseBase noise2(200);

    // Check that permutations differ
    int differences = 0;
    for (size_t i = 0; i < 256; ++i) {
        if (noise1.perm_[i] != noise2.perm_[i]) {
            ++differences;
        }
    }

    // Most values should be different
    EXPECT_GT(differences, 200);
}
