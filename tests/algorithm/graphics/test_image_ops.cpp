/*
 * test_image_ops.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "atom/algorithm/graphics/image_ops.hpp"

namespace atom::algorithm::test {

class ImageOpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple test image (gradient)
        test_image_.resize(width_ * height_);
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                test_image_[y * width_ + x] = static_cast<uint8_t>(
                    (x + y) * 255 / (width_ + height_ - 2));
            }
        }
    }

    static constexpr int width_ = 32;
    static constexpr int height_ = 32;
    std::vector<uint8_t> test_image_;
};

TEST_F(ImageOpsTest, ConvolveIdentityKernel) {
    // Identity kernel should not change the image
    std::vector<float> identity_kernel = {0, 0, 0, 0, 1, 0, 0, 0, 0};

    auto result = ImageOps::convolve<uint8_t>(test_image_, width_, height_,
                                              identity_kernel, 3);

    EXPECT_EQ(result.size(), test_image_.size());

    // Check center pixels (edges may differ due to boundary handling)
    for (int y = 1; y < height_ - 1; ++y) {
        for (int x = 1; x < width_ - 1; ++x) {
            EXPECT_EQ(result[y * width_ + x], test_image_[y * width_ + x])
                << "Mismatch at (" << x << ", " << y << ")";
        }
    }
}

TEST_F(ImageOpsTest, ConvolveBoxBlur) {
    std::vector<float> box_kernel(9, 1.0f / 9.0f);

    auto result = ImageOps::convolve<uint8_t>(test_image_, width_, height_,
                                              box_kernel, 3);

    EXPECT_EQ(result.size(), test_image_.size());
    // Just verify it doesn't crash and produces valid output
}

TEST_F(ImageOpsTest, GaussianBlur) {
    auto result =
        ImageOps::gaussianBlur<uint8_t>(test_image_, width_, height_, 1.0f);

    EXPECT_EQ(result.size(), test_image_.size());
    // Blurred image should have reduced variance
}

TEST_F(ImageOpsTest, SobelEdgeDetection) {
    auto result =
        ImageOps::sobelEdgeDetection<uint8_t>(test_image_, width_, height_);

    EXPECT_EQ(result.size(), test_image_.size());
}

TEST_F(ImageOpsTest, LaplacianEdgeDetection) {
    auto result =
        ImageOps::laplacianEdgeDetection<uint8_t>(test_image_, width_, height_);

    EXPECT_EQ(result.size(), test_image_.size());
}

TEST_F(ImageOpsTest, AdjustBrightness) {
    auto brighter = ImageOps::adjustBrightness<uint8_t>(test_image_, 50);
    auto darker = ImageOps::adjustBrightness<uint8_t>(test_image_, -50);

    EXPECT_EQ(brighter.size(), test_image_.size());
    EXPECT_EQ(darker.size(), test_image_.size());

    // Check that brightness actually changed
    int sum_original = 0, sum_brighter = 0, sum_darker = 0;
    for (size_t i = 0; i < test_image_.size(); ++i) {
        sum_original += test_image_[i];
        sum_brighter += brighter[i];
        sum_darker += darker[i];
    }

    EXPECT_GT(sum_brighter, sum_original);
    EXPECT_LT(sum_darker, sum_original);
}

TEST_F(ImageOpsTest, AdjustContrast) {
    auto high_contrast = ImageOps::adjustContrast<uint8_t>(test_image_, 1.5f);
    auto low_contrast = ImageOps::adjustContrast<uint8_t>(test_image_, 0.5f);

    EXPECT_EQ(high_contrast.size(), test_image_.size());
    EXPECT_EQ(low_contrast.size(), test_image_.size());
}

TEST_F(ImageOpsTest, HistogramEqualization) {
    auto result = ImageOps::histogramEqualization<uint8_t>(test_image_);

    EXPECT_EQ(result.size(), test_image_.size());
}

TEST_F(ImageOpsTest, Threshold) {
    auto result = ImageOps::threshold<uint8_t>(test_image_, 128);

    EXPECT_EQ(result.size(), test_image_.size());

    // All values should be either 0 or 255
    for (auto val : result) {
        EXPECT_TRUE(val == 0 || val == 255);
    }
}

TEST_F(ImageOpsTest, Invert) {
    auto result = ImageOps::invert<uint8_t>(test_image_);

    EXPECT_EQ(result.size(), test_image_.size());

    for (size_t i = 0; i < test_image_.size(); ++i) {
        EXPECT_EQ(result[i], 255 - test_image_[i]);
    }
}

TEST_F(ImageOpsTest, InvalidKernelSize) {
    std::vector<float> even_kernel(4, 0.25f);  // 2x2 kernel (invalid)

    EXPECT_THROW(ImageOps::convolve<uint8_t>(test_image_, width_, height_,
                                             even_kernel, 2),
                 std::invalid_argument);
}

}  // namespace atom::algorithm::test
