#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <array>
#include <memory>
#include <cmath>

#include "atom/image/processing/filters.hpp"
#include "atom/image/core/image_blob.hpp"
#include "test_utils.hpp"

namespace atom::image::test {

class FiltersTest : public ::testing::Test {
protected:
    void SetUp() override {
        filter = std::make_unique<ImageFilter>();
        fileManager = std::make_unique<TestFileManager>();

        // Create test images
        createTestImages();
    }

    void TearDown() override {
        fileManager->cleanup();
    }

    void createTestImages() {
        // Create a simple gradient image
        auto gradientData = TestDataGenerator::generateGradientImage(32, 32, 1);
        gradient_image = blob(gradientData.data(), gradientData.size());

        // Create a checkerboard pattern
        auto checkerboardData = TestDataGenerator::generateCheckerboard(32, 32, 1, 4);
        checkerboard_image = blob(checkerboardData.data(), checkerboardData.size());

        // Create a noisy image
        auto noisyData = TestDataGenerator::generateRandomNoise(32, 32, 1, 12345);
        noisy_image = blob(noisyData.data(), noisyData.size());

        // Create a solid color image
        auto solidData = TestDataGenerator::generateSolidColor(32, 32, 1, {128});
        solid_image = blob(solidData.data(), solidData.size());

        // Create an edge test image (circle)
        auto circleData = TestDataGenerator::generateCircularPattern(32, 32, 1, 10);
        circle_image = blob(circleData.data(), circleData.size());
    }

    std::unique_ptr<ImageFilter> filter;
    std::unique_ptr<TestFileManager> fileManager;

    blob gradient_image, checkerboard_image, noisy_image, solid_image, circle_image;
};

// Test basic Gaussian blur filter
TEST_F(FiltersTest, GaussianBlurFilter) {
    FilterParams params;
    params.sigma = 1.0;
    params.kernelSize = 5;

    auto result = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test box blur filter
TEST_F(FiltersTest, BoxBlurFilter) {
    FilterParams params;
    params.kernelSize = 3;

    auto result = filter->applyFilter(gradient_image, FilterType::BOX_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test motion blur filter
TEST_F(FiltersTest, MotionBlurFilter) {
    FilterParams params;
    params.angle = 45.0;
    params.distance = 5;

    auto result = filter->applyFilter(gradient_image, FilterType::MOTION_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test sharpening filters
TEST_F(FiltersTest, SharpeningFilters) {
    std::vector<FilterType> sharpenFilters = {
        FilterType::SHARPEN,
        FilterType::UNSHARP_MASK,
        FilterType::HIGH_PASS
    };

    for (const auto& filterType : sharpenFilters) {
        FilterParams params;
        params.strength = 1.5;

        auto result = filter->applyFilter(gradient_image, filterType, params);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), gradient_image.size());
    }
}

// Test edge detection filters
TEST_F(FiltersTest, EdgeDetectionFilters) {
    std::vector<FilterType> edgeFilters = {
        FilterType::SOBEL,
        FilterType::PREWITT,
        FilterType::ROBERTS,
        FilterType::LAPLACIAN
    };

    for (const auto& filterType : edgeFilters) {
        auto result = filter->applyFilter(circle_image, filterType);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), circle_image.size());
    }
}

// Test Canny edge detection
TEST_F(FiltersTest, CannyEdgeDetection) {
    FilterParams params;
    params.threshold1 = 50.0;
    params.threshold2 = 150.0;
    params.kernelSize = 3;

    auto result = filter->applyFilter(circle_image, FilterType::CANNY, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), circle_image.size());
}

// Test noise reduction filters
TEST_F(FiltersTest, NoiseReductionFilters) {
    FilterParams params;
    params.kernelSize = 5;

    // Test median filter
    auto medianResult = filter->applyFilter(noisy_image, FilterType::MEDIAN, params);
    EXPECT_GT(medianResult.size(), 0);
    EXPECT_EQ(medianResult.size(), noisy_image.size());

    // Test bilateral filter
    params.sigmaColor = 75.0;
    params.sigmaSpace = 75.0;
    auto bilateralResult = filter->applyFilter(noisy_image, FilterType::BILATERAL, params);
    EXPECT_GT(bilateralResult.size(), 0);
    EXPECT_EQ(bilateralResult.size(), noisy_image.size());
}

// Test non-local means denoising
TEST_F(FiltersTest, NonLocalMeansDenoising) {
    FilterParams params;
    params.h = 10.0;
    params.templateWindowSize = 7;
    params.searchWindowSize = 21;

    auto result = filter->applyFilter(noisy_image, FilterType::NON_LOCAL_MEANS, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), noisy_image.size());
}

// Test morphological operations
TEST_F(FiltersTest, MorphologicalOperations) {
    std::vector<FilterType> morphOps = {
        FilterType::EROSION,
        FilterType::DILATION,
        FilterType::OPENING,
        FilterType::CLOSING,
        FilterType::GRADIENT,
        FilterType::TOP_HAT,
        FilterType::BLACK_HAT
    };

    for (const auto& operation : morphOps) {
        auto result = filter->applyMorphological(checkerboard_image, operation,
                                               StructuringElement::RECTANGLE, 3);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), checkerboard_image.size());
    }
}

// Test different structuring elements
TEST_F(FiltersTest, StructuringElements) {
    std::vector<StructuringElement> elements = {
        StructuringElement::RECTANGLE,
        StructuringElement::ELLIPSE,
        StructuringElement::CROSS,
        StructuringElement::DIAMOND
    };

    for (const auto& element : elements) {
        auto result = filter->applyMorphological(checkerboard_image, FilterType::EROSION,
                                               element, 3);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), checkerboard_image.size());
    }
}

// Test custom convolution kernel
TEST_F(FiltersTest, CustomConvolutionKernel) {
    // Create a simple edge detection kernel
    std::vector<std::vector<double>> edgeKernel = {
        {-1, -1, -1},
        {-1,  8, -1},
        {-1, -1, -1}
    };

    auto result = filter->applyCustomKernel(gradient_image, edgeKernel, true);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test separable filter
TEST_F(FiltersTest, SeparableFilter) {
    // Create Gaussian kernels
    std::vector<double> gaussianKernel = {0.25, 0.5, 0.25};

    auto result = filter->applySeparableFilter(gradient_image, gaussianKernel, gaussianKernel);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test frequency domain filters
TEST_F(FiltersTest, FrequencyDomainFilters) {
    std::vector<FilterType> freqFilters = {
        FilterType::LOW_PASS,
        FilterType::HIGH_PASS_FREQ,
        FilterType::BAND_PASS,
        FilterType::BAND_STOP
    };

    for (const auto& filterType : freqFilters) {
        FilterParams params;
        params.cutoffFreq = 0.3;
        params.bandwidth = 0.1;

        auto result = filter->applyFrequencyFilter(gradient_image, filterType, params);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), gradient_image.size());
    }
}

// Test adaptive filtering
TEST_F(FiltersTest, AdaptiveFiltering) {
    FilterParams params;
    params.sigma = 1.0;
    params.kernelSize = 5;

    auto result = filter->applyAdaptiveFilter(noisy_image, FilterType::GAUSSIAN_BLUR, 7, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), noisy_image.size());
}

// Test artistic filters
TEST_F(FiltersTest, ArtisticFilters) {
    std::vector<FilterType> artisticFilters = {
        FilterType::EMBOSS,
        FilterType::EDGE_ENHANCE,
        FilterType::FIND_EDGES,
        FilterType::SMOOTH,
        FilterType::SMOOTH_MORE
    };

    for (const auto& filterType : artisticFilters) {
        auto result = filter->applyFilter(gradient_image, filterType);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), gradient_image.size());
    }
}

// Test filter parameter validation
TEST_F(FiltersTest, FilterParameterValidation) {
    FilterParams params;

    // Test invalid kernel size (even number)
    params.kernelSize = 4;
    EXPECT_THROW(filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params),
                 std::invalid_argument);

    // Test negative sigma
    params.kernelSize = 3;
    params.sigma = -1.0;
    EXPECT_THROW(filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params),
                 std::invalid_argument);
}

// Test empty image handling
TEST_F(FiltersTest, EmptyImageHandling) {
    blob emptyImage;

    EXPECT_THROW(filter->applyFilter(emptyImage, FilterType::GAUSSIAN_BLUR),
                 std::invalid_argument);
}

// Test filter chaining
TEST_F(FiltersTest, FilterChaining) {
    FilterParams blurParams;
    blurParams.sigma = 1.0;
    blurParams.kernelSize = 3;

    FilterParams sharpenParams;
    sharpenParams.strength = 1.5;

    // Apply blur first, then sharpen
    auto blurred = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, blurParams);
    auto sharpened = filter->applyFilter(blurred, FilterType::SHARPEN, sharpenParams);

    EXPECT_GT(sharpened.size(), 0);
    EXPECT_EQ(sharpened.size(), gradient_image.size());
}

// Test kernel normalization
TEST_F(FiltersTest, KernelNormalization) {
    // Create an unnormalized kernel
    std::vector<std::vector<double>> unnormalizedKernel = {
        {1, 2, 1},
        {2, 4, 2},
        {1, 2, 1}
    };

    // Test with normalization
    auto normalizedResult = filter->applyCustomKernel(gradient_image, unnormalizedKernel, true);

    // Test without normalization
    auto unnormalizedResult = filter->applyCustomKernel(gradient_image, unnormalizedKernel, false);

    EXPECT_GT(normalizedResult.size(), 0);
    EXPECT_GT(unnormalizedResult.size(), 0);
    EXPECT_EQ(normalizedResult.size(), unnormalizedResult.size());
}

// Test edge handling in convolution
TEST_F(FiltersTest, EdgeHandlingConvolution) {
    // Create a small test image to test edge effects
    auto smallData = TestDataGenerator::generateGradientImage(5, 5, 1);
    blob smallImage(smallData.data(), smallData.size());

    std::vector<std::vector<double>> kernel = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}
    };

    auto result = filter->applyCustomKernel(smallImage, kernel, true);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), smallImage.size());
}

// Test large kernel performance
TEST_F(FiltersTest, LargeKernelPerformance) {
    // Create a large Gaussian kernel
    FilterParams params;
    params.sigma = 5.0;
    params.kernelSize = 15; // Large kernel

    auto start = std::chrono::high_resolution_clock::now();
    auto result = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());

    // Should complete in reasonable time (less than 1 second for test image)
    EXPECT_LT(duration.count(), 1000);
}

// Test filter with different data types
TEST_F(FiltersTest, DifferentDataTypes) {
    // Test with float data
    auto floatData = TestDataGenerator::generateGradientImage(16, 16, 1);
    blob floatImage(floatData.data(), floatData.size());

    FilterParams params;
    params.sigma = 1.0;
    params.kernelSize = 3;

    auto result = filter->applyFilter(floatImage, FilterType::GAUSSIAN_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), floatImage.size());
}

// Test morphological operations with custom structuring element
TEST_F(FiltersTest, CustomStructuringElement) {
    FilterParams params;
    params.structElement = StructuringElement::CUSTOM;

    // Create a custom structuring element (cross shape)
    params.customKernel = {
        {0, 1, 0},
        {1, 1, 1},
        {0, 1, 0}
    };

    auto result = filter->applyMorphological(checkerboard_image, FilterType::EROSION,
                                           StructuringElement::CUSTOM, 3);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), checkerboard_image.size());
}

// Test frequency domain filter with different parameters
TEST_F(FiltersTest, FrequencyDomainParameterVariations) {
    std::vector<double> cutoffFreqs = {0.1, 0.3, 0.5, 0.7, 0.9};

    for (double cutoff : cutoffFreqs) {
        FilterParams params;
        params.cutoffFreq = cutoff;

        auto result = filter->applyFrequencyFilter(gradient_image, FilterType::LOW_PASS, params);

        EXPECT_GT(result.size(), 0);
        EXPECT_EQ(result.size(), gradient_image.size());
    }
}

// Test notch filter
TEST_F(FiltersTest, NotchFilter) {
    FilterParams params;
    params.cutoffFreq = 0.3;
    params.bandwidth = 0.05;

    auto result = filter->applyFrequencyFilter(gradient_image, FilterType::NOTCH, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), gradient_image.size());
}

// Test Wiener filter for noise reduction
TEST_F(FiltersTest, WienerFilter) {
    FilterParams params;
    params.sigma = 2.0; // Noise variance estimate

    auto result = filter->applyFilter(noisy_image, FilterType::WIENER, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), noisy_image.size());
}

// Test radial blur filter
TEST_F(FiltersTest, RadialBlurFilter) {
    FilterParams params;
    params.strength = 0.5;
    params.kernelSize = 7;

    auto result = filter->applyFilter(circle_image, FilterType::RADIAL_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), circle_image.size());
}

// Test filter robustness with extreme parameters
TEST_F(FiltersTest, ExtremeParameters) {
    FilterParams params;

    // Test with very small sigma
    params.sigma = 0.1;
    params.kernelSize = 3;
    auto result1 = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params);
    EXPECT_GT(result1.size(), 0);

    // Test with very large sigma
    params.sigma = 10.0;
    params.kernelSize = 31; // Large kernel for large sigma
    auto result2 = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params);
    EXPECT_GT(result2.size(), 0);

    // Test with maximum strength
    params.strength = 10.0;
    auto result3 = filter->applyFilter(gradient_image, FilterType::SHARPEN, params);
    EXPECT_GT(result3.size(), 0);
}

// Test concurrent filter operations
TEST_F(FiltersTest, ConcurrentFilterOperations) {
    const int numThreads = 4;
    const int operationsPerThread = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    FilterParams params;
    params.sigma = 1.0;
    params.kernelSize = 3;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, &params, &successCount, &errorCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    auto result = filter->applyFilter(gradient_image, FilterType::GAUSSIAN_BLUR, params);
                    if (result.size() == gradient_image.size()) {
                        successCount.fetch_add(1);
                    } else {
                        errorCount.fetch_add(1);
                    }
                } catch (...) {
                    errorCount.fetch_add(1);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * operationsPerThread);
    EXPECT_EQ(errorCount.load(), 0);
}

// Test memory efficiency with large images
TEST_F(FiltersTest, DISABLED_MemoryEfficiencyTest) {
    // Create a large test image
    auto largeData = TestDataGenerator::generateGradientImage(512, 512, 1);
    blob largeImage(largeData.data(), largeData.size());

    FilterParams params;
    params.sigma = 2.0;
    params.kernelSize = 7;

    // Test that large image filtering doesn't cause memory issues
    auto result = filter->applyFilter(largeImage, FilterType::GAUSSIAN_BLUR, params);

    EXPECT_GT(result.size(), 0);
    EXPECT_EQ(result.size(), largeImage.size());
}

} // namespace atom::image::test
