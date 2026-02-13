/*
 * convolve_common.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Common types, concepts, and error definitions for convolution
and signal processing operations.

**************************************************/

#ifndef ATOM_ALGORITHM_SIGNAL_CONVOLVE_COMMON_HPP
#define ATOM_ALGORITHM_SIGNAL_CONVOLVE_COMMON_HPP

#include <complex>
#include <thread>
#include <type_traits>
#include <vector>

#include "atom/algorithm/algorithm_exception.hpp"  // SignalException
#include "atom/algorithm/core/rust_numeric.hpp"

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

/**
 * @brief Exception class for convolution-related errors.
 *
 * Inherits from SignalException in the unified algorithm exception hierarchy.
 */
class ConvolveError : public SignalException {
public:
    using SignalException::SignalException;
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

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_SIGNAL_CONVOLVE_COMMON_HPP
