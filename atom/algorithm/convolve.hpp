/*
 * convolve.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Header for one-dimensional and two-dimensional convolution
and deconvolution with optional OpenCL support.

**************************************************/

#ifndef ATOM_ALGORITHM_CONVOLVE_HPP
#define ATOM_ALGORITHM_CONVOLVE_HPP

#include <complex>
#include <thread>
#include <vector>

// Define if OpenCL support is required
#ifndef USE_OPENCL
#define USE_OPENCL 0
#endif

// Define if SIMD support is required
#ifndef USE_SIMD
#define USE_SIMD 1
#endif

namespace atom::algorithm {

// Global constant for default thread usage
const int availableThreads = std::thread::hardware_concurrency();

/**
 * @brief Performs 2D convolution of an input with a kernel
 *
 * @param input 2D matrix to be convolved
 * @param kernel 2D kernel to convolve with
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<double>> Result of convolution
 */
auto convolve2D(const std::vector<std::vector<double>>& input,
                const std::vector<std::vector<double>>& kernel,
                int numThreads = availableThreads)
    -> std::vector<std::vector<double>>;

/**
 * @brief Performs 2D deconvolution (inverse of convolution)
 *
 * @param signal 2D matrix signal (result of convolution)
 * @param kernel 2D kernel used for convolution
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<double>> Original input recovered via
 * deconvolution
 */
auto deconvolve2D(const std::vector<std::vector<double>>& signal,
                  const std::vector<std::vector<double>>& kernel,
                  int numThreads = availableThreads)
    -> std::vector<std::vector<double>>;

/**
 * @brief Computes 2D Discrete Fourier Transform
 *
 * @param signal 2D input signal in spatial domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<std::complex<double>>> Frequency domain
 * representation
 */
auto dfT2D(const std::vector<std::vector<double>>& signal,
           int numThreads = availableThreads)
    -> std::vector<std::vector<std::complex<double>>>;

/**
 * @brief Computes inverse 2D Discrete Fourier Transform
 *
 * @param spectrum 2D input in frequency domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<double>> Spatial domain representation
 */
auto idfT2D(const std::vector<std::vector<std::complex<double>>>& spectrum,
            int numThreads = availableThreads)
    -> std::vector<std::vector<double>>;

/**
 * @brief Generates a 2D Gaussian kernel for image filtering
 *
 * @param size Size of the kernel (should be odd)
 * @param sigma Standard deviation of the Gaussian distribution
 * @return std::vector<std::vector<double>> Gaussian kernel
 */
auto generateGaussianKernel(int size,
                            double sigma) -> std::vector<std::vector<double>>;

/**
 * @brief Applies a Gaussian filter to an image
 *
 * @param image Input image as 2D matrix
 * @param kernel Gaussian kernel to apply
 * @return std::vector<std::vector<double>> Filtered image
 */
auto applyGaussianFilter(const std::vector<std::vector<double>>& image,
                         const std::vector<std::vector<double>>& kernel)
    -> std::vector<std::vector<double>>;

#if USE_OPENCL
/**
 * @brief Performs 2D convolution using OpenCL acceleration
 *
 * @param input 2D matrix to be convolved
 * @param kernel 2D kernel to convolve with
 * @param numThreads Used for fallback if OpenCL fails
 * @return std::vector<std::vector<double>> Result of convolution
 */
auto convolve2DOpenCL(const std::vector<std::vector<double>>& input,
                      const std::vector<std::vector<double>>& kernel,
                      int numThreads = availableThreads)
    -> std::vector<std::vector<double>>;

/**
 * @brief Performs 2D deconvolution using OpenCL acceleration
 *
 * @param signal 2D matrix signal (result of convolution)
 * @param kernel 2D kernel used for convolution
 * @param numThreads Used for fallback if OpenCL fails
 * @return std::vector<std::vector<double>> Original input recovered via
 * deconvolution
 */
auto deconvolve2DOpenCL(const std::vector<std::vector<double>>& signal,
                        const std::vector<std::vector<double>>& kernel,
                        int numThreads = availableThreads)
    -> std::vector<std::vector<double>>;
#endif

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CONVOLVE_HPP
