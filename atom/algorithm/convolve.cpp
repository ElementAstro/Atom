/*
 * convolve.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Implementation of one-dimensional and two-dimensional convolution
and deconvolution with optional OpenCL support.

**************************************************/

#include "convolve.hpp"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <cstring>
#include <numbers>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <vector>

#if USE_SIMD
#ifdef _MSC_VER
#include <intrin.h>
#define SIMD_ALIGNED __declspec(align(32))
#else
#include <x86intrin.h>
#define SIMD_ALIGNED __attribute__((aligned(32)))
#endif

#ifdef __AVX__
#define SIMD_ENABLED
#define SIMD_WIDTH 4
#elif defined(__SSE__)
#define SIMD_ENABLED
#define SIMD_WIDTH 2
#endif
#endif

#if USE_OPENCL
#include <CL/cl.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

// Code that might generate warnings

#ifdef __GNUC__
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "atom/error/exception.hpp"

namespace atom::algorithm {

// 常量定义
constexpr double EPSILON = 0.1;  // Prevent division by zero

// Function to convolve a 1D input with a kernel
auto convolve(const std::vector<double> &input,
              const std::vector<double> &kernel) -> std::vector<double> {
    const std::size_t inputSize = input.size();
    const std::size_t kernelSize = kernel.size();
    const std::size_t outputSize = inputSize + kernelSize - 1;
    std::vector<double> output(outputSize, 0.0);

#ifdef SIMD_ENABLED
    const int simdWidth = SIMD_WIDTH;
    std::vector<double> alignedKernel(kernelSize);
    std::memcpy(alignedKernel.data(), kernel.data(),
                kernelSize * sizeof(double));

    for (std::size_t i = 0; i < outputSize; i += simdWidth) {
        __m256d sum = _mm256_setzero_pd();

        for (std::size_t j = 0; j < kernelSize; ++j) {
            if (i >= j && (i - j + simdWidth) <= inputSize) {
                __m256d inputVec = _mm256_loadu_pd(&input[i - j]);
                __m256d kernelVal = _mm256_set1_pd(alignedKernel[j]);
                sum = _mm256_add_pd(sum, _mm256_mul_pd(inputVec, kernelVal));
            }
        }

        _mm256_storeu_pd(&output[i], sum);
    }

    // Handle remaining elements
    for (std::size_t i = (outputSize / simdWidth) * simdWidth; i < outputSize;
         ++i) {
        for (std::size_t j = 0; j < kernelSize; ++j) {
            if (i >= j && (i - j) < inputSize) {
                output[i] += input[i - j] * kernel[j];
            }
        }
    }
#else
    // Fallback to non-SIMD version
    for (std::size_t i = 0; i < outputSize; ++i) {
        for (std::size_t j = 0; j < kernelSize; ++j) {
            if (i >= j && (i - j) < inputSize) {
                output[i] += input[i - j] * kernel[j];
            }
        }
    }
#endif

    return output;
}

// Function to deconvolve a 1D input with a kernel
auto deconvolve(const std::vector<double> &input,
                const std::vector<double> &kernel) -> std::vector<double> {
    const std::size_t inputSize = input.size();
    const std::size_t kernelSize = kernel.size();
    if (kernelSize > inputSize) {
        THROW_INVALID_ARGUMENT("Kernel size cannot be larger than input size.");
    }

    const std::size_t outputSize = inputSize - kernelSize + 1;
    std::vector<double> output(outputSize, 0.0);

#ifdef SIMD_ENABLED
    const int simdWidth = SIMD_WIDTH;
    std::vector<double> alignedKernel(kernelSize);
    std::memcpy(alignedKernel.data(), kernel.data(),
                kernelSize * sizeof(double));

    for (std::size_t i = 0; i < outputSize; i += simdWidth) {
        __m256d sum = _mm256_setzero_pd();

        for (std::size_t j = 0; j < kernelSize; ++j) {
            __m256d inputVec = _mm256_loadu_pd(&input[i + j]);
            __m256d kernelVal = _mm256_set1_pd(alignedKernel[j]);
            sum = _mm256_add_pd(sum, _mm256_mul_pd(inputVec, kernelVal));
        }

        _mm256_storeu_pd(&output[i], sum);
    }

    // Handle remaining elements
    for (std::size_t i = (outputSize / simdWidth) * simdWidth; i < outputSize;
         ++i) {
        for (std::size_t j = 0; j < kernelSize; ++j) {
            output[i] += input[i + j] * kernel[j];
        }
    }
#else
    // Fallback to non-SIMD version
    for (std::size_t i = 0; i < outputSize; ++i) {
        for (std::size_t j = 0; j < kernelSize; ++j) {
            output[i] += input[i + j] * kernel[j];
        }
    }
#endif

    return output;
}

// Helper function to extend 2D vectors
template <typename T>
auto extend2D(const std::vector<std::vector<T>> &input, std::size_t newRows,
              std::size_t newCols) -> std::vector<std::vector<T>> {
    std::vector<std::vector<T>> extended(newRows, std::vector<T>(newCols, 0.0));
    const std::size_t inputRows = input.size();
    const std::size_t inputCols = input[0].size();
    for (std::size_t i = 0; i < inputRows; ++i) {
        for (std::size_t j = 0; j < inputCols; ++j) {
            extended[i + newRows / 2][j + newCols / 2] = input[i][j];
        }
    }
    return extended;
}

#if USE_OPENCL
// OpenCL initialization and helper functions
auto initializeOpenCL() -> cl_context {
    cl_uint numPlatforms;
    cl_platform_id platform = nullptr;
    clGetPlatformIDs(1, &platform, &numPlatforms);

    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM,
                                          (cl_context_properties)platform, 0};

    cl_int err;
    cl_context context = clCreateContextFromType(properties, CL_DEVICE_TYPE_GPU,
                                                 nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL context.");
    }
    return context;
}

auto createCommandQueue(cl_context context) -> cl_command_queue {
    cl_device_id device_id;
    clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device_id, nullptr);
    cl_int err;
    cl_command_queue commandQueue =
        clCreateCommandQueue(context, device_id, 0, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL command queue.");
    }
    return commandQueue;
}

auto createProgram(const std::string &source,
                   cl_context context) -> cl_program {
    const char *sourceStr = source.c_str();
    cl_int err;
    cl_program program =
        clCreateProgramWithSource(context, 1, &sourceStr, nullptr, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL program.");
    }
    return program;
}

void checkErr(cl_int err, const char *operation) {
    if (err != CL_SUCCESS) {
        std::string errMsg = "OpenCL Error during operation: ";
        errMsg += operation;
        THROW_RUNTIME_ERROR(errMsg.c_str());
    }
}

// OpenCL kernel code for 2D convolution
const std::string convolve2DKernelSrc = R"CLC(
__kernel void convolve2D(__global const float* input,
                         __global const float* kernel,
                         __global float* output,
                         const int inputRows,
                         const int inputCols,
                         const int kernelRows,
                         const int kernelCols) {
    int row = get_global_id(0);
    int col = get_global_id(1);

    int halfKernelRows = kernelRows / 2;
    int halfKernelCols = kernelCols / 2;

    float sum = 0.0f;
    for (int i = -halfKernelRows; i <= halfKernelRows; ++i) {
        for (int j = -halfKernelCols; j <= halfKernelCols; ++j) {
            int x = clamp(row + i, 0, inputRows - 1);
            int y = clamp(col + j, 0, inputCols - 1);
            sum += input[x * inputCols + y] * kernel[(i + halfKernelRows) * kernelCols + (j + halfKernelCols)];
        }
    }
    output[row * inputCols + col] = sum;
}
)CLC";

// Function to convolve a 2D input with a 2D kernel using OpenCL
auto convolve2DOpenCL(const std::vector<std::vector<double>> &input,
                      const std::vector<std::vector<double>> &kernel,
                      int numThreads) -> std::vector<std::vector<double>> {
    cl_context context = initializeOpenCL();
    cl_command_queue queue = createCommandQueue(context);

    const std::size_t inputRows = input.size();
    const std::size_t inputCols = input[0].size();
    const std::size_t kernelRows = kernel.size();
    const std::size_t kernelCols = kernel[0].size();

    std::vector<float> inputFlattened(inputRows * inputCols);
    std::vector<float> kernelFlattened(kernelRows * kernelCols);
    std::vector<float> outputFlattened(inputRows * inputCols, 0.0f);

    for (std::size_t i = 0; i < inputRows; ++i)
        for (std::size_t j = 0; j < inputCols; ++j)
            inputFlattened[i * inputCols + j] = static_cast<float>(input[i][j]);

    for (std::size_t i = 0; i < kernelRows; ++i)
        for (std::size_t j = 0; j < kernelCols; ++j)
            kernelFlattened[i * kernelCols + j] =
                static_cast<float>(kernel[i][j]);

    cl_int err;
    cl_mem inputBuffer = clCreateBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        sizeof(float) * inputFlattened.size(), inputFlattened.data(), &err);
    checkErr(err, "Creating input buffer");

    cl_mem kernelBuffer = clCreateBuffer(
        context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        sizeof(float) * kernelFlattened.size(), kernelFlattened.data(), &err);
    checkErr(err, "Creating kernel buffer");

    cl_mem outputBuffer =
        clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                       sizeof(float) * outputFlattened.size(), nullptr, &err);
    checkErr(err, "Creating output buffer");

    cl_program program = createProgram(convolve2DKernelSrc, context);
    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    checkErr(err, "Building program");

    cl_kernel openclKernel = clCreateKernel(program, "convolve2D", &err);
    checkErr(err, "Creating kernel");

    err = clSetKernelArg(openclKernel, 0, sizeof(cl_mem), &inputBuffer);
    err |= clSetKernelArg(openclKernel, 1, sizeof(cl_mem), &kernelBuffer);
    err |= clSetKernelArg(openclKernel, 2, sizeof(cl_mem), &outputBuffer);
    err |= clSetKernelArg(openclKernel, 3, sizeof(int), &inputRows);
    err |= clSetKernelArg(openclKernel, 4, sizeof(int), &inputCols);
    err |= clSetKernelArg(openclKernel, 5, sizeof(int), &kernelRows);
    err |= clSetKernelArg(openclKernel, 6, sizeof(int), &kernelCols);
    checkErr(err, "Setting kernel arguments");

    size_t globalWorkSize[2] = {inputRows, inputCols};
    err = clEnqueueNDRangeKernel(queue, openclKernel, 2, nullptr,
                                 globalWorkSize, nullptr, 0, nullptr, nullptr);
    checkErr(err, "Enqueueing kernel");

    err = clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0,
                              sizeof(float) * outputFlattened.size(),
                              outputFlattened.data(), 0, nullptr, nullptr);
    checkErr(err, "Reading back output buffer");

    // Convert output back to 2D vector
    std::vector<std::vector<double>> output(
        inputRows, std::vector<double>(inputCols, 0.0));
    for (std::size_t i = 0; i < inputRows; ++i)
        for (std::size_t j = 0; j < inputCols; ++j)
            output[i][j] =
                static_cast<double>(outputFlattened[i * inputCols + j]);

    // Clean up OpenCL resources
    clReleaseMemObject(inputBuffer);
    clReleaseMemObject(kernelBuffer);
    clReleaseMemObject(outputBuffer);
    clReleaseKernel(openclKernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return output;
}
#endif

// Function to convolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto convolve2D(const std::vector<std::vector<double>> &input,
                const std::vector<std::vector<double>> &kernel,
                int numThreads) -> std::vector<std::vector<double>> {
#if USE_OPENCL
    return convolve2DOpenCL(input, kernel, numThreads);
#else
    const std::size_t inputRows = input.size();
    const std::size_t inputCols = input[0].size();
    const std::size_t kernelRows = kernel.size();
    const std::size_t kernelCols = kernel[0].size();

    auto extendedInput =
        extend2D(input, inputRows + kernelRows - 1, inputCols + kernelCols - 1);
    auto extendedKernel = extend2D(kernel, inputRows + kernelRows - 1,
                                   inputCols + kernelCols - 1);

    std::vector<std::vector<double>> output(
        inputRows, std::vector<double>(inputCols, 0.0));

    // Function to compute a block of the convolution using SIMD
    auto computeBlock = [&](std::size_t blockStartRow,
                            std::size_t blockEndRow) {
#ifdef SIMD_ENABLED
        const int simdWidth = SIMD_WIDTH;
#endif
        std::vector<double> alignedKernel(kernelRows * kernelCols);
        for (std::size_t i = 0; i < kernelRows; ++i) {
            std::memcpy(&alignedKernel[i * kernelCols],
                        extendedKernel[i].data(), kernelCols * sizeof(double));
        }

#ifdef SIMD_ENABLED
        for (std::size_t i = blockStartRow; i < blockEndRow; ++i) {
            for (std::size_t j = kernelCols / 2; j < inputCols + kernelCols / 2;
                 j += simdWidth) {
                __m256d sum = _mm256_setzero_pd();

                for (std::size_t k = 0; k < kernelRows; ++k) {
                    for (std::size_t colOffset = 0; colOffset < kernelCols;
                         ++colOffset) {
                        __m256d inputVec = _mm256_loadu_pd(
                            &extendedInput[i + k - kernelRows / 2]
                                          [j + colOffset - kernelCols / 2]);
                        __m256d kernelVal = _mm256_set1_pd(
                            alignedKernel[k * kernelCols + colOffset]);
                        sum = _mm256_add_pd(sum,
                                            _mm256_mul_pd(inputVec, kernelVal));
                    }
                }

                _mm256_storeu_pd(
                    &output[i - kernelRows / 2][j - kernelCols / 2], sum);
            }

            // Handle remaining elements
            for (std::size_t j =
                     ((inputCols + kernelCols / 2) / simdWidth) * simdWidth +
                     kernelCols / 2;
                 j < inputCols + kernelCols / 2; ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < kernelRows; ++k) {
                    for (std::size_t colOffset = 0; colOffset < kernelCols;
                         ++colOffset) {
                        sum += extendedInput[i + k - kernelRows / 2]
                                            [j + colOffset - kernelCols / 2] *
                               alignedKernel[k * kernelCols + colOffset];
                    }
                }
                output[i - kernelRows / 2][j - kernelCols / 2] = sum;
            }
        }
#else
        // Fallback to non-SIMD version
        for (std::size_t i = blockStartRow; i < blockEndRow; ++i) {
            for (std::size_t j = kernelCols / 2; j < inputCols + kernelCols / 2;
                 ++j) {
                double sum = 0.0;
                for (std::size_t k = 0; k < kernelRows; ++k) {
                    for (std::size_t colOffset = 0; colOffset < kernelCols;
                         ++colOffset) {
                        sum += extendedInput[i + k - kernelRows / 2]
                                            [j + colOffset - kernelCols / 2] *
                               alignedKernel[k * kernelCols + colOffset];
                    }
                }
                output[i - kernelRows / 2][j - kernelCols / 2] = sum;
            }
        }
#endif
    };

    // Use multiple threads if requested
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        std::size_t blockSize = (inputRows + numThreads - 1) / numThreads;
        std::size_t blockStartRow = kernelRows / 2;

        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            std::size_t blockEndRow =
                std::min(blockStartRow + blockSize, inputRows + kernelRows / 2);
            threadPool.emplace_back(computeBlock, blockStartRow, blockEndRow);
            blockStartRow = blockEndRow;
        }

        // Threads are joined automatically by jthread destructor
    } else {
        // Single-threaded execution
        computeBlock(kernelRows / 2, inputRows + kernelRows / 2);
    }

    return output;
#endif
}

// Function to deconvolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto deconvolve2D(const std::vector<std::vector<double>> &signal,
                  const std::vector<std::vector<double>> &kernel,
                  int numThreads) -> std::vector<std::vector<double>> {
#if USE_OPENCL
    // Implement OpenCL support if necessary
    return deconvolve2DOpenCL(signal, kernel, numThreads);
#else
    const std::size_t signalRows = signal.size();
    const std::size_t signalCols = signal[0].size();
    const std::size_t kernelRows = kernel.size();
    const std::size_t kernelCols = kernel[0].size();

    auto extendedSignal = extend2D(signal, signalRows + kernelRows - 1,
                                   signalCols + kernelCols - 1);
    auto extendedKernel = extend2D(kernel, signalRows + kernelRows - 1,
                                   signalCols + kernelCols - 1);

    auto discreteFourierTransform2D =
        [&](const std::vector<std::vector<double>> &input) {
            return dfT2D(input,
                         numThreads);  // Assume DFT2D supports multithreading
        };

    auto frequencySignal = discreteFourierTransform2D(extendedSignal);
    auto frequencyKernel = discreteFourierTransform2D(extendedKernel);

    std::vector<std::vector<std::complex<double>>> frequencyProduct(
        signalRows + kernelRows - 1,
        std::vector<std::complex<double>>(signalCols + kernelCols - 1, {0, 0}));

    // SIMD-optimized computation of frequencyProduct
#ifdef SIMD_ENABLED
    const int simdWidth = SIMD_WIDTH;
    __m256d epsilon_vec = _mm256_set1_pd(EPSILON);

    for (std::size_t u = 0; u < signalRows + kernelRows - 1; ++u) {
        for (std::size_t v = 0; v < signalCols + kernelCols - 1;
             v += simdWidth) {
            __m256d kernelReal = _mm256_loadu_pd(&frequencyKernel[u][v].real());
            __m256d kernelImag = _mm256_loadu_pd(&frequencyKernel[u][v].imag());

            __m256d magnitude = _mm256_sqrt_pd(
                _mm256_add_pd(_mm256_mul_pd(kernelReal, kernelReal),
                              _mm256_mul_pd(kernelImag, kernelImag)));
            __m256d mask = _mm256_cmp_pd(magnitude, epsilon_vec, _CMP_GT_OQ);

            __m256d norm = _mm256_add_pd(_mm256_mul_pd(kernelReal, kernelReal),
                                         _mm256_mul_pd(kernelImag, kernelImag));
            norm = _mm256_add_pd(norm, epsilon_vec);

            __m256d normalizedReal = _mm256_div_pd(kernelReal, norm);
            __m256d normalizedImag = _mm256_div_pd(
                _mm256_xor_pd(kernelImag, _mm256_set1_pd(-0.0)), norm);

            normalizedReal = _mm256_blendv_pd(kernelReal, normalizedReal, mask);
            normalizedImag = _mm256_blendv_pd(kernelImag, normalizedImag, mask);

            _mm256_storeu_pd(&frequencyProduct[u][v].real(), normalizedReal);
            _mm256_storeu_pd(&frequencyProduct[u][v].imag(), normalizedImag);
        }

        // Handle remaining elements
        for (std::size_t v =
                 ((signalCols + kernelCols - 1) / simdWidth) * simdWidth;
             v < signalCols + kernelCols - 1; ++v) {
            if (std::abs(frequencyKernel[u][v]) > EPSILON) {
                frequencyProduct[u][v] =
                    std::conj(frequencyKernel[u][v]) /
                    (std::norm(frequencyKernel[u][v]) + EPSILON);
            } else {
                frequencyProduct[u][v] = std::conj(frequencyKernel[u][v]);
            }
        }
    }
#else
    // Fallback to non-SIMD version
    for (std::size_t u = 0; u < signalRows + kernelRows - 1; ++u) {
        for (std::size_t v = 0; v < signalCols + kernelCols - 1; ++v) {
            if (std::abs(frequencyKernel[u][v]) > EPSILON) {
                frequencyProduct[u][v] =
                    std::conj(frequencyKernel[u][v]) /
                    (std::norm(frequencyKernel[u][v]) + EPSILON);
            } else {
                frequencyProduct[u][v] = std::conj(frequencyKernel[u][v]);
            }
        }
    }
#endif

    std::vector<std::vector<double>> frequencyInverse =
        idfT2D(frequencyProduct, numThreads);

    std::vector<std::vector<double>> result(
        signalRows, std::vector<double>(signalCols, 0.0));
    for (std::size_t i = 0; i < signalRows; ++i) {
        for (std::size_t j = 0; j < signalCols; ++j) {
            result[i][j] = frequencyInverse[i][j] / (signalRows * signalCols);
        }
    }

    return result;
#endif
}

// 2D Discrete Fourier Transform (2D DFT)
auto dfT2D(const std::vector<std::vector<double>> &signal,
           int numThreads) -> std::vector<std::vector<std::complex<double>>> {
    const std::size_t M = signal.size();
    const std::size_t N = signal[0].size();
    std::vector<std::vector<std::complex<double>>> frequency(
        M, std::vector<std::complex<double>>(N, {0, 0}));

    // Lambda function to compute the DFT for a block of rows
    auto computeDFT = [&](std::size_t startRow, std::size_t endRow) {
        for (std::size_t u = startRow; u < endRow; ++u) {
            for (std::size_t v = 0; v < N; ++v) {
#ifdef SIMD_ENABLED
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();
                for (std::size_t m = 0; m < M; ++m) {
                    for (std::size_t n = 0; n < N; n += SIMD_WIDTH) {
                        __m256d theta = _mm256_set_pd(
                            -2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 3)) /
                                     static_cast<double>(N)),
                            -2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 2)) /
                                     static_cast<double>(N)),
                            -2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 1)) /
                                     static_cast<double>(N)),
                            -2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * n) /
                                     static_cast<double>(N)));
                        __m256d wReal = _mm256_cos_pd(theta);
                        __m256d wImag = _mm256_sin_pd(theta);
                        __m256d signalVal = _mm256_loadu_pd(&signal[m][n]);

                        sumReal = _mm256_fmadd_pd(signalVal, wReal, sumReal);
                        sumImag = _mm256_fmadd_pd(signalVal, wImag, sumImag);
                    }
                }
                // Assuming _mm256_reduce_add_pd is defined or use an
                // alternative
                double realPart =
                    _mm256_hadd_pd(sumReal, sumReal).m256d_f64[0] +
                    _mm256_hadd_pd(sumReal, sumReal).m256d_f64[2];
                double imagPart =
                    _mm256_hadd_pd(sumImag, sumImag).m256d_f64[0] +
                    _mm256_hadd_pd(sumImag, sumImag).m256d_f64[2];
                frequency[u][v] = std::complex<double>(realPart, imagPart);
#else
                std::complex<double> sum(0, 0);
                for (std::size_t m = 0; m < M; ++m) {
                    for (std::size_t n = 0; n < N; ++n) {
                        double theta = -2 * std::numbers::pi *
                                       ((static_cast<double>(u) * m) /
                                            static_cast<double>(M) +
                                        (static_cast<double>(v) * n) /
                                            static_cast<double>(N));
                        std::complex<double> w(std::cos(theta),
                                               std::sin(theta));
                        sum += signal[m][n] * w;
                    }
                }
                frequency[u][v] = sum;
#endif
            }
        }
    };

    // Multithreading support
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        std::size_t rowsPerThread = M / numThreads;
        std::size_t blockStartRow = 0;

        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            std::size_t blockEndRow = (threadIndex == numThreads - 1)
                                          ? M
                                          : blockStartRow + rowsPerThread;
            threadPool.emplace_back(computeDFT, blockStartRow, blockEndRow);
            blockStartRow = blockEndRow;
        }

        // Threads are joined automatically by jthread destructor
    } else {
        // Single-threaded execution
        computeDFT(0, M);
    }

    return frequency;
}

// 2D Inverse Discrete Fourier Transform (2D IDFT)
auto idfT2D(const std::vector<std::vector<std::complex<double>>> &spectrum,
            int numThreads) -> std::vector<std::vector<double>> {
    const std::size_t M = spectrum.size();
    const std::size_t N = spectrum[0].size();
    std::vector<std::vector<double>> spatial(M, std::vector<double>(N, 0.0));

    // Lambda function to compute the IDFT for a block of rows
    auto computeIDFT = [&](std::size_t startRow, std::size_t endRow) {
        for (std::size_t m = startRow; m < endRow; ++m) {
            for (std::size_t n = 0; n < N; ++n) {
#ifdef SIMD_ENABLED
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();
                for (std::size_t u = 0; u < M; ++u) {
                    for (std::size_t v = 0; v < N; v += SIMD_WIDTH) {
                        __m256d theta = _mm256_set_pd(
                            2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 3)) /
                                     static_cast<double>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 2)) /
                                     static_cast<double>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * (n + 1)) /
                                     static_cast<double>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<double>(u) * m) /
                                     static_cast<double>(M) +
                                 (static_cast<double>(v) * n) /
                                     static_cast<double>(N)));
                        __m256d wReal = _mm256_cos_pd(theta);
                        __m256d wImag = _mm256_sin_pd(theta);
                        __m256d spectrumReal =
                            _mm256_loadu_pd(&spectrum[u][v].real());
                        __m256d spectrumImag =
                            _mm256_loadu_pd(&spectrum[u][v].imag());

                        sumReal = _mm256_fmadd_pd(spectrumReal, wReal, sumReal);
                        sumImag = _mm256_fmadd_pd(spectrumImag, wImag, sumImag);
                    }
                }
                // Assuming _mm256_reduce_add_pd is defined or use an
                // alternative
                double realPart =
                    _mm256_hadd_pd(sumReal, sumReal).m256d_f64[0] +
                    _mm256_hadd_pd(sumReal, sumReal).m256d_f64[2];
                double imagPart =
                    _mm256_hadd_pd(sumImag, sumImag).m256d_f64[0] +
                    _mm256_hadd_pd(sumImag, sumImag).m256d_f64[2];
                spatial[m][n] =
                    (realPart + imagPart) /
                    (static_cast<double>(M) * static_cast<double>(N));
#else
                std::complex<double> sum(0.0, 0.0);
                for (std::size_t u = 0; u < M; ++u) {
                    for (std::size_t v = 0; v < N; ++v) {
                        double theta = 2 * std::numbers::pi *
                                       ((static_cast<double>(u) * m) /
                                            static_cast<double>(M) +
                                        (static_cast<double>(v) * n) /
                                            static_cast<double>(N));
                        std::complex<double> w(std::cos(theta),
                                               std::sin(theta));
                        sum += spectrum[u][v] * w;
                    }
                }
                spatial[m][n] = std::real(sum) / (static_cast<double>(M) *
                                                  static_cast<double>(N));
#endif
            }
        }
    };

    // Multithreading support
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        std::size_t rowsPerThread = M / numThreads;
        std::size_t blockStartRow = 0;

        for (int threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            std::size_t blockEndRow = (threadIndex == numThreads - 1)
                                          ? M
                                          : blockStartRow + rowsPerThread;
            threadPool.emplace_back(computeIDFT, blockStartRow, blockEndRow);
            blockStartRow = blockEndRow;
        }

        // Threads are joined automatically by jthread destructor
    } else {
        // Single-threaded execution
        computeIDFT(0, M);
    }

    return spatial;
}

// Function to generate a Gaussian kernel
auto generateGaussianKernel(int size,
                            double sigma) -> std::vector<std::vector<double>> {
    std::vector<std::vector<double>> kernel(size, std::vector<double>(size));
    double sum = 0.0;
    int center = size / 2;

#ifdef SIMD_ENABLED
    SIMD_ALIGNED double tempBuffer[SIMD_WIDTH];
    __m256d sigmaVec = _mm256_set1_pd(sigma);
    __m256d twoSigmaSquared =
        _mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(sigmaVec, sigmaVec));
    __m256d scale = _mm256_div_pd(
        _mm256_set1_pd(1.0),
        _mm256_mul_pd(_mm256_set1_pd(2 * std::numbers::pi), twoSigmaSquared));

    for (int i = 0; i < size; ++i) {
        __m256d iVec = _mm256_set1_pd(static_cast<double>(i - center));
        for (int j = 0; j < size; j += SIMD_WIDTH) {
            __m256d jVec = _mm256_set_pd(static_cast<double>(j + 3 - center),
                                         static_cast<double>(j + 2 - center),
                                         static_cast<double>(j + 1 - center),
                                         static_cast<double>(j - center));

            __m256d xSquared = _mm256_mul_pd(iVec, iVec);
            __m256d ySquared = _mm256_mul_pd(jVec, jVec);
            __m256d exponent = _mm256_div_pd(_mm256_add_pd(xSquared, ySquared),
                                             twoSigmaSquared);
            __m256d kernelValues = _mm256_mul_pd(
                scale,
                _mm256_exp_pd(_mm256_mul_pd(_mm256_set1_pd(-0.5), exponent)));

            _mm256_store_pd(tempBuffer, kernelValues);
            for (int k = 0; k < SIMD_WIDTH && (j + k) < size; ++k) {
                kernel[i][j + k] = tempBuffer[k];
                sum += tempBuffer[k];
            }
        }
    }

    // Normalize to ensure the sum of the weights is 1
    __m256d sumVec = _mm256_set1_pd(sum);
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; j += SIMD_WIDTH) {
            __m256d kernelValues = _mm256_loadu_pd(&kernel[i][j]);
            kernelValues = _mm256_div_pd(kernelValues, sumVec);
            _mm256_storeu_pd(&kernel[i][j], kernelValues);
        }
    }
#else
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            kernel[i][j] =
                std::exp(-0.5 * (std::pow((i - center) / sigma, 2.0) +
                                 std::pow((j - center) / sigma, 2.0))) /
                (2 * std::numbers::pi * sigma * sigma);
            sum += kernel[i][j];
        }
    }

    // Normalize to ensure the sum of the weights is 1
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            kernel[i][j] /= sum;
        }
    }
#endif

    return kernel;
}

// Function to apply Gaussian filter to an image
auto applyGaussianFilter(const std::vector<std::vector<double>> &image,
                         const std::vector<std::vector<double>> &kernel)
    -> std::vector<std::vector<double>> {
    const std::size_t imageHeight = image.size();
    const std::size_t imageWidth = image[0].size();
    const std::size_t kernelSize = kernel.size();
    const std::size_t kernelRadius = kernelSize / 2;
    std::vector<std::vector<double>> filteredImage(
        imageHeight, std::vector<double>(imageWidth, 0.0));

#ifdef SIMD_ENABLED
    SIMD_ALIGNED double tempBuffer[SIMD_WIDTH];

    for (std::size_t i = 0; i < imageHeight; ++i) {
        for (std::size_t j = 0; j < imageWidth; j += SIMD_WIDTH) {
            __m256d sumVec = _mm256_setzero_pd();

            for (std::size_t k = 0; k < kernelSize; ++k) {
                for (std::size_t l = 0; l < kernelSize; ++l) {
                    __m256d kernelVal = _mm256_set1_pd(
                        kernel[kernelRadius + k][kernelRadius + l]);

                    for (int m = 0; m < SIMD_WIDTH; ++m) {
                        int x = std::clamp(static_cast<int>(i + k), 0,
                                           static_cast<int>(imageHeight) - 1);
                        int y = std::clamp(static_cast<int>(j + l + m), 0,
                                           static_cast<int>(imageWidth) - 1);
                        tempBuffer[m] = image[x][y];
                    }

                    __m256d imageVal = _mm256_loadu_pd(tempBuffer);
                    sumVec = _mm256_add_pd(sumVec,
                                           _mm256_mul_pd(imageVal, kernelVal));
                }
            }

            _mm256_storeu_pd(tempBuffer, sumVec);
            for (int m = 0; m < SIMD_WIDTH && (j + m) < imageWidth; ++m) {
                filteredImage[i][j + m] = tempBuffer[m];
            }
        }
    }
#else
    for (std::size_t i = 0; i < imageHeight; ++i) {
        for (std::size_t j = 0; j < imageWidth; ++j) {
            double sum = 0.0;
            for (std::size_t k = 0; k < kernelSize; ++k) {
                for (std::size_t l = 0; l < kernelSize; ++l) {
                    int x = std::clamp(static_cast<int>(i + k), 0,
                                       static_cast<int>(imageHeight) - 1);
                    int y = std::clamp(static_cast<int>(j + l), 0,
                                       static_cast<int>(imageWidth) - 1);
                    sum += image[x][y] *
                           kernel[kernelRadius + k][kernelRadius + l];
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
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif