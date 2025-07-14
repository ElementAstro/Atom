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
#include "rust_numeric.hpp"

#include <algorithm>
#include <cmath>
#include <future>
#include <numbers>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#if ATOM_USE_SIMD && !ATOM_USE_STD_SIMD
#ifdef __SSE__
#include <immintrin.h>
#endif
#endif

// SIMD constants
#ifdef __AVX__
constexpr int SIMD_WIDTH = 4;  // 4 doubles per AVX register
#define SIMD_ALIGNED alignas(32)
#else
constexpr int SIMD_WIDTH = 1;  // Fallback for non-SIMD
#define SIMD_ALIGNED
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
// Constants and helper class definitions
constexpr f64 EPSILON = 1e-10;  // Prevent division by zero

// Validate matrix dimensions
template <typename T>
void validateMatrix(const std::vector<std::vector<T>>& matrix,
                    const std::string& name) {
    if (matrix.empty()) {
        THROW_CONVOLVE_ERROR("Empty matrix: {}", name);
    }

    const usize cols = matrix[0].size();
    if (cols == 0) {
        THROW_CONVOLVE_ERROR("Matrix {} has empty rows", name);
    }

    // Check if all rows have the same length
    for (usize i = 1; i < matrix.size(); ++i) {
        if (matrix[i].size() != cols) {
            THROW_CONVOLVE_ERROR("Matrix {} has inconsistent row lengths",
                                 name);
        }
    }
}

// Validate and adjust thread count
i32 validateAndAdjustThreadCount(i32 requestedThreads) {
    i32 availableThreads =
        static_cast<i32>(std::thread::hardware_concurrency());
    if (availableThreads == 0) {
        availableThreads = 1;  // Use at least one thread
    }

    if (requestedThreads <= 0) {
        return availableThreads;
    }

    if (requestedThreads > availableThreads) {
        return availableThreads;
    }

    return requestedThreads;
}

// Cache-friendly matrix structure
template <typename T>
class AlignedMatrix {
public:
    AlignedMatrix(usize rows, usize cols) : rows_(rows), cols_(cols) {
        // Allocate cache-line aligned memory
        const usize alignment = 64;  // Common cache line size
        usize size = rows * cols * sizeof(T);
        data_.resize(size);
    }

    AlignedMatrix(const std::vector<std::vector<T>>& input)
        : AlignedMatrix(input.size(), input[0].size()) {
        // Copy data
        for (usize i = 0; i < rows_; ++i) {
            for (usize j = 0; j < cols_; ++j) {
                at(i, j) = input[i][j];
            }
        }
    }

    T& at(usize row, usize col) {
        return *reinterpret_cast<T*>(&data_[sizeof(T) * (row * cols_ + col)]);
    }

    const T& at(usize row, usize col) const {
        return *reinterpret_cast<const T*>(
            &data_[sizeof(T) * (row * cols_ + col)]);
    }

    std::vector<std::vector<T>> toVector() const {
        std::vector<std::vector<T>> result(rows_, std::vector<T>(cols_));
        for (usize i = 0; i < rows_; ++i) {
            for (usize j = 0; j < cols_; ++j) {
                result[i][j] = at(i, j);
            }
        }
        return result;
    }

    usize rows() const { return rows_; }
    usize cols() const { return cols_; }

    T* data() { return reinterpret_cast<T*>(data_.data()); }
    const T* data() const { return reinterpret_cast<const T*>(data_.data()); }

private:
    usize rows_;
    usize cols_;
    std::vector<std::byte> data_;
};

// OpenCL resource management
#if ATOM_USE_OPENCL
template <typename T>
struct OpenCLReleaser {
    void operator()(cl_mem obj) const noexcept { clReleaseMemObject(obj); }
    void operator()(cl_program obj) const noexcept { clReleaseProgram(obj); }
    void operator()(cl_kernel obj) const noexcept { clReleaseKernel(obj); }
    void operator()(cl_context obj) const noexcept { clReleaseContext(obj); }
    void operator()(cl_command_queue obj) const noexcept {
        clReleaseCommandQueue(obj);
    }
};

// Smart pointers for OpenCL resources
using CLMemPtr =
    std::unique_ptr<std::remove_pointer_t<cl_mem>, OpenCLReleaser<cl_mem>>;
using CLProgramPtr = std::unique_ptr<std::remove_pointer_t<cl_program>,
                                     OpenCLReleaser<cl_program>>;
using CLKernelPtr = std::unique_ptr<std::remove_pointer_t<cl_kernel>,
                                    OpenCLReleaser<cl_kernel>>;
using CLContextPtr = std::unique_ptr<std::remove_pointer_t<cl_context>,
                                     OpenCLReleaser<cl_context>>;
using CLCmdQueuePtr = std::unique_ptr<std::remove_pointer_t<cl_command_queue>,
                                      OpenCLReleaser<cl_command_queue>>;
#endif

// Helper function to extend 2D vectors
template <typename T>
auto extend2D(const std::vector<std::vector<T>>& input, usize newRows,
              usize newCols) -> std::vector<std::vector<T>> {
    if (input.empty() || input[0].empty()) {
        THROW_CONVOLVE_ERROR("Input matrix cannot be empty");
    }
    if (newRows < input.size() || newCols < input[0].size()) {
        THROW_CONVOLVE_ERROR(
            "New dimensions must be greater than or equal to original "
            "dimensions");
    }

    std::vector<std::vector<T>> result(newRows, std::vector<T>(newCols, T{}));

    // Copy original data
    for (usize i = 0; i < input.size(); ++i) {
        if (input[i].size() != input[0].size()) {
            THROW_CONVOLVE_ERROR("Input matrix must have uniform column sizes");
        }
        std::copy(input[i].begin(), input[i].end(), result[i].begin());
    }

    return result;
}

// Helper function to extend 2D vectors with proper padding modes
template <typename T>
auto pad2D(const std::vector<std::vector<T>>& input, usize padTop,
           usize padBottom, usize padLeft, usize padRight, PaddingMode mode)
    -> std::vector<std::vector<T>> {
    if (input.empty() || input[0].empty()) {
        THROW_CONVOLVE_ERROR("Cannot pad empty matrix");
    }

    const usize inputRows = input.size();
    const usize inputCols = input[0].size();
    const usize outputRows = inputRows + padTop + padBottom;
    const usize outputCols = inputCols + padLeft + padRight;

    std::vector<std::vector<T>> output(outputRows, std::vector<T>(outputCols));

    // Implementation of different padding modes
    switch (mode) {
        case PaddingMode::VALID: {
            // In VALID mode, no padding is applied, just copy the original data
            for (usize i = 0; i < inputRows; ++i) {
                for (usize j = 0; j < inputCols; ++j) {
                    output[i + padTop][j + padLeft] = input[i][j];
                }
            }
            break;
        }

        case PaddingMode::SAME: {
            // For SAME mode, we pad the borders with zeros
            for (usize i = 0; i < inputRows; ++i) {
                for (usize j = 0; j < inputCols; ++j) {
                    output[i + padTop][j + padLeft] = input[i][j];
                }
            }
            break;
        }

        case PaddingMode::FULL: {
            // For FULL mode, we pad the borders with reflected values
            // Copy the original data
            for (usize i = 0; i < inputRows; ++i) {
                for (usize j = 0; j < inputCols; ++j) {
                    output[i + padTop][j + padLeft] = input[i][j];
                }
            }

            // Top border padding
            for (usize i = 0; i < padTop; ++i) {
                for (usize j = 0; j < outputCols; ++j) {
                    if (j < padLeft) {
                        // Top-left corner
                        output[padTop - 1 - i][padLeft - 1 - j] =
                            input[std::min(i, inputRows - 1)]
                                 [std::min(j, inputCols - 1)];
                    } else if (j >= padLeft + inputCols) {
                        // Top-right corner
                        output[padTop - 1 - i][j] =
                            input[std::min(i, inputRows - 1)][std::min(
                                inputCols - 1 - (j - (padLeft + inputCols)),
                                inputCols - 1)];
                    } else {
                        // Top edge
                        output[padTop - 1 - i][j] =
                            input[std::min(i, inputRows - 1)][j - padLeft];
                    }
                }
            }

            // Bottom border padding
            for (usize i = 0; i < padBottom; ++i) {
                for (usize j = 0; j < outputCols; ++j) {
                    if (j < padLeft) {
                        // Bottom-left corner
                        output[padTop + inputRows + i][j] =
                            input[std::max(0UL, inputRows - 1 - i)]
                                 [std::min(j, inputCols - 1)];
                    } else if (j >= padLeft + inputCols) {
                        // Bottom-right corner
                        output[padTop + inputRows + i][j] =
                            input[std::max(0UL, inputRows - 1 - i)]
                                 [std::max(0UL,
                                             inputCols - 1 -
                                                 (j - (padLeft + inputCols)))];
                    } else {
                        // Bottom edge
                        output[padTop + inputRows + i][j] = input[std::max(
                            0UL, inputRows - 1 - i)][j - padLeft];
                    }
                }
            }

            // Left border padding
            for (usize i = padTop; i < padTop + inputRows; ++i) {
                for (usize j = 0; j < padLeft; ++j) {
                    output[i][padLeft - 1 - j] =
                        input[i - padTop][std::min(j, inputCols - 1)];
                }
            }

            // Right border padding
            for (usize i = padTop; i < padTop + inputRows; ++i) {
                for (usize j = 0; j < padRight; ++j) {
                    output[i][padLeft + inputCols + j] =
                        input[i - padTop][std::max(0UL, inputCols - 1 - j)];
                }
            }

            break;
        }
    }

    return output;
}

// Helper function to get output dimensions for convolution
auto getConvolutionOutputDimensions(usize inputHeight, usize inputWidth,
                                    usize kernelHeight, usize kernelWidth,
                                    usize strideY, usize strideX,
                                    PaddingMode paddingMode)
    -> std::pair<usize, usize> {
    if (kernelHeight > inputHeight || kernelWidth > inputWidth) {
        THROW_CONVOLVE_ERROR(
            "Kernel dimensions ({},{}) cannot be larger than input dimensions "
            "({},{})",
            kernelHeight, kernelWidth, inputHeight, inputWidth);
    }

    usize outputHeight = 0;
    usize outputWidth = 0;

    switch (paddingMode) {
        case PaddingMode::VALID:
            outputHeight = (inputHeight - kernelHeight) / strideY + 1;
            outputWidth = (inputWidth - kernelWidth) / strideX + 1;
            break;

        case PaddingMode::SAME:
            outputHeight = (inputHeight + strideY - 1) / strideY;
            outputWidth = (inputWidth + strideX - 1) / strideX;
            break;

        case PaddingMode::FULL:
            outputHeight =
                (inputHeight + kernelHeight - 1 + strideY - 1) / strideY;
            outputWidth =
                (inputWidth + kernelWidth - 1 + strideX - 1) / strideX;
            break;
    }

    return {outputHeight, outputWidth};
}

#if ATOM_USE_OPENCL
// OpenCL initialization and helper functions
auto initializeOpenCL() -> CLContextPtr {
    cl_uint numPlatforms;
    cl_platform_id platform = nullptr;
    cl_int err = clGetPlatformIDs(1, &platform, &numPlatforms);

    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("Failed to get OpenCL platforms: error {}", err);
    }

    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM,
                                          (cl_context_properties)platform, 0};

    cl_context context = clCreateContextFromType(properties, CL_DEVICE_TYPE_GPU,
                                                 nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("Failed to create OpenCL context: error {}", err);
    }

    return CLContextPtr(context);
}

auto createCommandQueue(cl_context context) -> CLCmdQueuePtr {
    cl_device_id device_id;
    cl_int err =
        clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device_id, nullptr);
    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("Failed to get OpenCL device: error {}", err);
    }

    cl_command_queue commandQueue =
        clCreateCommandQueue(context, device_id, 0, &err);
    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("Failed to create OpenCL command queue: error {}",
                             err);
    }

    return CLCmdQueuePtr(commandQueue);
}

auto createProgram(const std::string& source, cl_context context)
    -> CLProgramPtr {
    const char* sourceStr = source.c_str();
    cl_int err;
    cl_program program =
        clCreateProgramWithSource(context, 1, &sourceStr, nullptr, &err);
    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("Failed to create OpenCL program: error {}", err);
    }

    return CLProgramPtr(program);
}

void checkErr(cl_int err, const char* operation) {
    if (err != CL_SUCCESS) {
        THROW_CONVOLVE_ERROR("OpenCL Error during {}: error {}", operation,
                             err);
    }
}

// OpenCL kernel code for 2D convolution - C++20风格改进
const std::string convolve2DKernelSrc = R"CLC(
#ifdef USE_DOUBLE
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
typedef double float_type;
#else
typedef float float_type;
#endif

__kernel void convolve2D(__global const float_type* input,
                         __global const float_type* kernel,
                         __global float_type* output,
                         const int inputRows,
                         const int inputCols,
                         const int kernelRows,
                         const int kernelCols) {
    const int row = get_global_id(0);
    const int col = get_global_id(1);

    const int halfKernelRows = kernelRows / 2;
    const int halfKernelCols = kernelCols / 2;

    float_type sum = 0.0f;
    for (int i = -halfKernelRows; i <= halfKernelRows; ++i) {
        for (int j = -halfKernelCols; j <= halfKernelCols; ++j) {
            int x = clamp(row + i, 0, inputRows - 1);
            int y = clamp(col + j, 0, inputCols - 1);

            int kernelIdx = (i + halfKernelRows) * kernelCols + (j + halfKernelCols);
            int inputIdx = x * inputCols + y;

            sum += input[inputIdx] * kernel[kernelIdx];
        }
    }
    output[row * inputCols + col] = sum;
}
)CLC";

// Function to convolve a 2D input with a 2D kernel using OpenCL
auto convolve2DOpenCL(const std::vector<std::vector<f64>>& input,
                      const std::vector<std::vector<f64>>& kernel,
                      const ConvolutionOptions<f64>& options,
                      std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            try {
                auto context = initializeOpenCL();
                auto queue = createCommandQueue(context.get());

                const usize inputRows = input.size();
                const usize inputCols = input[0].size();
                const usize kernelRows = kernel.size();
                const usize kernelCols = kernel[0].size();

                // 验证输入有效性
                if (inputRows == 0 || inputCols == 0 || kernelRows == 0 ||
                    kernelCols == 0) {
                    THROW_CONVOLVE_ERROR(
                        "Input and kernel matrices must not be empty");
                }

                // 检查所有行的长度是否一致
                for (const auto& row : input) {
                    if (row.size() != inputCols) {
                        THROW_CONVOLVE_ERROR(
                            "Input matrix must have uniform column sizes");
                    }
                }

                for (const auto& row : kernel) {
                    if (row.size() != kernelCols) {
                        THROW_CONVOLVE_ERROR(
                            "Kernel matrix must have uniform column sizes");
                    }
                }

                // Determine data type for OpenCL
                std::string buildOptions = "";
                usize elementSize = sizeof(f32);
                if (options.useDoublePrecision) {
                    // Check for double precision support
                    cl_device_id device_id;
                    clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device_id,
                                   nullptr);
                    char extensions[1024];
                    clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS,
                                    sizeof(extensions), extensions, nullptr);
                    if (std::string(extensions).find("cl_khr_fp64") !=
                        std::string::npos) {
                        buildOptions = "-D USE_DOUBLE";
                        elementSize = sizeof(f64);
                    } else {
                        // Fallback to float if double is not supported
                        // THROW_CONVOLVE_ERROR("Double precision not supported
                        // by OpenCL device. Falling back to float.");
                    }
                }

                // 扁平化数据以便传输到OpenCL设备
                std::vector<std::byte> inputFlattened(inputRows * inputCols *
                                                      elementSize);
                std::vector<std::byte> kernelFlattened(kernelRows * kernelCols *
                                                       elementSize);
                std::vector<std::byte> outputFlattened(inputRows * inputCols *
                                                       elementSize);

                if (elementSize == sizeof(f64)) {
                    for (usize i = 0; i < inputRows; ++i) {
                        for (usize j = 0; j < inputCols; ++j) {
                            *reinterpret_cast<f64*>(
                                &inputFlattened[elementSize *
                                                (i * inputCols + j)]) =
                                input[i][j];
                        }
                    }
                    for (usize i = 0; i < kernelRows; ++i) {
                        for (usize j = 0; j < kernelCols; ++j) {
                            *reinterpret_cast<f64*>(
                                &kernelFlattened[elementSize *
                                                 (i * kernelCols + j)]) =
                                kernel[i][j];
                        }
                    }
                } else {
                    for (usize i = 0; i < inputRows; ++i) {
                        for (usize j = 0; j < inputCols; ++j) {
                            *reinterpret_cast<f32*>(
                                &inputFlattened[elementSize *
                                                (i * inputCols + j)]) =
                                static_cast<f32>(input[i][j]);
                        }
                    }
                    for (usize i = 0; i < kernelRows; ++i) {
                        for (usize j = 0; j < kernelCols; ++j) {
                            *reinterpret_cast<f32*>(
                                &kernelFlattened[elementSize *
                                                 (i * kernelCols + j)]) =
                                static_cast<f32>(kernel[i][j]);
                        }
                    }
                }

                // 创建OpenCL缓冲区
                cl_int err;
                CLMemPtr inputBuffer(clCreateBuffer(
                    context.get(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                    inputFlattened.size(), inputFlattened.data(), &err));
                checkErr(err, "Creating input buffer");

                CLMemPtr kernelBuffer(clCreateBuffer(
                    context.get(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                    kernelFlattened.size(), kernelFlattened.data(), &err));
                checkErr(err, "Creating kernel buffer");

                CLMemPtr outputBuffer(
                    clCreateBuffer(context.get(), CL_MEM_WRITE_ONLY,
                                   outputFlattened.size(), nullptr, &err));
                checkErr(err, "Creating output buffer");

                // 创建和编译OpenCL程序
                auto program =
                    createProgram(convolve2DKernelSrc, context.get());
                err = clBuildProgram(program.get(), 0, nullptr,
                                     buildOptions.c_str(), nullptr, nullptr);

                // 处理构建错误，提供详细错误信息
                if (err != CL_SUCCESS) {
                    cl_device_id device_id;
                    clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device_id,
                                   nullptr);

                    usize logSize;
                    clGetProgramBuildInfo(program.get(), device_id,
                                          CL_PROGRAM_BUILD_LOG, 0, nullptr,
                                          &logSize);

                    std::vector<char> buildLog(logSize);
                    clGetProgramBuildInfo(program.get(), device_id,
                                          CL_PROGRAM_BUILD_LOG, logSize,
                                          buildLog.data(), nullptr);

                    THROW_CONVOLVE_ERROR("Failed to build OpenCL program: {}",
                                         std::string(buildLog.data(), logSize));
                }

                // 创建内核
                CLKernelPtr openclKernel(
                    clCreateKernel(program.get(), "convolve2D", &err));
                checkErr(err, "Creating kernel");

                // 设置内核参数
                i32 inputRowsInt = static_cast<i32>(inputRows);
                i32 inputColsInt = static_cast<i32>(inputCols);
                i32 kernelRowsInt = static_cast<i32>(kernelRows);
                i32 kernelColsInt = static_cast<i32>(kernelCols);

                err = clSetKernelArg(openclKernel.get(), 0, sizeof(cl_mem),
                                     &inputBuffer.get());
                err |= clSetKernelArg(openclKernel.get(), 1, sizeof(cl_mem),
                                      &kernelBuffer.get());
                err |= clSetKernelArg(openclKernel.get(), 2, sizeof(cl_mem),
                                      &outputBuffer.get());
                err |= clSetKernelArg(openclKernel.get(), 3, sizeof(i32),
                                      &inputRowsInt);
                err |= clSetKernelArg(openclKernel.get(), 4, sizeof(i32),
                                      &inputColsInt);
                err |= clSetKernelArg(openclKernel.get(), 5, sizeof(i32),
                                      &kernelRowsInt);
                err |= clSetKernelArg(openclKernel.get(), 6, sizeof(i32),
                                      &kernelColsInt);
                checkErr(err, "Setting kernel arguments");

                // 执行内核
                usize globalWorkSize[2] = {inputRows, inputCols};
                err = clEnqueueNDRangeKernel(queue.get(), openclKernel.get(), 2,
                                             nullptr, globalWorkSize, nullptr,
                                             0, nullptr, nullptr);
                checkErr(err, "Enqueueing kernel");

                // 等待完成并读取结果
                clFinish(queue.get());

                err = clEnqueueReadBuffer(queue.get(), outputBuffer.get(),
                                          CL_TRUE, 0, outputFlattened.size(),
                                          outputFlattened.data(), 0, nullptr,
                                          nullptr);
                checkErr(err, "Reading back output buffer");

                // 将结果转换回2D向量
                std::vector<std::vector<f64>> output(
                    inputRows, std::vector<f64>(inputCols));

                if (elementSize == sizeof(f64)) {
                    for (usize i = 0; i < inputRows; ++i) {
                        for (usize j = 0; j < inputCols; ++j) {
                            output[i][j] = *reinterpret_cast<f64*>(
                                &outputFlattened[elementSize *
                                                 (i * inputCols + j)]);
                        }
                    }
                } else {
                    for (usize i = 0; i < inputRows; ++i) {
                        for (usize j = 0; j < inputCols; ++j) {
                            output[i][j] =
                                static_cast<f64>(*reinterpret_cast<f32*>(
                                    &outputFlattened[elementSize *
                                                     (i * inputCols + j)]));
                        }
                    }
                }

                return output;
            } catch (const std::exception& e) {
                // 重新抛出异常，提供更多上下文
                THROW_CONVOLVE_ERROR("OpenCL convolution failed: {}", e.what());
            }
        });
}

// OpenCL实现的二维反卷积
auto deconvolve2DOpenCL(const std::vector<std::vector<f64>>& signal,
                        const std::vector<std::vector<f64>>& kernel,
                        i32 numThreads) -> std::vector<std::vector<f64>> {
    ConvolutionOptions<f64> options;
    options.numThreads = numThreads;
    return deconvolve2DOpenCL(signal, kernel, options, {}).get();
}

auto deconvolve2DOpenCL(const std::vector<std::vector<f64>>& signal,
                        const std::vector<std::vector<f64>>& kernel,
                        const ConvolutionOptions<f64>& options,
                        std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            try {
                // Can implement OpenCL version of deconvolution here.
                // For simplicity, calling non-OpenCL version.
                return deconvolve2D(signal, kernel, options, stopToken).get();
            } catch (const std::exception& e) {
                THROW_CONVOLVE_ERROR("OpenCL deconvolution failed: {}",
                                     e.what());
            }
        });
}
#endif

// Function to convolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto convolve2D(const std::vector<std::vector<f64>>& input,
                const std::vector<std::vector<f64>>& kernel, i32 numThreads)
    -> std::vector<std::vector<f64>> {
    ConvolutionOptions<f64> options;
    options.numThreads = numThreads;
    return convolve2D(input, kernel, options, {}).get();
}

// Function to convolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto convolve2D(const std::vector<std::vector<f64>>& input,
                const std::vector<std::vector<f64>>& kernel,
                const ConvolutionOptions<f64>& options,
                std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            try {
                // 输入验证
                if (input.empty() || input[0].empty()) {
                    THROW_CONVOLVE_ERROR("Input matrix cannot be empty");
                }
                if (kernel.empty() || kernel[0].empty()) {
                    THROW_CONVOLVE_ERROR("Kernel matrix cannot be empty");
                }

                // 检查每行的列数是否一致
                const auto inputCols = input[0].size();
                const auto kernelCols = kernel[0].size();

                for (const auto& row : input) {
                    if (row.size() != inputCols) {
                        THROW_CONVOLVE_ERROR(
                            "Input matrix must have uniform column sizes");
                    }
                }

                for (const auto& row : kernel) {
                    if (row.size() != kernelCols) {
                        THROW_CONVOLVE_ERROR(
                            "Kernel matrix must have uniform column sizes");
                    }
                }

                // 线程数验证和调整
                i32 numThreads =
                    validateAndAdjustThreadCount(options.numThreads);

#if ATOM_USE_OPENCL
                if (options.useOpenCL) {
                    return convolve2DOpenCL(input, kernel, numThreads).get();
                }
#endif
                const usize inputRows = input.size();
                const usize kernelRows = kernel.size();

                // 扩展输入和卷积核以便于计算
                auto extendedInput = extend2D(input, inputRows + kernelRows - 1,
                                              inputCols + kernelCols - 1);
                auto extendedKernel =
                    extend2D(kernel, inputRows + kernelRows - 1,
                             inputCols + kernelCols - 1);

                std::vector<std::vector<f64>> output(
                    inputRows, std::vector<f64>(inputCols, 0.0));

                // 使用C++20 ranges提高可读性，用std::execution提高性能
                auto computeBlock = [&](usize blockStartRow,
                                        usize blockEndRow) {
                    for (usize i = blockStartRow; i < blockEndRow; ++i) {
                        if (stopToken.stop_requested()) {
                            return;
                        }
                        for (usize j = 0; j < inputCols; ++j) {
                            f64 sum = 0.0;

                            // Standard convolution implementation
                            for (usize ki = 0; ki < kernelRows; ++ki) {
                                for (usize kj = 0; kj < kernelCols; ++kj) {
                                    usize ii = i + ki;
                                    usize jj = j + kj;
                                    if (ii < inputRows + kernelRows - 1 &&
                                        jj < inputCols + kernelCols - 1) {
                                        sum +=
                                            extendedInput[ii][jj] *
                                            extendedKernel[kernelRows - 1 - ki]
                                                          [kernelCols - 1 - kj];
                                    }
                                }
                            }
                            output[i - kernelRows / 2][j] = sum;
                        }
                    }
                };

                // 使用多线程处理
                if (numThreads > 1) {
                    std::vector<std::jthread> threadPool;
                    usize blockSize =
                        (inputRows + static_cast<usize>(numThreads) - 1) /
                        static_cast<usize>(numThreads);
                    usize blockStartRow = kernelRows / 2;

                    for (i32 threadIndex = 0; threadIndex < numThreads;
                         ++threadIndex) {
                        usize startRow =
                            blockStartRow +
                            static_cast<usize>(threadIndex) * blockSize;
                        usize endRow = std::min(startRow + blockSize,
                                                  inputRows + kernelRows / 2);

                        // 使用C++20 jthread自动管理线程生命周期
                        threadPool.emplace_back(computeBlock, startRow, endRow);
                    }

                    // jthread会在作用域结束时自动join
                } else {
                    // 单线程执行
                    computeBlock(kernelRows / 2, inputRows + kernelRows / 2);
                }

                return output;
            } catch (const std::exception& e) {
                THROW_CONVOLVE_ERROR("2D convolution failed: {}", e.what());
            }
        });
}

// Function to deconvolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto deconvolve2D(const std::vector<std::vector<f64>>& signal,
                  const std::vector<std::vector<f64>>& kernel, i32 numThreads)
    -> std::vector<std::vector<f64>> {
    ConvolutionOptions<f64> options;
    options.numThreads = numThreads;
    return deconvolve2D(signal, kernel, options, {}).get();
}

auto deconvolve2D(const std::vector<std::vector<f64>>& signal,
                  const std::vector<std::vector<f64>>& kernel,
                  const ConvolutionOptions<f64>& options,
                  std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            try {
                // 输入验证
                if (signal.empty() || signal[0].empty()) {
                    THROW_CONVOLVE_ERROR("Signal matrix cannot be empty");
                }
                if (kernel.empty() || kernel[0].empty()) {
                    THROW_CONVOLVE_ERROR("Kernel matrix cannot be empty");
                }

                // 验证所有行的列数是否一致
                const auto signalCols = signal[0].size();
                const auto kernelCols = kernel[0].size();

                for (const auto& row : signal) {
                    if (row.size() != signalCols) {
                        THROW_CONVOLVE_ERROR(
                            "Signal matrix must have uniform column sizes");
                    }
                }

                for (const auto& row : kernel) {
                    if (row.size() != kernelCols) {
                        THROW_CONVOLVE_ERROR(
                            "Kernel matrix must have uniform column sizes");
                    }
                }

                // 线程数验证和调整
                i32 numThreads =
                    validateAndAdjustThreadCount(options.numThreads);

#if ATOM_USE_OPENCL
                if (options.useOpenCL) {
                    return deconvolve2DOpenCL(signal, kernel, numThreads).get();
                }
#endif
                const usize signalRows = signal.size();
                const usize kernelRows = kernel.size();

                auto extendedSignal =
                    extend2D(signal, signalRows + kernelRows - 1,
                             signalCols + kernelCols - 1);
                auto extendedKernel =
                    extend2D(kernel, signalRows + kernelRows - 1,
                             signalCols + kernelCols - 1);

                auto discreteFourierTransform2D =
                    [&](const std::vector<std::vector<f64>>& input) {
                        return dfT2D(input, numThreads, stopToken)
                            .get();  // Assume DFT2D supports multithreading
                    };

                auto frequencySignal =
                    discreteFourierTransform2D(extendedSignal);
                auto frequencyKernel =
                    discreteFourierTransform2D(extendedKernel);

                std::vector<std::vector<std::complex<f64>>> frequencyProduct(
                    signalRows + kernelRows - 1,
                    std::vector<std::complex<f64>>(signalCols + kernelCols - 1,
                                                   {0, 0}));

            // Compute frequency domain multiplication (deconvolution)
            for (usize u = 0; u < signalRows + kernelRows - 1; ++u) {
                if (stopToken.stop_requested()) {
                    return {};
                }
                for (usize v = 0; v < signalCols + kernelCols - 1; ++v) {
                    if (std::abs(frequencyKernel[u][v]) > EPSILON) {
                        frequencyProduct[u][v] =
                            std::conj(frequencyKernel[u][v]) /
                            (std::norm(frequencyKernel[u][v]) + EPSILON);
                    } else {
                        frequencyProduct[u][v] = std::conj(frequencyKernel[u][v]);
                    }
                }
            }

                std::vector<std::vector<f64>> frequencyInverse =
                    idfT2D(frequencyProduct, numThreads, stopToken).get();

                std::vector<std::vector<f64>> result(
                    signalRows, std::vector<f64>(signalCols, 0.0));
                for (usize i = 0; i < signalRows; ++i) {
                    for (usize j = 0; j < signalCols; ++j) {
                        result[i][j] =
                            frequencyInverse[i][j] /
                            static_cast<f64>(signalRows * signalCols);
                    }
                }

                return result;
            } catch (const std::exception& e) {
                THROW_CONVOLVE_ERROR("2D deconvolution failed: {}", e.what());
            }
        });
}

// 2D Discrete Fourier Transform (2D DFT)
auto dfT2D(const std::vector<std::vector<f64>>& signal, i32 numThreads)
    -> std::vector<std::vector<std::complex<f64>>> {
    return dfT2D(signal, numThreads, {}).get();
}

auto dfT2D(const std::vector<std::vector<f64>>& signal, i32 numThreads,
           std::stop_token stopToken)
    -> std::future<std::vector<std::vector<std::complex<f64>>>> {
    return std::async(
        std::launch::async,
        [=]() -> std::vector<std::vector<std::complex<f64>>> {
            const usize M = signal.size();
            const usize N = signal[0].size();
            std::vector<std::vector<std::complex<f64>>> frequency(
                M, std::vector<std::complex<f64>>(N, {0, 0}));

            // Lambda function to compute the DFT for a block of rows
            auto computeDFT = [&](usize startRow, usize endRow) {
                for (usize u = startRow; u < endRow; ++u) {
                    if (stopToken.stop_requested()) {
                        return;
                    }
                    for (usize v = 0; v < N; ++v) {
                        std::complex<f64> sum(0, 0);
                        for (usize m = 0; m < M; ++m) {
                            for (usize n = 0; n < N; ++n) {
                                f64 theta = -2 * std::numbers::pi *
                                            ((static_cast<f64>(u) *
                                              static_cast<f64>(m)) /
                                                 static_cast<f64>(M) +
                                             (static_cast<f64>(v) *
                                              static_cast<f64>(n)) /
                                                 static_cast<f64>(N));
                                std::complex<f64> w(std::cos(theta),
                                                    std::sin(theta));
                                sum += signal[m][n] * w;
                            }
                        }
                        frequency[u][v] = sum;
                    }
                }
            };

            // Multithreading support
            if (numThreads > 1) {
                std::vector<std::jthread> threadPool;
                usize rowsPerThread = M / static_cast<usize>(numThreads);
                usize blockStartRow = 0;

                for (i32 threadIndex = 0; threadIndex < numThreads;
                     ++threadIndex) {
                    usize blockEndRow = (threadIndex == numThreads - 1)
                                            ? M
                                            : blockStartRow + rowsPerThread;
                    threadPool.emplace_back(computeDFT, blockStartRow,
                                            blockEndRow);
                    blockStartRow = blockEndRow;
                }

                // Threads are joined automatically by jthread destructor
            } else {
                // Single-threaded execution
                computeDFT(0, M);
            }

            return frequency;
        });
}

// 2D Inverse Discrete Fourier Transform (2D IDFT)
auto idfT2D(const std::vector<std::vector<std::complex<f64>>>& spectrum,
            i32 numThreads) -> std::vector<std::vector<f64>> {
    return idfT2D(spectrum, numThreads, {}).get();
}

auto idfT2D(const std::vector<std::vector<std::complex<f64>>>& spectrum,
            i32 numThreads, std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            const usize M = spectrum.size();
            const usize N = spectrum[0].size();
            std::vector<std::vector<f64>> spatial(M, std::vector<f64>(N, 0.0));

            // Lambda function to compute the IDFT for a block of rows
            auto computeIDFT = [&](usize startRow, usize endRow) {
                for (usize m = startRow; m < endRow; ++m) {
                    if (stopToken.stop_requested()) {
                        return;
                    }
                    for (usize n = 0; n < N; ++n) {
                        std::complex<f64> sum(0.0, 0.0);
                        for (usize u = 0; u < M; ++u) {
                            for (usize v = 0; v < N; ++v) {
                                f64 theta = 2 * std::numbers::pi *
                                            ((static_cast<f64>(u) *
                                              static_cast<f64>(m)) /
                                                 static_cast<f64>(M) +
                                             (static_cast<f64>(v) *
                                              static_cast<f64>(n)) /
                                                 static_cast<f64>(N));
                                std::complex<f64> w(std::cos(theta),
                                                    std::sin(theta));
                                sum += spectrum[u][v] * w;
                            }
                        }
                        spatial[m][n] = sum.real() / (static_cast<f64>(M) *
                                                      static_cast<f64>(N));
                    }
                }
            };

            // Multithreading support
            if (numThreads > 1) {
                std::vector<std::jthread> threadPool;
                usize rowsPerThread = M / static_cast<usize>(numThreads);
                usize blockStartRow = 0;

                for (i32 threadIndex = 0; threadIndex < numThreads;
                     ++threadIndex) {
                    usize blockEndRow = (threadIndex == numThreads - 1)
                                            ? M
                                            : blockStartRow + rowsPerThread;
                    threadPool.emplace_back(computeIDFT, blockStartRow,
                                            blockEndRow);
                    blockStartRow = blockEndRow;
                }

                // Threads are joined automatically by jthread destructor
            } else {
                // Single-threaded execution
                computeIDFT(0, M);
            }

            return spatial;
        });
}

// Function to generate a Gaussian kernel
auto generateGaussianKernel(i32 size, f64 sigma)
    -> std::vector<std::vector<f64>> {
    std::vector<std::vector<f64>> kernel(
        static_cast<usize>(size), std::vector<f64>(static_cast<usize>(size)));
    f64 sum = 0.0;
    i32 center = size / 2;

    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; ++j) {
            kernel[static_cast<usize>(i)][static_cast<usize>(j)] =
                std::exp(
                    -0.5 *
                    (std::pow(static_cast<f64>(i - center) / sigma, 2.0) +
                     std::pow(static_cast<f64>(j - center) / sigma, 2.0))) /
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

    return kernel;
}

// Function to apply Gaussian filter to an image
auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel)
    -> std::vector<std::vector<f64>> {
    ConvolutionOptions<f64> options;
    return applyGaussianFilter(image, kernel, options, {}).get();
}

auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel,
                         const ConvolutionOptions<f64>& options,
                         std::stop_token stopToken)
    -> std::future<std::vector<std::vector<f64>>> {
    return std::async(
        std::launch::async, [=]() -> std::vector<std::vector<f64>> {
            const usize imageHeight = image.size();
            const usize imageWidth = image[0].size();
            const usize kernelSize = kernel.size();
            const usize kernelRadius = kernelSize / 2;
            std::vector<std::vector<f64>> filteredImage(
                imageHeight, std::vector<f64>(imageWidth, 0.0));

            for (usize i = 0; i < imageHeight; ++i) {
                if (stopToken.stop_requested()) {
                    return {};
                }
                for (usize j = 0; j < imageWidth; ++j) {
                    f64 sum = 0.0;
                    for (usize k = 0; k < kernelSize; ++k) {
                        for (usize l = 0; l < kernelSize; ++l) {
                            i32 x =
                                std::clamp(static_cast<i32>(i + k), 0,
                                           static_cast<i32>(imageHeight) - 1);
                            i32 y =
                                std::clamp(static_cast<i32>(j + l), 0,
                                           static_cast<i32>(imageWidth) - 1);
                            sum += image[static_cast<usize>(x)]
                                        [static_cast<usize>(y)] *
                                   kernel[kernelRadius + k][kernelRadius + l];
                        }
                    }
                    filteredImage[i][j] = sum;
                }
            }
            return filteredImage;
        });
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
