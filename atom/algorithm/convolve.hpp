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
#if defined(__cpp_lib_experimental_parallel_simd) && USE_SIMD
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
    std::is_arithmetic_v<T> || std::is_same_v<T, std::complex<float>> ||
    std::is_same_v<T, std::complex<double>>;

/**
 * @brief Configuration options for convolution operations
 *
 * @tparam T Numeric type for convolution calculations
 */
template <ConvolutionNumeric T = double>
struct ConvolutionOptions {
    PaddingMode paddingMode = PaddingMode::SAME;  ///< Padding mode
    int strideX = 1;                              ///< Horizontal stride
    int strideY = 1;                              ///< Vertical stride
    int numThreads =
        std::thread::hardware_concurrency();  ///< Number of threads to use
    bool useOpenCL = false;  ///< Whether to use OpenCL if available
    bool useSIMD = true;     ///< Whether to use SIMD if available
    int tileSize = 32;       ///< Tile size for cache optimization
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
template <ConvolutionNumeric T = double>
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
template <ConvolutionNumeric T = double>
auto deconvolve2D(const std::vector<std::vector<T>>& signal,
                  const std::vector<std::vector<T>>& kernel,
                  const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto convolve2D(const std::vector<std::vector<double>>& input,
                const std::vector<std::vector<double>>& kernel,
                int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<double>>;

auto deconvolve2D(const std::vector<std::vector<double>>& signal,
                  const std::vector<std::vector<double>>& kernel,
                  int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<double>>;

/**
 * @brief Computes 2D Discrete Fourier Transform
 *
 * @tparam T Type of the input data
 * @param signal 2D input signal in spatial domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<std::complex<T>>> Frequency domain
 * representation
 */
template <ConvolutionNumeric T = double>
auto dfT2D(const std::vector<std::vector<T>>& signal,
           int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<std::complex<T>>>;

/**
 * @brief Computes inverse 2D Discrete Fourier Transform
 *
 * @tparam T Type of the data
 * @param spectrum 2D input in frequency domain
 * @param numThreads Number of threads to use (default: all available cores)
 * @return std::vector<std::vector<T>> Spatial domain representation
 */
template <ConvolutionNumeric T = double>
auto idfT2D(const std::vector<std::vector<std::complex<T>>>& spectrum,
            int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<T>>;

/**
 * @brief Generates a 2D Gaussian kernel for image filtering
 *
 * @tparam T Type of the kernel data
 * @param size Size of the kernel (should be odd)
 * @param sigma Standard deviation of the Gaussian distribution
 * @return std::vector<std::vector<T>> Gaussian kernel
 */
template <ConvolutionNumeric T = double>
auto generateGaussianKernel(int size, double sigma)
    -> std::vector<std::vector<T>>;

/**
 * @brief Applies a Gaussian filter to an image
 *
 * @tparam T Type of the image data
 * @param image Input image as 2D matrix
 * @param kernel Gaussian kernel to apply
 * @param options Configuration options for the filtering
 * @return std::vector<std::vector<T>> Filtered image
 */
template <ConvolutionNumeric T = double>
auto applyGaussianFilter(const std::vector<std::vector<T>>& image,
                         const std::vector<std::vector<T>>& kernel,
                         const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto dfT2D(const std::vector<std::vector<double>>& signal,
           int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<std::complex<double>>>;

auto idfT2D(const std::vector<std::vector<std::complex<double>>>& spectrum,
            int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<double>>;

auto generateGaussianKernel(int size, double sigma)
    -> std::vector<std::vector<double>>;

auto applyGaussianFilter(const std::vector<std::vector<double>>& image,
                         const std::vector<std::vector<double>>& kernel)
    -> std::vector<std::vector<double>>;

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
template <ConvolutionNumeric T = double>
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
template <ConvolutionNumeric T = double>
auto deconvolve2DOpenCL(const std::vector<std::vector<T>>& signal,
                        const std::vector<std::vector<T>>& kernel,
                        const ConvolutionOptions<T>& options = {})
    -> std::vector<std::vector<T>>;

// Legacy overloads for backward compatibility
auto convolve2DOpenCL(const std::vector<std::vector<double>>& input,
                      const std::vector<std::vector<double>>& kernel,
                      int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<double>>;

auto deconvolve2DOpenCL(const std::vector<std::vector<double>>& signal,
                        const std::vector<std::vector<double>>& kernel,
                        int numThreads = std::thread::hardware_concurrency())
    -> std::vector<std::vector<double>>;
#endif

/**
 * @brief Class providing static methods for applying various convolution
 * filters
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = double>
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
template <ConvolutionNumeric T = double>
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
    static auto convolve(const std::vector<T>& signal,
                         const std::vector<T>& kernel,
                         PaddingMode paddingMode = PaddingMode::SAME,
                         int stride = 1,
                         int numThreads = std::thread::hardware_concurrency())
        -> std::vector<T>;

    /**
     * @brief Perform 1D deconvolution (inverse of convolution)
     *
     * @param signal Input signal (result of convolution)
     * @param kernel Original convolution kernel
     * @param numThreads Number of threads to use
     * @return std::vector<T> Deconvolved signal
     */
    static auto deconvolve(const std::vector<T>& signal,
                           const std::vector<T>& kernel,
                           int numThreads = std::thread::hardware_concurrency())
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
template <ConvolutionNumeric T = double>
auto pad2D(const std::vector<std::vector<T>>& input, size_t padTop,
           size_t padBottom, size_t padLeft, size_t padRight,
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
 * @return std::pair<size_t, size_t> Output dimensions (height, width)
 */
auto getConvolutionOutputDimensions(size_t inputHeight, size_t inputWidth,
                                    size_t kernelHeight, size_t kernelWidth,
                                    size_t strideY = 1, size_t strideX = 1,
                                    PaddingMode paddingMode = PaddingMode::SAME)
    -> std::pair<size_t, size_t>;

/**
 * @brief Efficient class for working with convolution in frequency domain
 *
 * @tparam T Type of the data
 */
template <ConvolutionNumeric T = double>
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
    FrequencyDomainConvolution(size_t inputHeight, size_t inputWidth,
                               size_t kernelHeight, size_t kernelWidth);

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
    size_t padded_height_;
    size_t padded_width_;
    std::vector<std::vector<std::complex<T>>> frequency_space_buffer_;
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CONVOLVE_HPP
