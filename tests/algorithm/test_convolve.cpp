#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <random>
#include <vector>

#include "atom/algorithm/convolve.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper functions for test data generation and verification
namespace {

// Generate random 2D matrix
auto generateRandomMatrix(size_t rows, size_t cols, double min = -100.0,
                          double max = 100.0)
    -> std::vector<std::vector<double>> {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(min, max);

    std::vector<std::vector<double>> matrix(rows, std::vector<double>(cols));
    for (auto& row : matrix) {
        for (auto& val : row) {
            val = dist(gen);
        }
    }
    return matrix;
}

// Compare two matrices with tolerance
bool matricesNearlyEqual(const std::vector<std::vector<double>>& a,
                         const std::vector<std::vector<double>>& b,
                         double tolerance = 1e-6) {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].size() != b[i].size())
            return false;
        for (size_t j = 0; j < a[i].size(); ++j) {
            if (std::abs(a[i][j] - b[i][j]) > tolerance) {
                return false;
            }
        }
    }
    return true;
}

}  // anonymous namespace

// Test fixture
class ConvolveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Simple test cases
    std::vector<std::vector<double>> identity_kernel{
        {0, 0, 0}, {0, 1, 0}, {0, 0, 0}};
    std::vector<std::vector<double>> edge_detection_kernel{
        {-1, -1, -1}, {-1, 8, -1}, {-1, -1, -1}};
    std::vector<std::vector<double>> simple_image{
        {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
};

// Basic functionality tests
TEST_F(ConvolveTest, IdentityKernelPreservesImage) {
    auto result = convolve2D(simple_image, identity_kernel);

    ASSERT_EQ(result.size(), simple_image.size());
    ASSERT_EQ(result[0].size(), simple_image[0].size());

    // Identity kernel should approximately preserve the original image
    // (excluding edge effects)
    EXPECT_NEAR(result[1][1], simple_image[1][1], 1e-6);
}

TEST_F(ConvolveTest, EdgeDetectionKernel) {
    auto result = convolve2D(simple_image, edge_detection_kernel);

    // Edge detection kernel should highlight the center for this simple matrix
    EXPECT_GT(std::abs(result[1][1]), 0.0);

    // The center value should be 8*5 - sum of surrounding pixels
    double expected_center =
        8 * simple_image[1][1] -
        (simple_image[0][0] + simple_image[0][1] + simple_image[0][2] +
         simple_image[1][0] + simple_image[1][2] + simple_image[2][0] +
         simple_image[2][1] + simple_image[2][2]);

    EXPECT_NEAR(result[1][1], expected_center, 1e-6);
}

TEST_F(ConvolveTest, GaussianKernelGeneration) {
    int size = 5;
    double sigma = 1.0;
    auto kernel = generateGaussianKernel(size, sigma);

    // Verify kernel dimensions
    ASSERT_EQ(kernel.size(), size);
    ASSERT_EQ(kernel[0].size(), size);

    // Gaussian kernel should be symmetric
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            EXPECT_NEAR(kernel[i][j], kernel[size - i - 1][size - j - 1],
                        1e-10);
        }
    }

    // Center should have the maximum value
    double center_value = kernel[size / 2][size / 2];
    for (const auto& row : kernel) {
        for (const auto& val : row) {
            EXPECT_LE(val, center_value + 1e-10);
        }
    }

    // Sum of all values should be approximately 1.0
    double sum = 0.0;
    for (const auto& row : kernel) {
        for (const auto& val : row) {
            sum += val;
        }
    }
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(ConvolveTest, GaussianFilterBlursImage) {
    auto kernel = generateGaussianKernel(5, 1.0);
    auto blurred = applyGaussianFilter(simple_image, kernel);

    ASSERT_EQ(blurred.size(), simple_image.size());
    ASSERT_EQ(blurred[0].size(), simple_image[0].size());

    // Center pixel should be a weighted average of surrounding pixels
    // So it should be closer to the mean than the original
    double original_center = simple_image[1][1];
    double blurred_center = blurred[1][1];
    double mean = 0.0;

    for (const auto& row : simple_image) {
        for (const auto& val : row) {
            mean += val;
        }
    }
    mean /= (simple_image.size() * simple_image[0].size());

    // The blurred value should be between the original and the mean
    double dist_orig_to_mean = std::abs(original_center - mean);
    double dist_blur_to_mean = std::abs(blurred_center - mean);

    EXPECT_LE(dist_blur_to_mean, dist_orig_to_mean);
}

// Edge case tests
TEST_F(ConvolveTest, EmptyInputThrowsException) {
    std::vector<std::vector<double>> empty_matrix;
    std::vector<std::vector<double>> empty_rows_matrix(3,
                                                       std::vector<double>());

    EXPECT_THROW(convolve2D(empty_matrix, identity_kernel),
                 std::invalid_argument);
    EXPECT_THROW(convolve2D(simple_image, empty_matrix), std::invalid_argument);
    EXPECT_THROW(convolve2D(empty_rows_matrix, identity_kernel),
                 std::invalid_argument);
}

TEST_F(ConvolveTest, NonUniformInputThrowsException) {
    std::vector<std::vector<double>> non_uniform{{1, 2, 3}, {4, 5}, {6, 7, 8}};

    EXPECT_THROW(convolve2D(non_uniform, identity_kernel),
                 std::invalid_argument);
    EXPECT_THROW(convolve2D(simple_image, non_uniform), std::invalid_argument);
}

// Multithreading tests
TEST_F(ConvolveTest, MultiThreadingProducesSameResults) {
    // Create larger test matrices
    auto large_image = generateRandomMatrix(20, 20);
    auto large_kernel = generateRandomMatrix(5, 5);

    // Single-threaded
    auto result_single = convolve2D(large_image, large_kernel, 1);

    // Multi-threaded with default thread count
    auto result_multi = convolve2D(large_image, large_kernel);

    // Results should be identical
    EXPECT_TRUE(matricesNearlyEqual(result_single, result_multi, 1e-6));

    // Explicit thread count
    auto result_explicit = convolve2D(large_image, large_kernel, 4);
    EXPECT_TRUE(matricesNearlyEqual(result_single, result_explicit, 1e-6));
}

TEST_F(ConvolveTest, NegativeThreadCountDefaultsToOne) {
    auto result_negative = convolve2D(simple_image, identity_kernel, -2);
    auto result_single = convolve2D(simple_image, identity_kernel, 1);

    EXPECT_TRUE(matricesNearlyEqual(result_negative, result_single));
}

// Deconvolution tests
TEST_F(ConvolveTest, BasicDeconvolution) {
    // Create a simple test case
    std::vector<std::vector<double>> original = {{1, 2}, {3, 4}};
    auto kernel = generateGaussianKernel(3, 1.0);

    // Convolve the original with the kernel
    auto convolved = convolve2D(original, kernel);

    // Deconvolve to get back the original
    auto deconvolved = deconvolve2D(convolved, kernel);

    // Size check
    ASSERT_EQ(deconvolved.size(), original.size());
    ASSERT_EQ(deconvolved[0].size(), original[0].size());

    // The deconvolution is an approximation, so we use a larger tolerance
    // Focus on checking if the pattern is preserved
    double original_ratio = original[1][1] / original[0][0];
    double deconvolved_ratio = deconvolved[1][1] / deconvolved[0][0];

    EXPECT_NEAR(original_ratio, deconvolved_ratio, 0.5);
}

TEST_F(ConvolveTest, DeconvolutionExceptions) {
    std::vector<std::vector<double>> empty_matrix;

    EXPECT_THROW(deconvolve2D(empty_matrix, identity_kernel),
                 std::invalid_argument);
    EXPECT_THROW(deconvolve2D(simple_image, empty_matrix),
                 std::invalid_argument);

    std::vector<std::vector<double>> non_uniform{{1, 2, 3}, {4, 5}, {6, 7, 8}};
    EXPECT_THROW(deconvolve2D(non_uniform, identity_kernel),
                 std::invalid_argument);
}

// Performance tests
TEST_F(ConvolveTest, ConvolutionPerformance) {
    // Create large matrices to test performance
    auto large_image = generateRandomMatrix(100, 100);
    auto kernel = generateGaussianKernel(5, 1.0);

    auto start = std::chrono::high_resolution_clock::now();
    auto result = convolve2D(large_image, kernel);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Convolution of 100x100 matrix took: " << duration << "ms"
              << std::endl;

    // Just verify the operation completed - this is more of a benchmark
    EXPECT_EQ(result.size(), large_image.size());
}

// DFT tests
TEST_F(ConvolveTest, DiscreteFourierTransform) {
    // Create a simple matrix with known frequencies
    std::vector<std::vector<double>> signal = {{1.0, 0.0, 1.0, 0.0},
                                               {0.0, 1.0, 0.0, 1.0},
                                               {1.0, 0.0, 1.0, 0.0},
                                               {0.0, 1.0, 0.0, 1.0}};

    // Apply DFT
    auto frequency = dfT2D(signal);

    // The DC component (0,0) should be the sum of all values
    double sum = 0.0;
    for (const auto& row : signal) {
        for (const auto& val : row) {
            sum += val;
        }
    }

    EXPECT_NEAR(frequency[0][0].real(), sum, 1e-6);
    EXPECT_NEAR(frequency[0][0].imag(), 0.0, 1e-6);

    // Apply inverse DFT
    auto reconstructed = idfT2D(frequency);

    // Check if we get back the original signal
    ASSERT_EQ(reconstructed.size(), signal.size());
    ASSERT_EQ(reconstructed[0].size(), signal[0].size());

    for (size_t i = 0; i < signal.size(); ++i) {
        for (size_t j = 0; j < signal[0].size(); ++j) {
            EXPECT_NEAR(reconstructed[i][j], signal[i][j], 1e-6);
        }
    }
}

// Test DFT and IDFT roundtrip
TEST_F(ConvolveTest, DFTRoundtrip) {
    // Generate random matrix
    auto original = generateRandomMatrix(8, 8, 0.0, 10.0);

    // Forward transform
    auto frequency = dfT2D(original);

    // Inverse transform
    auto reconstructed = idfT2D(frequency);

    // Check dimensions
    ASSERT_EQ(reconstructed.size(), original.size());
    ASSERT_EQ(reconstructed[0].size(), original[0].size());

    // Check values (allowing for small numerical errors)
    for (size_t i = 0; i < original.size(); ++i) {
        for (size_t j = 0; j < original[0].size(); ++j) {
            EXPECT_NEAR(reconstructed[i][j], original[i][j], 1e-5);
        }
    }
}

// End-to-end test
TEST_F(ConvolveTest, EndToEndConvolutionDeconvolution) {
    // Generate random matrices
    auto original = generateRandomMatrix(10, 10, 1.0, 10.0);
    auto kernel = generateGaussianKernel(5, 1.5);

    // Convolution
    auto convolved = convolve2D(original, kernel);

    // Deconvolution
    auto deconvolved = deconvolve2D(convolved, kernel);

    // Compare original and deconvolved
    // Since deconvolution is an approximation, we check correlations instead of
    // exact values
    double orig_mean = 0.0, deconv_mean = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        for (size_t j = 0; j < original[0].size(); ++j) {
            orig_mean += original[i][j];
            deconv_mean += deconvolved[i][j];
        }
    }
    orig_mean /= (original.size() * original[0].size());
    deconv_mean /= (deconvolved.size() * deconvolved[0].size());

    // Calculate correlation coefficient
    double numerator = 0.0, denom1 = 0.0, denom2 = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        for (size_t j = 0; j < original[0].size(); ++j) {
            double diff1 = original[i][j] - orig_mean;
            double diff2 = deconvolved[i][j] - deconv_mean;
            numerator += diff1 * diff2;
            denom1 += diff1 * diff1;
            denom2 += diff2 * diff2;
        }
    }
    double correlation = numerator / std::sqrt(denom1 * denom2);

    // We expect a strong positive correlation (above 0.5)
    EXPECT_GT(correlation, 0.5);
    std::cout << "Correlation between original and deconvolved: " << correlation
              << std::endl;
}

// Test multithreaded DFT
TEST_F(ConvolveTest, MultithreadedDFT) {
    auto signal = generateRandomMatrix(16, 16);

    // Single-threaded
    auto freq_single = dfT2D(signal, 1);

    // Multi-threaded
    auto freq_multi = dfT2D(signal, 4);

    // Results should be nearly identical
    for (size_t i = 0; i < freq_single.size(); ++i) {
        for (size_t j = 0; j < freq_single[0].size(); ++j) {
            EXPECT_NEAR(freq_single[i][j].real(), freq_multi[i][j].real(),
                        1e-5);
            EXPECT_NEAR(freq_single[i][j].imag(), freq_multi[i][j].imag(),
                        1e-5);
        }
    }
}

// Test multithreaded IDFT
TEST_F(ConvolveTest, MultithreadedIDFT) {
    auto signal = generateRandomMatrix(16, 16);
    auto frequency = dfT2D(signal);

    // Single-threaded
    auto recon_single = idfT2D(frequency, 1);

    // Multi-threaded
    auto recon_multi = idfT2D(frequency, 4);

    // Results should be nearly identical
    EXPECT_TRUE(matricesNearlyEqual(recon_single, recon_multi, 1e-5));
}
