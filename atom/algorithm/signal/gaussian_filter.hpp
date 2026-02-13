/*
 * gaussian_filter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Gaussian kernel generation and Gaussian filter operations
for image processing.

Note: This operates on std::vector<std::vector<T>> matrices using the
full signal processing convolution pipeline.

For a simpler Gaussian blur on flat std::span<T> pixel arrays, see
graphics/convolution.hpp (Convolution::gaussianBlur) instead.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_GAUSSIAN_FILTER_HPP
#define ATOM_ALGORITHM_SIGNAL_GAUSSIAN_FILTER_HPP

#include "convolve_common.hpp"

namespace atom::algorithm {

/**
 * @brief Generates a 2D Gaussian kernel for image filtering
 *
 * @tparam T Type of the kernel data
 * @param size Size of the kernel (should be odd)
 * @param sigma Standard deviation of the Gaussian distribution
 * @return std::vector<std::vector<T>> Gaussian kernel
 */
template <ConvolutionNumeric T = f64>
auto generateGaussianKernel(i32 size, f64 sigma) -> std::vector<std::vector<T>>;

/**
 * @brief Applies a Gaussian filter to an image
 *
 * @tparam T Type of the image data
 * @param image Input image as 2D matrix
 * @param kernel Gaussian kernel to apply
 * @param options Configuration options for the filtering
 * @return std::vector<std::vector<T>> Filtered image
 */
template <ConvolutionNumeric T = f64>
auto applyGaussianFilter(const std::vector<std::vector<T>>& image,
                         const std::vector<std::vector<T>>& kernel,
                         const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto generateGaussianKernel(i32 size, f64 sigma)
    -> std::vector<std::vector<f64>>;

auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel)
    -> std::vector<std::vector<f64>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_GAUSSIAN_FILTER_HPP
