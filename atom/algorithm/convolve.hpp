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
#include <type_traits>
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"
#include "atom/error/exception.hpp"

// Define if OpenCL support is required
#ifndef ATOM_USE_OPENCL
#define ATOM_USE_OPENCL 0
#endif

// Define if SIMD support is required
#ifndef ATOM_USE_SIMD
#define ATOM_USE_SIMD 1
#endif

// Define if C++20 std::simd should be used (if available)
#if defined(__cpp_lib_experimental_parallel_simd) && ATOM_USE_SIMD
#include <experimental/simd>
#define ATOM_USE_STD_SIMD 1
#else
#define ATOM_USE_STD_SIMD 0
#endif

namespace atom::algorithm {
class ConvolveError : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_CONVOLVE_ERROR(...)                                        \
    throw atom::algorithm::ConvolveError(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                         ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Padding modes for convolution operations
 */
enum class PaddingMode {
    VALID,  ///< No padding, output size smaller than input
    SAME,   ///< Padding to keep output size same as input
    FULL    ///< Full padding, output size larger than input
};

/**
 * @brief Concept for numeric types that can be used in convolution operations
 */
template <typename T>
concept ConvolutionNumeric =
    std::is_arithmetic_v<T> || std::is_same_v<T, std::complex<f32>> ||
    std::is_same_v<T, std::complex<f64>>;

/**
 * @brief Configuration options for convolution operations
 *
 * @tparam T Numeric type for convolution calculations
 */
template <ConvolutionNumeric T = f64>
struct ConvolutionOptions {
    PaddingMode paddingMode = PaddingMode::SAME;  ///< Padding mode
    i32 strideX = 1;                              ///< Horizontal stride
    i32 strideY = 1;                              ///< Vertical stride
    i32 numThreads = static_cast<i32>(
        std::thread::hardware_concurrency());  ///< Number of threads to use
    bool useOpenCL = false;  ///< Whether to use OpenCL if available
    bool useSIMD = true;     ///< Whether to use SIMD if available
    i32 tileSize = 32;       ///< Tile size for cache optimization
};

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
 * @brief Computes 2D Discrete Fourier Transform
 *
 * @tparam T Type of the input data
 * @param signal 2D input signal in spatial domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<std::complex<T>>> Frequency domain
 * representation
 */
template <ConvolutionNumeric T = f64>
auto dfT2D(
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
auto idfT2D(
    const std::vector<std::vector<std::complex<T>>>& spectrum,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<T>>;

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
auto dfT2D(
    const std::vector<std::vector<f64>>& signal,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<std::complex<f64>>>;

auto idfT2D(
    const std::vector<std::vector<std::complex<f64>>>& spectrum,
    i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
    -> std::vector<std::vector<f64>>;

auto generateGaussianKernel(i32 size, f64 sigma)
    -> std::vector<std::vector<f64>>;

auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel)
    -> std::vector<std::vector<f64>>;

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

/**
 * @brief Class providing static methods for applying various convolution
 * filters
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = f64>
class ConvolutionFilters {
public:
    /**
     * @brief Apply a Sobel edge detection filter
     *
     * @param image Input image as 2D matrix
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Edge detection result
     */
    static auto applySobel(const std::vector<std::vector<T>>& image,
                           const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;

    /**
     * @brief Apply a Laplacian edge detection filter
     *
     * @param image Input image as 2D matrix
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Edge detection result
     */
    static auto applyLaplacian(const std::vector<std::vector<T>>& image,
                               const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;

    /**
     * @brief Apply a custom filter with the specified kernel
     *
     * @param image Input image as 2D matrix
     * @param kernel Custom convolution kernel
     * @param options Configuration options for the operation
     * @return std::vector<std::vector<T>> Filtered image
     */
    static auto applyCustomFilter(const std::vector<std::vector<T>>& image,
                                  const std::vector<std::vector<T>>& kernel,
                                  const ConvolutionOptions<T>& options = {})
        -> std::vector<std::vector<T>>;
};

/**
 * @brief Class for performing 1D convolution operations
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = f64>
class Convolution1D {
public:
    /**
     * @brief Perform 1D convolution
     *
     * @param signal Input signal as 1D vector
     * @param kernel Convolution kernel as 1D vector
     * @param paddingMode Mode to handle boundaries
     * @param stride Step size for convolution
     * @param numThreads Number of threads to use
     * @return std::vector<T> Result of convolution
     */
    static auto convolve(
        const std::vector<T>& signal, const std::vector<T>& kernel,
        PaddingMode paddingMode = PaddingMode::SAME, i32 stride = 1,
        i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
        -> std::vector<T>;

    /**
     * @brief Perform 1D deconvolution (inverse of convolution)
     *
     * @param signal Input signal (result of convolution)
     * @param kernel Original convolution kernel
     * @param numThreads Number of threads to use
     * @return std::vector<T> Deconvolved signal
     */
    static auto deconvolve(
        const std::vector<T>& signal, const std::vector<T>& kernel,
        i32 numThreads = static_cast<i32>(std::thread::hardware_concurrency()))
        -> std::vector<T>;
};

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

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CONVOLVE_HPP
