/*
 * frequency_domain.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Frequency domain convolution class for efficient convolution
operations using Fourier transforms.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_FREQUENCY_DOMAIN_HPP
#define ATOM_ALGORITHM_SIGNAL_FREQUENCY_DOMAIN_HPP

#include "convolve_common.hpp"
#include "convolution_2d.hpp"

namespace atom::algorithm {

/**
 * @brief Efficient class for working with convolution in frequency domain
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = f64>
class FrequencyDomainConvolution {
public:
    /**
     * @brief Initialize with input and kernel dimensions
     *
     * @param inputHeight Height of input
     * @param inputWidth Width of input
     * @param kernelHeight Height of kernel
     * @param kernelWidth Width of kernel
     */
    FrequencyDomainConvolution(usize inputHeight, usize inputWidth,
                               usize kernelHeight, usize kernelWidth);

    /**
     * @brief Perform convolution in frequency domain
     *
     * @param input Input matrix
     * @param kernel Convolution kernel
     * @param options Configuration options
     * @return std::vector<std::vector<T>> Convolution result
     */
    auto convolve(const std::vector<std::vector<T>>& input,
                  const std::vector<std::vector<T>>& kernel,
                  const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;

private:
    usize padded_height_;
    usize padded_width_;
    std::vector<std::vector<std::complex<T>>> frequency_space_buffer_;
};

// Template implementations

template <ConvolutionNumeric T>
FrequencyDomainConvolution<T>::FrequencyDomainConvolution(usize inputHeight,
                                                          usize inputWidth,
                                                          usize kernelHeight,
                                                          usize kernelWidth)
    : padded_height_(inputHeight + kernelHeight - 1),
      padded_width_(inputWidth + kernelWidth - 1) {
    // Initialize frequency space buffer
    frequency_space_buffer_.resize(padded_height_);
    for (auto& row : frequency_space_buffer_) {
        row.resize(padded_width_);
    }
}

template <ConvolutionNumeric T>
auto FrequencyDomainConvolution<T>::convolve(
    const std::vector<std::vector<T>>& input,
    const std::vector<std::vector<T>>& kernel,
    const ConvolutionOptions<T>& options) -> std::vector<std::vector<T>> {
    // For now, delegate to the non-template function
    // This is a temporary implementation to fix compilation
    if constexpr (std::is_same_v<T, f64>) {
        return atom::algorithm::convolve2D(
            reinterpret_cast<const std::vector<std::vector<f64>>&>(input),
            reinterpret_cast<const std::vector<std::vector<f64>>&>(kernel),
            ConvolutionOptions<f64>{options.paddingMode, options.strideX,
                                    options.strideY, options.numThreads,
                                    options.useOpenCL, options.useSIMD,
                                    options.tileSize});
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

        auto result_f64 = atom::algorithm::convolve2D(
            input_f64, kernel_f64,
            ConvolutionOptions<f64>{options.paddingMode, options.strideX,
                                    options.strideY, options.numThreads,
                                    options.useOpenCL, options.useSIMD,
                                    options.tileSize});

        std::vector<std::vector<T>> result;
        result.reserve(result_f64.size());
        for (const auto& row : result_f64) {
            result.emplace_back(row.begin(), row.end());
        }
        return result;
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_FREQUENCY_DOMAIN_HPP
