/*
 * convolution_2d.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Two-dimensional convolution and deconvolution operations
with padding utilities and optional OpenCL support.

Note: This is the full-featured 2D convolution pipeline operating on
std::vector<std::vector<T>> matrices with configurable padding, stride,
multi-threading, and DFT-based fast convolution.

For simple pixel-level convolution on flat std::span<T> images, see
graphics/convolution.hpp instead.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_CONVOLUTION_2D_HPP
#define ATOM_ALGORITHM_SIGNAL_CONVOLUTION_2D_HPP

#include "convolve_common.hpp"
#include "dft.hpp"

namespace atom::algorithm {

/**
 * @brief Performs 2D convolution of an input with a kernel
 *
 * @tparam T Type of the data
 * @param input 2D matrix to be convolved
 * @param kernel 2D kernel to convolve with
 * @param options Configuration options for the convolution
 * @return std::vector<std::vector<T>> Result of convolution
 */
template <ConvolutionNumeric T = f64>
auto convolve2D(const std::vector<std::vector<T>>& input,
                const std::vector<std::vector<T>>& kernel,
                const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

/**
 * @brief Performs 2D deconvolution (inverse of convolution)
 *
 * @tparam T Type of the data
 * @param signal 2D matrix signal (result of convolution)
 * @param kernel 2D kernel used for convolution
 * @param options Configuration options for the deconvolution
 * @return std::vector<std::vector<T>> Original input recovered via
 * deconvolution
 */
template <ConvolutionNumeric T = f64>
auto deconvolve2D(const std::vector<std::vector<T>>& signal,
                  const std::vector<std::vector<T>>& kernel,
                  const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto convolve2D(
    const std::vector<std::vector<f64>>& input,
    const std::vector<std::vector<f64>>& kernel,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;

auto deconvolve2D(
    const std::vector<std::vector<f64>>& signal,
    const std::vector<std::vector<f64>>& kernel,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;

/**
 * @brief Apply different types of padding to a 2D matrix
 *
 * @tparam T Type of the data
 * @param input Input matrix
 * @param padTop Number of rows to add at top
 * @param padBottom Number of rows to add at bottom
 * @param padLeft Number of columns to add at left
 * @param padRight Number of columns to add at right
 * @param mode Padding mode (zero, reflect, symmetric, etc.)
 * @return std::vector<std::vector<T>> Padded matrix
 */
template <ConvolutionNumeric T = f64>
auto pad2D(const std::vector<std::vector<T>>& input, usize padTop,
           usize padBottom, usize padLeft, usize padRight,
           PaddingMode mode = PaddingMode::SAME) -> std::vector<std::vector<T>>;

/**
 * @brief Get output dimensions after convolution operation
 *
 * @param inputHeight Height of input
 * @param inputWidth Width of input
 * @param kernelHeight Height of kernel
 * @param kernelWidth Width of kernel
 * @param strideY Vertical stride
 * @param strideX Horizontal stride
 * @param paddingMode Mode for handling boundaries
 * @return std::pair<usize, usize> Output dimensions (height, width)
 */
auto getConvolutionOutputDimensions(usize inputHeight, usize inputWidth,
                                    usize kernelHeight, usize kernelWidth,
                                    usize strideY = 1, usize strideX = 1,
                                    PaddingMode paddingMode = PaddingMode::SAME)
    -> std::pair<usize, usize>;

#if ATOM_USE_OPENCL
/**
 * @brief Performs 2D convolution using OpenCL acceleration
 *
 * @tparam T Type of the data
 * @param input 2D matrix to be convolved
 * @param kernel 2D kernel to convolve with
 * @param options Configuration options for the convolution
 * @return std::vector<std::vector<T>> Result of convolution
 */
template <ConvolutionNumeric T = f64>
auto convolve2DOpenCL(const std::vector<std::vector<T>>& input,
                      const std::vector<std::vector<T>>& kernel,
                      const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

/**
 * @brief Performs 2D deconvolution using OpenCL acceleration
 *
 * @tparam T Type of the data
 * @param signal 2D matrix signal (result of convolution)
 * @param kernel 2D kernel used for convolution
 * @param options Configuration options for the deconvolution
 * @return std::vector<std::vector<T>> Original input recovered via
 * deconvolution
 */
template <ConvolutionNumeric T = f64>
auto deconvolve2DOpenCL(const std::vector<std::vector<T>>& signal,
                        const std::vector<std::vector<T>>& kernel,
                        const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto convolve2DOpenCL(
    const std::vector<std::vector<f64>>& input,
    const std::vector<std::vector<f64>>& kernel,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;

auto deconvolve2DOpenCL(
    const std::vector<std::vector<f64>>& signal,
    const std::vector<std::vector<f64>>& kernel,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;
#endif

// Template implementations

template <ConvolutionNumeric T>
auto pad2D(const std::vector<std::vector<T>>& input, usize padTop,
           usize padBottom, usize padLeft, usize padRight, PaddingMode mode)
    -> std::vector<std::vector<T>> {
    if (input.empty()) {
        return {};
    }

    const usize inputRows = input.size();
    const usize inputCols = input[0].size();
    const usize outputRows = inputRows + padTop + padBottom;
    const usize outputCols = inputCols + padLeft + padRight;

    std::vector<std::vector<T>> result(outputRows,
                                       std::vector<T>(outputCols, T{0}));

    // Copy original data
    for (usize i = 0; i < inputRows; ++i) {
        for (usize j = 0; j < inputCols; ++j) {
            result[i + padTop][j + padLeft] = input[i][j];
        }
    }

    // Apply padding mode
    switch (mode) {
        case PaddingMode::VALID:
        case PaddingMode::SAME:
        case PaddingMode::FULL:
        default:
            // For simplicity, use zero padding for all modes
            // Already initialized with zeros
            break;
    }

    return result;
}

// Template implementations for convolve2D and deconvolve2D with
// ConvolutionOptions
template <ConvolutionNumeric T>
auto convolve2D(const std::vector<std::vector<T>>& input,
                const std::vector<std::vector<T>>& kernel,
                const ConvolutionOptions<T>& options)
    -> std::vector<std::vector<T>> {
    // For now, delegate to the legacy function that takes numThreads
    if constexpr (std::is_same_v<T, f64>) {
        return atom::algorithm::convolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(input),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(kernel),
            options.numThreads);
    } else {
        // Convert to f64, process, and convert back
        std::vector<std::vector<f64>> input_f64;
        input_f64.reserve(input.size());
        for (const auto& row : input) {
            input_f64.emplace_back(row.begin(), row.end());
        }

        std::vector<std::vector<f64>> kernel_f64;
        kernel_f64.reserve(kernel.size());
        for (const auto& row : kernel) {
            kernel_f64.emplace_back(row.begin(), row.end());
        }

        auto result_f64 = atom::algorithm::convolve2D(input_f64, kernel_f64,
                                                      options.numThreads);

        std::vector<std::vector<T>> result;
        result.reserve(result_f64.size());
        for (const auto& row : result_f64) {
            result.emplace_back(row.begin(), row.end());
        }
        return result;
    }
}

template <ConvolutionNumeric T>
auto deconvolve2D(const std::vector<std::vector<T>>& signal,
                  const std::vector<std::vector<T>>& kernel,
                  const ConvolutionOptions<T>& options)
    -> std::vector<std::vector<T>> {
    // For now, delegate to the legacy function that takes numThreads
    if constexpr (std::is_same_v<T, f64>) {
        return atom::algorithm::deconvolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(signal),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(kernel),
            options.numThreads);
    } else {
        // Convert to f64, process, and convert back
        std::vector<std::vector<f64>> signal_f64;
        signal_f64.reserve(signal.size());
        for (const auto& row : signal) {
            signal_f64.emplace_back(row.begin(), row.end());
        }

        std::vector<std::vector<f64>> kernel_f64;
        kernel_f64.reserve(kernel.size());
        for (const auto& row : kernel) {
            kernel_f64.emplace_back(row.begin(), row.end());
        }

        auto result_f64 = atom::algorithm::deconvolve2D(signal_f64, kernel_f64,
                                                        options.numThreads);

        std::vector<std::vector<T>> result;
        result.reserve(result_f64.size());
        for (const auto& row : result_f64) {
            result.emplace_back(row.begin(), row.end());
        }
        return result;
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_CONVOLUTION_2D_HPP
