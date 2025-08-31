#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <random>
#include <vector>
#include "atom/algorithm/convolve.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

namespace {

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

}  // namespace

class ConvolveTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
    std::vector<std::vector<double>> identity_kernel{
        {0, 0, 0}, {0, 1, 0}, {0, 0, 0}};
    std::vector<std::vector<double>> edge_detection_kernel{
        {-1, -1, -1}, {-1, 8, -1}, {-1, -1, -1}};
    std::vector<std::vector<double>> simple_image{
        {1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
};

TEST_F(ConvolveTest, IdentityKernelPreservesImage) {
    auto result = convolve2D(simple_image, identity_kernel);
    ASSERT_EQ(result.size(), simple_image.size());
    ASSERT_EQ(result[0].size(), simple_image[0].size());
    EXPECT_NEAR(result[1][1], simple_image[1][1], 1e-6);
}

TEST_F(ConvolveTest, EdgeDetectionKernel) {
    auto result = convolve2D(simple_image, edge_detection_kernel);
    EXPECT_GT(std::abs(result[1][1]), 0.0);
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
    ASSERT_EQ(kernel.size(), size);
    ASSERT_EQ(kernel[0].size(), size);
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            EXPECT_NEAR(kernel[i][j], kernel[size - i - 1][size - j - 1],
                        1e-10);
        }
    }
    double center_value = kernel[size / 2][size / 2];
    for (const auto& row : kernel) {
        for (const auto& val : row) {
            EXPECT_LE(val, center_value + 1e-10);
        }
    }
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
    double original_center = simple_image[1][1];
    double blurred_center = blurred[1][1];
    double mean = 0.0;
    for (const auto& row : simple_image) {
        for (const auto& val : row) {
            mean += val;
        }
    }
    mean /= (simple_image.size() * simple_image[0].size());
    double dist_orig_to_mean = std::abs(original_center - mean);
    double dist_blur_to_mean = std::abs(blurred_center - mean);
    EXPECT_LE(dist_blur_to_mean, dist_orig_to_mean);
}

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

TEST_F(ConvolveTest, MultiThreadingProducesSameResults) {
    auto large_image = generateRandomMatrix(20, 20);
    auto large_kernel = generateRandomMatrix(5, 5);
    auto result_single = convolve2D(large_image, large_kernel, 1);
    auto result_multi = convolve2D(large_image, large_kernel);
    EXPECT_TRUE(matricesNearlyEqual(result_single, result_multi, 1e-6));
    auto result_explicit = convolve2D(large_image, large_kernel, 4);
    EXPECT_TRUE(matricesNearlyEqual(result_single, result_explicit, 1e-6));
}

TEST_F(ConvolveTest, NegativeThreadCountDefaultsToOne) {
    auto result_negative = convolve2D(simple_image, identity_kernel, -2);
    auto result_single = convolve2D(simple_image, identity_kernel, 1);
    EXPECT_TRUE(matricesNearlyEqual(result_negative, result_single));
}

TEST_F(ConvolveTest, BasicDeconvolution) {
    std::vector<std::vector<double>> original = {{1, 2}, {3, 4}};
    auto kernel = generateGaussianKernel(3, 1.0);
    auto convolved = convolve2D(original, kernel);
    auto deconvolved = deconvolve2D(convolved, kernel);
    ASSERT_EQ(deconvolved.size(), original.size());
    ASSERT_EQ(deconvolved[0].size(), original[0].size());
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

TEST_F(ConvolveTest, ConvolutionPerformance) {
    auto large_image = generateRandomMatrix(100, 100);
    auto kernel = generateGaussianKernel(5, 1.0);
    auto start = std::chrono::high_resolution_clock::now();
    auto result = convolve2D(large_image, kernel);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Convolution of 100x100 matrix took: {}ms", duration);
    EXPECT_EQ(result.size(), large_image.size());
}

TEST_F(ConvolveTest, DiscreteFourierTransform) {
    std::vector<std::vector<double>> signal = {{1.0, 0.0, 1.0, 0.0},
                                               {0.0, 1.0, 0.0, 1.0},
                                               {1.0, 0.0, 1.0, 0.0},
                                               {0.0, 1.0, 0.0, 1.0}};
    auto frequency = dfT2D(signal);
    double sum = 0.0;
    for (const auto& row : signal) {
        for (const auto& val : row) {
            sum += val;
        }
    }
    EXPECT_NEAR(frequency[0][0].real(), sum, 1e-6);
    EXPECT_NEAR(frequency[0][0].imag(), 0.0, 1e-6);
    auto reconstructed = idfT2D(frequency);
    ASSERT_EQ(reconstructed.size(), signal.size());
    ASSERT_EQ(reconstructed[0].size(), signal[0].size());
    for (size_t i = 0; i < signal.size(); ++i) {
        for (size_t j = 0; j < signal[0].size(); ++j) {
            EXPECT_NEAR(reconstructed[i][j], signal[i][j], 1e-6);
        }
    }
}

TEST_F(ConvolveTest, DFTRoundtrip) {
    auto original = generateRandomMatrix(8, 8, 0.0, 10.0);
    auto frequency = dfT2D(original);
    auto reconstructed = idfT2D(frequency);
    ASSERT_EQ(reconstructed.size(), original.size());
    ASSERT_EQ(reconstructed[0].size(), original[0].size());
    for (size_t i = 0; i < original.size(); ++i) {
        for (size_t j = 0; j < original[0].size(); ++j) {
            EXPECT_NEAR(reconstructed[i][j], original[i][j], 1e-5);
        }
    }
}

TEST_F(ConvolveTest, EndToEndConvolutionDeconvolution) {
    auto original = generateRandomMatrix(10, 10, 1.0, 10.0);
    auto kernel = generateGaussianKernel(5, 1.5);
    auto convolved = convolve2D(original, kernel);
    auto deconvolved = deconvolve2D(convolved, kernel);
    double orig_mean = 0.0, deconv_mean = 0.0;
    for (size_t i = 0; i < original.size(); ++i) {
        for (size_t j = 0; j < original[0].size(); ++j) {
            orig_mean += original[i][j];
            deconv_mean += deconvolved[i][j];
        }
    }
    orig_mean /= (original.size() * original[0].size());
    deconv_mean /= (deconvolved.size() * deconvolved[0].size());
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
    EXPECT_GT(correlation, 0.5);
    spdlog::info("Correlation between original and deconvolved: {}",
                 correlation);
}

TEST_F(ConvolveTest, MultithreadedDFT) {
    auto signal = generateRandomMatrix(16, 16);
    auto freq_single = dfT2D(signal, 1);
    auto freq_multi = dfT2D(signal, 4);
    for (size_t i = 0; i < freq_single.size(); ++i) {
        for (size_t j = 0; j < freq_single[0].size(); ++j) {
            EXPECT_NEAR(freq_single[i][j].real(), freq_multi[i][j].real(),
                        1e-5);
            EXPECT_NEAR(freq_single[i][j].imag(), freq_multi[i][j].imag(),
                        1e-5);
        }
    }
}

TEST_F(ConvolveTest, MultithreadedIDFT) {
    auto signal = generateRandomMatrix(16, 16);
    auto frequency = dfT2D(signal);
    auto recon_single = idfT2D(frequency, 1);
    auto recon_multi = idfT2D(frequency, 4);
    EXPECT_TRUE(matricesNearlyEqual(recon_single, recon_multi, 1e-5));
}
