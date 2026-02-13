/*
 * dft.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Two-dimensional Discrete Fourier Transform (DFT) and
Inverse Discrete Fourier Transform (IDFT) operations.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_DFT_HPP
#define ATOM_ALGORITHM_SIGNAL_DFT_HPP

#include "convolve_common.hpp"

namespace atom::algorithm {

/**
 * @brief Computes 2D Discrete Fourier Transform
 *
 * @tparam T Type of the input data
 * @param signal 2D input signal in spatial domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<std::complex<T>>> Frequency domain
 * representation
 */
template <ConvolutionNumeric T = f64>
auto dft2D(
    const std::vector<std::vector<T>>& signal,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<std::complex<T>>>;

/**
 * @brief Computes inverse 2D Discrete Fourier Transform
 *
 * @tparam T Type of the data
 * @param spectrum 2D input in frequency domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<T>> Spatial domain representation
 */
template <ConvolutionNumeric T = f64>
auto idft2D(
    const std::vector<std::vector<std::complex<T>>>& spectrum,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto dft2D(
    const std::vector<std::vector<f64>>& signal,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<std::complex<f64>>>;

auto idft2D(
    const std::vector<std::vector<std::complex<f64>>>& spectrum,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_DFT_HPP
