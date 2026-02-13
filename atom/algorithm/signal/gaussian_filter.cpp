/*
 * gaussian_filter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Implementation of Gaussian kernel generation and
Gaussian filter operations for image processing.

**************************************************/

#include "gaussian_filter.hpp"
#include "atom/algorithm/core/rust_numeric.hpp"

#include <cmath>
#include <numbers>
#include <vector>

#if ATOM_USE_SIMD && !ATOM_USE_STD_SIMD
#ifdef __SSE__
#include <immintrin.h>
#endif
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#pragma warning(disable : 4251)  // Needs to have dll-interface
#pragma warning(disable : 4275)  // Non dll-interface class used as base for
                                 // dll-interface class
#endif

namespace atom::algorithm {

// Function to generate a Gaussian kernel
auto generateGaussianKernel(i32 size, f64 sigma)
    -> std::vector<std::vector<f64>> {
    std::vector<std::vector<f64>> kernel(
        static_cast<usize>(size), std::vector<f64>(static_cast<usize>(size)));
    f64 sum = 0.0;
    i32 center = size / 2;

#ifdef ATOM_ATOM_USE_SIMD
    SIMD_ALIGNED f64 tempBuffer[SIMD_WIDTH];
    __m256d sigmaVec = _mm256_set1_pd(sigma);
    __m256d twoSigmaSquared =
        _mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(sigmaVec, sigmaVec));
    __m256d scale = _mm256_div_pd(
        _mm256_set1_pd(1.0),
        _mm256_mul_pd(_mm256_set1_pd(2 * std::numbers::pi), twoSigmaSquared));

    for (i32 i = 0; i < size; ++i) {
        __m256d iVec = _mm256_set1_pd(static_cast<f64>(i - center));
        for (i32 j = 0; j < size; j += SIMD_WIDTH) {
            __m256d jVec = _mm256_set_pd(static_cast<f64>(j + 3 - center),
                                         static_cast<f64>(j + 2 - center),
                                         static_cast<f64>(j + 1 - center),
                                         static_cast<f64>(j - center));

            __m256d xSquared = _mm256_mul_pd(iVec, iVec);
            __m256d ySquared = _mm256_mul_pd(jVec, jVec);
            __m256d exponent = _mm256_div_pd(_mm256_add_pd(xSquared, ySquared),
                                             twoSigmaSquared);
            __m256d kernelValues = _mm256_mul_pd(
                scale,
                _mm256_exp_pd(_mm256_mul_pd(_mm256_set1_pd(-0.5), exponent)));

            _mm256_store_pd(tempBuffer, kernelValues);
            for (i32 k = 0; k < SIMD_WIDTH && (j + k) < size; ++k) {
                kernel[static_cast<usize>(i)][static_cast<usize>(j + k)] =
                    tempBuffer[k];
                sum += tempBuffer[k];
            }
        }
    }

    // Normalize to ensure the sum of the weights is 1
    __m256d sumVec = _mm256_set1_pd(sum);
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; j += SIMD_WIDTH) {
            __m256d kernelValues = _mm256_loadu_pd(
                &kernel[static_cast<usize>(i)][static_cast<usize>(j)]);
            kernelValues = _mm256_div_pd(kernelValues, sumVec);
            _mm256_storeu_pd(
                &kernel[static_cast<usize>(i)][static_cast<usize>(j)],
                kernelValues);
        }
    }
#else
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; ++j) {
            kernel[static_cast<usize>(i)][static_cast<usize>(j)] =
                F64::exp(
                    -0.5 *
                    (F64::pow(static_cast<f64>(i - center) / sigma, 2.0) +
                     F64::pow(static_cast<f64>(j - center) / sigma, 2.0))) /
                (2 * std::numbers::pi * sigma * sigma);
            sum += kernel[static_cast<usize>(i)][static_cast<usize>(j)];
        }
    }

    // Normalize to ensure the sum of the weights is 1
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; ++j) {
            kernel[static_cast<usize>(i)][static_cast<usize>(j)] /= sum;
        }
    }
#endif

    return kernel;
}

// Function to apply Gaussian filter to an image
auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel)
    -> std::vector<std::vector<f64>> {
    const usize imageHeight = image.size();
    const usize imageWidth = image[0].size();
    const usize kernelSize = kernel.size();
    const usize kernelRadius = kernelSize / 2;
    std::vector<std::vector<f64>> filteredImage(
        imageHeight, std::vector<f64>(imageWidth, 0.0));

#ifdef ATOM_ATOM_USE_SIMD
    SIMD_ALIGNED f64 tempBuffer[SIMD_WIDTH];

    for (usize i = 0; i < imageHeight; ++i) {
        for (usize j = 0; j < imageWidth; j += SIMD_WIDTH) {
            __m256d sumVec = _mm256_setzero_pd();

            for (usize k = 0; k < kernelSize; ++k) {
                for (usize l = 0; l < kernelSize; ++l) {
                    __m256d kernelVal = _mm256_set1_pd(kernel[k][l]);

                    for (i32 m = 0; m < SIMD_WIDTH; ++m) {
                        // Center the kernel at position (i, j+m)
                        i32 x = I32::clamp(
                            static_cast<i32>(i) + static_cast<i32>(k) -
                                static_cast<i32>(kernelRadius),
                            0, static_cast<i32>(imageHeight) - 1);
                        i32 y = I32::clamp(static_cast<i32>(j) +
                                               static_cast<i32>(l) + m -
                                               static_cast<i32>(kernelRadius),
                                           0, static_cast<i32>(imageWidth) - 1);
                        tempBuffer[m] =
                            image[static_cast<usize>(x)][static_cast<usize>(y)];
                    }

                    __m256d imageVal = _mm256_loadu_pd(tempBuffer);
                    sumVec = _mm256_add_pd(sumVec,
                                           _mm256_mul_pd(imageVal, kernelVal));
                }
            }

            _mm256_storeu_pd(tempBuffer, sumVec);
            for (i32 m = 0;
                 m < SIMD_WIDTH && (j + static_cast<usize>(m)) < imageWidth;
                 ++m) {
                filteredImage[i][j + static_cast<usize>(m)] = tempBuffer[m];
            }
        }
    }
#else
    for (usize i = 0; i < imageHeight; ++i) {
        for (usize j = 0; j < imageWidth; ++j) {
            f64 sum = 0.0;
            for (usize k = 0; k < kernelSize; ++k) {
                for (usize l = 0; l < kernelSize; ++l) {
                    // Center the kernel at position (i, j)
                    i32 x =
                        I32::clamp(static_cast<i32>(i) + static_cast<i32>(k) -
                                       static_cast<i32>(kernelRadius),
                                   0, static_cast<i32>(imageHeight) - 1);
                    i32 y =
                        I32::clamp(static_cast<i32>(j) + static_cast<i32>(l) -
                                       static_cast<i32>(kernelRadius),
                                   0, static_cast<i32>(imageWidth) - 1);
                    sum += image[static_cast<usize>(x)][static_cast<usize>(y)] *
                           kernel[k][l];
                }
            }
            filteredImage[i][j] = sum;
        }
    }
#endif
    return filteredImage;
}

}  // namespace atom::algorithm

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif
