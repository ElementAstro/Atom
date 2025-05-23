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
#include <numbers>
#include <thread>
#include <utility>
#include <vector>

#if ATOM_USE_SIMD && !ATOM_USE_STD_SIMD
#ifdef __SSE__
#include <immintrin.h>
#endif
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
                            input[Usize::min(i, inputRows - 1)]
                                 [Usize::min(j, inputCols - 1)];
                    } else if (j >= padLeft + inputCols) {
                        // Top-right corner
                        output[padTop - 1 - i][j] =
                            input[Usize::min(i, inputRows - 1)][Usize::min(
                                inputCols - 1 - (j - (padLeft + inputCols)),
                                inputCols - 1)];
                    } else {
                        // Top edge
                        output[padTop - 1 - i][j] =
                            input[Usize::min(i, inputRows - 1)][j - padLeft];
                    }
                }
            }

            // Bottom border padding
            for (usize i = 0; i < padBottom; ++i) {
                for (usize j = 0; j < outputCols; ++j) {
                    if (j < padLeft) {
                        // Bottom-left corner
                        output[padTop + inputRows + i][j] =
                            input[Usize::max(0UL, inputRows - 1 - i)]
                                 [Usize::min(j, inputCols - 1)];
                    } else if (j >= padLeft + inputCols) {
                        // Bottom-right corner
                        output[padTop + inputRows + i][j] =
                            input[Usize::max(0UL, inputRows - 1 - i)]
                                 [Usize::max(0UL,
                                             inputCols - 1 -
                                                 (j - (padLeft + inputCols)))];
                    } else {
                        // Bottom edge
                        output[padTop + inputRows + i][j] = input[Usize::max(
                            0UL, inputRows - 1 - i)][j - padLeft];
                    }
                }
            }

            // Left border padding
            for (usize i = padTop; i < padTop + inputRows; ++i) {
                for (usize j = 0; j < padLeft; ++j) {
                    output[i][padLeft - 1 - j] =
                        input[i - padTop][Usize::min(j, inputCols - 1)];
                }
            }

            // Right border padding
            for (usize i = padTop; i < padTop + inputRows; ++i) {
                for (usize j = 0; j < padRight; ++j) {
                    output[i][padLeft + inputCols + j] =
                        input[i - padTop][Usize::max(0UL, inputCols - 1 - j)];
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
__kernel void convolve2D(__global const float* input,
                         __global const float* kernel,
                         __global float* output,
                         const int inputRows,
                         const int inputCols,
                         const int kernelRows,
                         const int kernelCols) {
    const int row = get_global_id(0);
    const int col = get_global_id(1);

    const int halfKernelRows = kernelRows / 2;
    const int halfKernelCols = kernelCols / 2;

    float sum = 0.0f;
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
                      i32 numThreads) -> std::vector<std::vector<f64>> {
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
            THROW_CONVOLVE_ERROR("Input and kernel matrices must not be empty");
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

        // 扁平化数据以便传输到OpenCL设备
        std::vector<f32> inputFlattened(inputRows * inputCols);
        std::vector<f32> kernelFlattened(kernelRows * kernelCols);
        std::vector<f32> outputFlattened(inputRows * inputCols, 0.0f);

        // 使用C++20 ranges进行数据扁平化
        for (usize i = 0; i < inputRows; ++i) {
            for (usize j = 0; j < inputCols; ++j) {
                inputFlattened[i * inputCols + j] =
                    static_cast<f32>(input[i][j]);
            }
        }

        for (usize i = 0; i < kernelRows; ++i) {
            for (usize j = 0; j < kernelCols; ++j) {
                kernelFlattened[i * kernelCols + j] =
                    static_cast<f32>(kernel[i][j]);
            }
        }

        // 创建OpenCL缓冲区
        cl_int err;
        CLMemPtr inputBuffer(clCreateBuffer(
            context.get(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            sizeof(f32) * inputFlattened.size(), inputFlattened.data(), &err));
        checkErr(err, "Creating input buffer");

        CLMemPtr kernelBuffer(clCreateBuffer(
            context.get(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
            sizeof(f32) * kernelFlattened.size(), kernelFlattened.data(),
            &err));
        checkErr(err, "Creating kernel buffer");

        CLMemPtr outputBuffer(clCreateBuffer(
            context.get(), CL_MEM_WRITE_ONLY,
            sizeof(f32) * outputFlattened.size(), nullptr, &err));
        checkErr(err, "Creating output buffer");

        // 创建和编译OpenCL程序
        auto program = createProgram(convolve2DKernelSrc, context.get());
        err = clBuildProgram(program.get(), 0, nullptr, nullptr, nullptr,
                             nullptr);

        // 处理构建错误，提供详细错误信息
        if (err != CL_SUCCESS) {
            cl_device_id device_id;
            clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device_id, nullptr);

            usize logSize;
            clGetProgramBuildInfo(program.get(), device_id,
                                  CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);

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
        err |=
            clSetKernelArg(openclKernel.get(), 3, sizeof(i32), &inputRowsInt);
        err |=
            clSetKernelArg(openclKernel.get(), 4, sizeof(i32), &inputColsInt);
        err |=
            clSetKernelArg(openclKernel.get(), 5, sizeof(i32), &kernelRowsInt);
        err |=
            clSetKernelArg(openclKernel.get(), 6, sizeof(i32), &kernelColsInt);
        checkErr(err, "Setting kernel arguments");

        // 执行内核
        usize globalWorkSize[2] = {inputRows, inputCols};
        err = clEnqueueNDRangeKernel(queue.get(), openclKernel.get(), 2,
                                     nullptr, globalWorkSize, nullptr, 0,
                                     nullptr, nullptr);
        checkErr(err, "Enqueueing kernel");

        // 等待完成并读取结果
        clFinish(queue.get());

        err = clEnqueueReadBuffer(queue.get(), outputBuffer.get(), CL_TRUE, 0,
                                  sizeof(f32) * outputFlattened.size(),
                                  outputFlattened.data(), 0, nullptr, nullptr);
        checkErr(err, "Reading back output buffer");

        // 将结果转换回2D向量
        std::vector<std::vector<f64>> output(inputRows,
                                             std::vector<f64>(inputCols));

        for (usize i = 0; i < inputRows; ++i) {
            for (usize j = 0; j < inputCols; ++j) {
                output[i][j] =
                    static_cast<f64>(outputFlattened[i * inputCols + j]);
            }
        }

        return output;
    } catch (const std::exception& e) {
        // 重新抛出异常，提供更多上下文
        THROW_CONVOLVE_ERROR("OpenCL convolution failed: {}", e.what());
    }
}

// OpenCL实现的二维反卷积
auto deconvolve2DOpenCL(const std::vector<std::vector<f64>>& signal,
                        const std::vector<std::vector<f64>>& kernel,
                        i32 numThreads) -> std::vector<std::vector<f64>> {
    try {
        // 可以实现OpenCL版本的反卷积
        // 这里为简化起见，调用非OpenCL版本
        return deconvolve2D(signal, kernel, numThreads);
    } catch (const std::exception& e) {
        THROW_CONVOLVE_ERROR("OpenCL deconvolution failed: {}", e.what());
    }
}
#endif

// Function to convolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto convolve2D(const std::vector<std::vector<f64>>& input,
                const std::vector<std::vector<f64>>& kernel, i32 numThreads)
    -> std::vector<std::vector<f64>> {
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
        i32 availableThreads =
            static_cast<i32>(std::thread::hardware_concurrency());
        if (numThreads <= 0) {
            numThreads = 1;
        } else if (numThreads > availableThreads) {
            numThreads = availableThreads;
        }

#if ATOM_USE_OPENCL
        return convolve2DOpenCL(input, kernel, numThreads);
#else
        const usize inputRows = input.size();
        const usize kernelRows = kernel.size();

        // 扩展输入和卷积核以便于计算
        auto extendedInput = extend2D(input, inputRows + kernelRows - 1,
                                      inputCols + kernelCols - 1);
        auto extendedKernel = extend2D(kernel, inputRows + kernelRows - 1,
                                       inputCols + kernelCols - 1);

        std::vector<std::vector<f64>> output(inputRows,
                                             std::vector<f64>(inputCols, 0.0));

        // 使用C++20 ranges提高可读性，用std::execution提高性能
        auto computeBlock = [&](usize blockStartRow, usize blockEndRow) {
            for (usize i = blockStartRow; i < blockEndRow; ++i) {
                for (usize j = 0; j < inputCols; ++j) {
                    f64 sum = 0.0;

#ifdef ATOM_ATOM_USE_SIMD
                    // 使用SIMD加速内循环计算
                    const usize kernelRowMid = kernelRows / 2;
                    const usize kernelColMid = kernelCols / 2;

                    // SIMD_ALIGNED double simdSum[SIMD_WIDTH] = {0.0};
                    // __m256d sum_vec = _mm256_setzero_pd();

                    for (usize ki = 0; ki < kernelRows; ++ki) {
                        for (usize kj = 0; kj < kernelCols; ++kj) {
                            usize ii = i + ki;
                            usize jj = j + kj;
                            if (ii < inputRows + kernelRows - 1 &&
                                jj < inputCols + kernelCols - 1) {
                                sum += extendedInput[ii][jj] *
                                       extendedKernel[kernelRows - 1 - ki]
                                                     [kernelCols - 1 - kj];
                            }
                        }
                    }
#else
                    // 标准实现
                    for (usize ki = 0; ki < kernelRows; ++ki) {
                        for (usize kj = 0; kj < kernelCols; ++kj) {
                            usize ii = i + ki;
                            usize jj = j + kj;
                            if (ii < inputRows + kernelRows - 1 &&
                                jj < inputCols + kernelCols - 1) {
                                sum += extendedInput[ii][jj] *
                                       extendedKernel[kernelRows - 1 - ki]
                                                     [kernelCols - 1 - kj];
                            }
                        }
                    }
#endif
                    output[i - kernelRows / 2][j] = sum;
                }
            }
        };

        // 使用多线程处理
        if (numThreads > 1) {
            std::vector<std::jthread> threadPool;
            usize blockSize = (inputRows + static_cast<usize>(numThreads) - 1) /
                              static_cast<usize>(numThreads);
            usize blockStartRow = kernelRows / 2;

            for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
                usize startRow =
                    blockStartRow + static_cast<usize>(threadIndex) * blockSize;
                usize endRow = Usize::min(startRow + blockSize,
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
#endif
    } catch (const std::exception& e) {
        THROW_CONVOLVE_ERROR("2D convolution failed: {}", e.what());
    }
}

// Function to deconvolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto deconvolve2D(const std::vector<std::vector<f64>>& signal,
                  const std::vector<std::vector<f64>>& kernel, i32 numThreads)
    -> std::vector<std::vector<f64>> {
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
        i32 availableThreads =
            static_cast<i32>(std::thread::hardware_concurrency());
        if (numThreads <= 0) {
            numThreads = 1;
        } else if (numThreads > availableThreads) {
            numThreads = availableThreads;
        }

#if ATOM_USE_OPENCL
        return deconvolve2DOpenCL(signal, kernel, numThreads);
#else
        const usize signalRows = signal.size();
        const usize kernelRows = kernel.size();

        auto extendedSignal = extend2D(signal, signalRows + kernelRows - 1,
                                       signalCols + kernelCols - 1);
        auto extendedKernel = extend2D(kernel, signalRows + kernelRows - 1,
                                       signalCols + kernelCols - 1);

        auto discreteFourierTransform2D =
            [&](const std::vector<std::vector<f64>>& input) {
                return dfT2D(
                    input,
                    numThreads);  // Assume DFT2D supports multithreading
            };

        auto frequencySignal = discreteFourierTransform2D(extendedSignal);
        auto frequencyKernel = discreteFourierTransform2D(extendedKernel);

        std::vector<std::vector<std::complex<f64>>> frequencyProduct(
            signalRows + kernelRows - 1,
            std::vector<std::complex<f64>>(signalCols + kernelCols - 1,
                                           {0, 0}));

        // SIMD-optimized computation of frequencyProduct
#ifdef ATOM_ATOM_USE_SIMD
        const i32 simdWidth = SIMD_WIDTH;
        __m256d epsilon_vec = _mm256_set1_pd(EPSILON);

        for (usize u = 0; u < signalRows + kernelRows - 1; ++u) {
            for (usize v = 0; v < signalCols + kernelCols - 1;
                 v += static_cast<usize>(simdWidth)) {
                __m256d kernelReal =
                    _mm256_loadu_pd(&frequencyKernel[u][v].real());
                __m256d kernelImag =
                    _mm256_loadu_pd(&frequencyKernel[u][v].imag());

                __m256d magnitude = _mm256_sqrt_pd(
                    _mm256_add_pd(_mm256_mul_pd(kernelReal, kernelReal),
                                  _mm256_mul_pd(kernelImag, kernelImag)));
                __m256d mask =
                    _mm256_cmp_pd(magnitude, epsilon_vec, _CMP_GT_OQ);

                __m256d norm =
                    _mm256_add_pd(_mm256_mul_pd(kernelReal, kernelReal),
                                  _mm256_mul_pd(kernelImag, kernelImag));
                norm = _mm256_add_pd(norm, epsilon_vec);

                __m256d normalizedReal = _mm256_div_pd(kernelReal, norm);
                __m256d normalizedImag = _mm256_div_pd(
                    _mm256_xor_pd(kernelImag, _mm256_set1_pd(-0.0)), norm);

                normalizedReal =
                    _mm256_blendv_pd(kernelReal, normalizedReal, mask);
                normalizedImag =
                    _mm256_blendv_pd(kernelImag, normalizedImag, mask);

                _mm256_storeu_pd(&frequencyProduct[u][v].real(),
                                 normalizedReal);
                _mm256_storeu_pd(&frequencyProduct[u][v].imag(),
                                 normalizedImag);
            }

            // Handle remaining elements
            for (usize v = ((signalCols + kernelCols - 1) /
                            static_cast<usize>(simdWidth)) *
                           static_cast<usize>(simdWidth);
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
        for (usize u = 0; u < signalRows + kernelRows - 1; ++u) {
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
#endif

        std::vector<std::vector<f64>> frequencyInverse =
            idfT2D(frequencyProduct, numThreads);

        std::vector<std::vector<f64>> result(signalRows,
                                             std::vector<f64>(signalCols, 0.0));
        for (usize i = 0; i < signalRows; ++i) {
            for (usize j = 0; j < signalCols; ++j) {
                result[i][j] = frequencyInverse[i][j] /
                               static_cast<f64>(signalRows * signalCols);
            }
        }

        return result;
#endif
    } catch (const std::exception& e) {
        THROW_CONVOLVE_ERROR("2D deconvolution failed: {}", e.what());
    }
}

// 2D Discrete Fourier Transform (2D DFT)
auto dfT2D(const std::vector<std::vector<f64>>& signal, i32 numThreads)
    -> std::vector<std::vector<std::complex<f64>>> {
    const usize M = signal.size();
    const usize N = signal[0].size();
    std::vector<std::vector<std::complex<f64>>> frequency(
        M, std::vector<std::complex<f64>>(N, {0, 0}));

    // Lambda function to compute the DFT for a block of rows
    auto computeDFT = [&](usize startRow, usize endRow) {
#ifdef ATOM_ATOM_USE_SIMD
        std::array<f64, 4> realParts{};
        std::array<f64, 4> imagParts{};
#endif
        for (usize u = startRow; u < endRow; ++u) {
            for (usize v = 0; v < N; ++v) {
#ifdef ATOM_ATOM_USE_SIMD
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();

                for (usize m = 0; m < M; ++m) {
                    for (usize n = 0; n < N; n += 4) {
                        f64 theta[4];
                        for (i32 k = 0; k < 4; ++k) {
                            theta[k] =
                                -2.0 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + static_cast<usize>(k))) /
                                     static_cast<f64>(N));
                        }

                        __m256d signalVec = _mm256_loadu_pd(&signal[m][n]);
                        __m256d cosVec = _mm256_setr_pd(
                            F64::cos(theta[0]), F64::cos(theta[1]),
                            F64::cos(theta[2]), F64::cos(theta[3]));
                        __m256d sinVec = _mm256_setr_pd(
                            F64::sin(theta[0]), F64::sin(theta[1]),
                            F64::sin(theta[2]), F64::sin(theta[3]));

                        sumReal = _mm256_add_pd(
                            sumReal, _mm256_mul_pd(signalVec, cosVec));
                        sumImag = _mm256_add_pd(
                            sumImag, _mm256_mul_pd(signalVec, sinVec));
                    }
                }

                _mm256_store_pd(realParts.data(), sumReal);
                _mm256_store_pd(imagParts.data(), sumImag);

                f64 realSum =
                    realParts[0] + realParts[1] + realParts[2] + realParts[3];
                f64 imagSum =
                    imagParts[0] + imagParts[1] + imagParts[2] + imagParts[3];

                frequency[u][v] = std::complex<f64>(realSum, imagSum);
#else
                std::complex<f64> sum(0, 0);
                for (usize m = 0; m < M; ++m) {
                    for (usize n = 0; n < N; ++n) {
                        f64 theta =
                            -2 * std::numbers::pi *
                            ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                 static_cast<f64>(M) +
                             (static_cast<f64>(v) * static_cast<f64>(n)) /
                                 static_cast<f64>(N));
                        std::complex<f64> w(F64::cos(theta), F64::sin(theta));
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
        usize rowsPerThread = M / static_cast<usize>(numThreads);
        usize blockStartRow = 0;

        for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            usize blockEndRow = (threadIndex == numThreads - 1)
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
auto idfT2D(const std::vector<std::vector<std::complex<f64>>>& spectrum,
            i32 numThreads) -> std::vector<std::vector<f64>> {
    const usize M = spectrum.size();
    const usize N = spectrum[0].size();
    std::vector<std::vector<f64>> spatial(M, std::vector<f64>(N, 0.0));

    // Lambda function to compute the IDFT for a block of rows
    auto computeIDFT = [&](usize startRow, usize endRow) {
        for (usize m = startRow; m < endRow; ++m) {
            for (usize n = 0; n < N; ++n) {
#ifdef ATOM_ATOM_USE_SIMD
                __m256d sumReal = _mm256_setzero_pd();
                __m256d sumImag = _mm256_setzero_pd();
                for (usize u = 0; u < M; ++u) {
                    for (usize v = 0; v < N; v += SIMD_WIDTH) {
                        __m256d theta = _mm256_set_pd(
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 3)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 2)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) *
                                  static_cast<f64>(n + 1)) /
                                     static_cast<f64>(N)),
                            2 * std::numbers::pi *
                                ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                     static_cast<f64>(M) +
                                 (static_cast<f64>(v) * static_cast<f64>(n)) /
                                     static_cast<f64>(N)));
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
                f64 realPart = _mm256_hadd_pd(sumReal, sumReal).m256d_f64[0] +
                               _mm256_hadd_pd(sumReal, sumReal).m256d_f64[2];
                f64 imagPart = _mm256_hadd_pd(sumImag, sumImag).m256d_f64[0] +
                               _mm256_hadd_pd(sumImag, sumImag).m256d_f64[2];
                spatial[m][n] = (realPart + imagPart) /
                                (static_cast<f64>(M) * static_cast<f64>(N));
#else
                std::complex<f64> sum(0.0, 0.0);
                for (usize u = 0; u < M; ++u) {
                    for (usize v = 0; v < N; ++v) {
                        f64 theta =
                            2 * std::numbers::pi *
                            ((static_cast<f64>(u) * static_cast<f64>(m)) /
                                 static_cast<f64>(M) +
                             (static_cast<f64>(v) * static_cast<f64>(n)) /
                                 static_cast<f64>(N));
                        std::complex<f64> w(F64::cos(theta), F64::sin(theta));
                        sum += spectrum[u][v] * w;
                    }
                }
                spatial[m][n] = std::real(sum) /
                                (static_cast<f64>(M) * static_cast<f64>(N));
#endif
            }
        }
    };

    // Multithreading support
    if (numThreads > 1) {
        std::vector<std::jthread> threadPool;
        usize rowsPerThread = M / static_cast<usize>(numThreads);
        usize blockStartRow = 0;

        for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
            usize blockEndRow = (threadIndex == numThreads - 1)
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
auto generateGaussianKernel(i32 size, f64 sigma)
    -> std::vector<std::vector<f64>> {
    std::vector<std::vector<f64>> kernel(
        static_cast<usize>(size), std::vector<f64>(static_cast<usize>(size)));
    f64 sum = 0.0;
    i32 center = size / 2;

#ifdef ATOM_ATOM_USE_SIMD
    SIMD_ALIGNED f64 tempBuffer[SIMD_WIDTH];
    __m256d sigmaVec = _mm256_set1_pd(sigma);
    __m256d twoSigmaSquared =
        _mm256_mul_pd(_mm256_set1_pd(2.0), _mm256_mul_pd(sigmaVec, sigmaVec));
    __m256d scale = _mm256_div_pd(
        _mm256_set1_pd(1.0),
        _mm256_mul_pd(_mm256_set1_pd(2 * std::numbers::pi), twoSigmaSquared));

    for (i32 i = 0; i < size; ++i) {
        __m256d iVec = _mm256_set1_pd(static_cast<f64>(i - center));
        for (i32 j = 0; j < size; j += SIMD_WIDTH) {
            __m256d jVec = _mm256_set_pd(static_cast<f64>(j + 3 - center),
                                         static_cast<f64>(j + 2 - center),
                                         static_cast<f64>(j + 1 - center),
                                         static_cast<f64>(j - center));

            __m256d xSquared = _mm256_mul_pd(iVec, iVec);
            __m256d ySquared = _mm256_mul_pd(jVec, jVec);
            __m256d exponent = _mm256_div_pd(_mm256_add_pd(xSquared, ySquared),
                                             twoSigmaSquared);
            __m256d kernelValues = _mm256_mul_pd(
                scale,
                _mm256_exp_pd(_mm256_mul_pd(_mm256_set1_pd(-0.5), exponent)));

            _mm256_store_pd(tempBuffer, kernelValues);
            for (i32 k = 0; k < SIMD_WIDTH && (j + k) < size; ++k) {
                kernel[static_cast<usize>(i)][static_cast<usize>(j + k)] =
                    tempBuffer[k];
                sum += tempBuffer[k];
            }
        }
    }

    // Normalize to ensure the sum of the weights is 1
    __m256d sumVec = _mm256_set1_pd(sum);
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; j += SIMD_WIDTH) {
            __m256d kernelValues = _mm256_loadu_pd(
                &kernel[static_cast<usize>(i)][static_cast<usize>(j)]);
            kernelValues = _mm256_div_pd(kernelValues, sumVec);
            _mm256_storeu_pd(
                &kernel[static_cast<usize>(i)][static_cast<usize>(j)],
                kernelValues);
        }
    }
#else
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; i < size; ++j) {
            kernel[static_cast<usize>(i)][static_cast<usize>(j)] =
                F64::exp(
                    -0.5 *
                    (F64::pow(static_cast<f64>(i - center) / sigma, 2.0) +
                     F64::pow(static_cast<f64>(j - center) / sigma, 2.0))) /
                (2 * std::numbers::pi * sigma * sigma);
            sum += kernel[static_cast<usize>(i)][static_cast<usize>(j)];
        }
    }

    // Normalize to ensure the sum of the weights is 1
    for (i32 i = 0; i < size; ++i) {
        for (i32 j = 0; j < size; ++j) {  // 修复循环变量错误
            kernel[static_cast<usize>(i)][static_cast<usize>(j)] /= sum;
        }
    }
#endif

    return kernel;
}

// Function to apply Gaussian filter to an image
auto applyGaussianFilter(const std::vector<std::vector<f64>>& image,
                         const std::vector<std::vector<f64>>& kernel)
    -> std::vector<std::vector<f64>> {
    const usize imageHeight = image.size();
    const usize imageWidth = image[0].size();
    const usize kernelSize = kernel.size();
    const usize kernelRadius = kernelSize / 2;
    std::vector<std::vector<f64>> filteredImage(
        imageHeight, std::vector<f64>(imageWidth, 0.0));

#ifdef ATOM_ATOM_USE_SIMD
    SIMD_ALIGNED f64 tempBuffer[SIMD_WIDTH];

    for (usize i = 0; i < imageHeight; ++i) {
        for (usize j = 0; j < imageWidth; j += SIMD_WIDTH) {
            __m256d sumVec = _mm256_setzero_pd();

            for (usize k = 0; k < kernelSize; ++k) {
                for (usize l = 0; l < kernelSize; ++l) {
                    __m256d kernelVal = _mm256_set1_pd(
                        kernel[kernelRadius + k][kernelRadius + l]);

                    for (i32 m = 0; m < SIMD_WIDTH; ++m) {
                        i32 x = I32::clamp(static_cast<i32>(i + k), 0,
                                           static_cast<i32>(imageHeight) - 1);
                        i32 y = I32::clamp(
                            static_cast<i32>(j + l + static_cast<usize>(m)), 0,
                            static_cast<i32>(imageWidth) - 1);
                        tempBuffer[m] =
                            image[static_cast<usize>(x)][static_cast<usize>(y)];
                    }

                    __m256d imageVal = _mm256_loadu_pd(tempBuffer);
                    sumVec = _mm256_add_pd(sumVec,
                                           _mm256_mul_pd(imageVal, kernelVal));
                }
            }

            _mm256_storeu_pd(tempBuffer, sumVec);
            for (i32 m = 0;
                 m < SIMD_WIDTH && (j + static_cast<usize>(m)) < imageWidth;
                 ++m) {
                filteredImage[i][j + static_cast<usize>(m)] = tempBuffer[m];
            }
        }
    }
#else
    for (usize i = 0; i < imageHeight; ++i) {
        for (usize j = 0; j < imageWidth; ++j) {
            f64 sum = 0.0;
            for (usize k = 0; k < kernelSize; ++k) {
                for (usize l = 0; l < kernelSize; ++l) {
                    i32 x = I32::clamp(static_cast<i32>(i + k), 0,
                                       static_cast<i32>(imageHeight) - 1);
                    i32 y = I32::clamp(static_cast<i32>(j + l), 0,
                                       static_cast<i32>(imageWidth) - 1);
                    sum += image[static_cast<usize>(x)][static_cast<usize>(y)] *
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
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif