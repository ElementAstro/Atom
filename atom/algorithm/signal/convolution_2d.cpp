/*
 * convolution_2d.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Implementation of two-dimensional convolution and deconvolution
with optional OpenCL support.

**************************************************/

#include "convolution_2d.hpp"
#include "atom/algorithm/core/rust_numeric.hpp"

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
auto pad2DImpl(const std::vector<std::vector<T>>& input, usize padTop,
               usize padBottom, usize padLeft, usize padRight,
               PaddingMode mode) -> std::vector<std::vector<T>> {
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
auto getConvolutionOutputDimensions(
    usize inputHeight, usize inputWidth, usize kernelHeight, usize kernelWidth,
    usize strideY, usize strideX,
    PaddingMode paddingMode) -> std::pair<usize, usize> {
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

auto createProgram(const std::string& source,
                   cl_context context) -> CLProgramPtr {
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

// OpenCL kernel code for 2D convolution
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

        if (inputRows == 0 || inputCols == 0 || kernelRows == 0 ||
            kernelCols == 0) {
            THROW_CONVOLVE_ERROR("Input and kernel matrices must not be empty");
        }

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

        std::vector<f32> inputFlattened(inputRows * inputCols);
        std::vector<f32> kernelFlattened(kernelRows * kernelCols);
        std::vector<f32> outputFlattened(inputRows * inputCols, 0.0f);

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

        auto program = createProgram(convolve2DKernelSrc, context.get());
        err = clBuildProgram(program.get(), 0, nullptr, nullptr, nullptr,
                             nullptr);

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

        CLKernelPtr openclKernel(
            clCreateKernel(program.get(), "convolve2D", &err));
        checkErr(err, "Creating kernel");

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

        usize globalWorkSize[2] = {inputRows, inputCols};
        err = clEnqueueNDRangeKernel(queue.get(), openclKernel.get(), 2,
                                     nullptr, globalWorkSize, nullptr, 0,
                                     nullptr, nullptr);
        checkErr(err, "Enqueueing kernel");

        clFinish(queue.get());

        err = clEnqueueReadBuffer(queue.get(), outputBuffer.get(), CL_TRUE, 0,
                                  sizeof(f32) * outputFlattened.size(),
                                  outputFlattened.data(), 0, nullptr, nullptr);
        checkErr(err, "Reading back output buffer");

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
        THROW_CONVOLVE_ERROR("OpenCL convolution failed: {}", e.what());
    }
}

// OpenCL deconvolution
auto deconvolve2DOpenCL(const std::vector<std::vector<f64>>& signal,
                        const std::vector<std::vector<f64>>& kernel,
                        i32 numThreads) -> std::vector<std::vector<f64>> {
    try {
        return deconvolve2D(signal, kernel, numThreads);
    } catch (const std::exception& e) {
        THROW_CONVOLVE_ERROR("OpenCL deconvolution failed: {}", e.what());
    }
}
#endif

// Function to convolve a 2D input with a 2D kernel using multithreading or
// OpenCL
auto convolve2D(const std::vector<std::vector<f64>>& input,
                const std::vector<std::vector<f64>>& kernel,
                i32 numThreads) -> std::vector<std::vector<f64>> {
    try {
        if (input.empty() || input[0].empty()) {
            THROW_CONVOLVE_ERROR("Input matrix cannot be empty");
        }
        if (kernel.empty() || kernel[0].empty()) {
            THROW_CONVOLVE_ERROR("Kernel matrix cannot be empty");
        }

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

        // Check if kernel is larger than input
        const usize inputRows = input.size();
        const usize kernelRows = kernel.size();
        if (kernelRows > inputRows || kernelCols > inputCols) {
            THROW_CONVOLVE_ERROR(
                "Kernel dimensions ({},{}) cannot be larger than input "
                "dimensions ({},{})",
                kernelRows, kernelCols, inputRows, inputCols);
        }

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
        auto extendedInput = extend2D(input, inputRows + kernelRows - 1,
                                      inputCols + kernelCols - 1);
        auto extendedKernel = extend2D(kernel, inputRows + kernelRows - 1,
                                       inputCols + kernelCols - 1);

        std::vector<std::vector<f64>> output(inputRows,
                                             std::vector<f64>(inputCols, 0.0));

        auto computeBlock = [&](usize blockStartRow, usize blockEndRow) {
            const usize halfKernelRows = kernelRows / 2;
            const usize halfKernelCols = kernelCols / 2;

            for (usize i = blockStartRow; i < blockEndRow; ++i) {
                for (usize j = 0; j < inputCols; ++j) {
                    f64 sum = 0.0;

#ifdef ATOM_ATOM_USE_SIMD
                    for (usize ki = 0; ki < kernelRows; ++ki) {
                        for (usize kj = 0; kj < kernelCols; ++kj) {
                            i32 ii = static_cast<i32>(i) +
                                     static_cast<i32>(ki) -
                                     static_cast<i32>(halfKernelRows);
                            i32 jj = static_cast<i32>(j) +
                                     static_cast<i32>(kj) -
                                     static_cast<i32>(halfKernelCols);
                            if (ii >= 0 && ii < static_cast<i32>(inputRows) &&
                                jj >= 0 && jj < static_cast<i32>(inputCols)) {
                                sum += input[static_cast<usize>(ii)]
                                            [static_cast<usize>(jj)] *
                                       kernel[ki][kj];
                            }
                        }
                    }
#else
                    for (usize ki = 0; ki < kernelRows; ++ki) {
                        for (usize kj = 0; kj < kernelCols; ++kj) {
                            i32 ii = static_cast<i32>(i) +
                                     static_cast<i32>(ki) -
                                     static_cast<i32>(halfKernelRows);
                            i32 jj = static_cast<i32>(j) +
                                     static_cast<i32>(kj) -
                                     static_cast<i32>(halfKernelCols);
                            if (ii >= 0 && ii < static_cast<i32>(inputRows) &&
                                jj >= 0 && jj < static_cast<i32>(inputCols)) {
                                sum += input[static_cast<usize>(ii)]
                                            [static_cast<usize>(jj)] *
                                       kernel[ki][kj];
                            }
                        }
                    }
#endif
                    output[i][j] = sum;
                }
            }
        };

        if (numThreads > 1) {
            std::vector<std::jthread> threadPool;
            usize blockSize = (inputRows + static_cast<usize>(numThreads) - 1) /
                              static_cast<usize>(numThreads);

            for (i32 threadIndex = 0; threadIndex < numThreads; ++threadIndex) {
                usize startRow = static_cast<usize>(threadIndex) * blockSize;
                usize endRow = Usize::min(startRow + blockSize, inputRows);

                threadPool.emplace_back(computeBlock, startRow, endRow);
            }

            // jthread auto-joins
        } else {
            computeBlock(0, inputRows);
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
                  const std::vector<std::vector<f64>>& kernel,
                  i32 numThreads) -> std::vector<std::vector<f64>> {
    try {
        if (signal.empty() || signal[0].empty()) {
            THROW_CONVOLVE_ERROR("Signal matrix cannot be empty");
        }
        if (kernel.empty() || kernel[0].empty()) {
            THROW_CONVOLVE_ERROR("Kernel matrix cannot be empty");
        }

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
                return dft2D(input, numThreads);
            };

        auto frequencySignal = discreteFourierTransform2D(extendedSignal);
        auto frequencyKernel = discreteFourierTransform2D(extendedKernel);

        std::vector<std::vector<std::complex<f64>>> frequencyProduct(
            signalRows + kernelRows - 1,
            std::vector<std::complex<f64>>(signalCols + kernelCols - 1,
                                           {0, 0}));

#ifdef ATOM_ATOM_USE_SIMD
        const i32 simdWidth = SIMD_WIDTH;
        __m256d epsilon_vec = _mm256_set1_pd(EPSILON);

        for (usize u = 0; u < signalRows + kernelRows - 1; ++u) {
            for (usize v = 0; v < signalCols + kernelCols - 1;
                 v += static_cast<usize>(simdWidth)) {
                __m256d signalReal =
                    _mm256_loadu_pd(&frequencySignal[u][v].real());
                __m256d signalImag =
                    _mm256_loadu_pd(&frequencySignal[u][v].imag());
                __m256d kernelReal =
                    _mm256_loadu_pd(&frequencyKernel[u][v].real());
                __m256d kernelImag =
                    _mm256_loadu_pd(&frequencyKernel[u][v].imag());

                __m256d norm =
                    _mm256_add_pd(_mm256_mul_pd(kernelReal, kernelReal),
                                  _mm256_mul_pd(kernelImag, kernelImag));
                norm = _mm256_add_pd(norm, epsilon_vec);

                __m256d resultReal = _mm256_div_pd(
                    _mm256_add_pd(_mm256_mul_pd(signalReal, kernelReal),
                                  _mm256_mul_pd(signalImag, kernelImag)),
                    norm);
                __m256d resultImag = _mm256_div_pd(
                    _mm256_sub_pd(_mm256_mul_pd(signalImag, kernelReal),
                                  _mm256_mul_pd(signalReal, kernelImag)),
                    norm);

                _mm256_storeu_pd(&frequencyProduct[u][v].real(), resultReal);
                _mm256_storeu_pd(&frequencyProduct[u][v].imag(), resultImag);
            }

            // Handle remaining elements
            for (usize v = ((signalCols + kernelCols - 1) /
                            static_cast<usize>(simdWidth)) *
                           static_cast<usize>(simdWidth);
                 v < signalCols + kernelCols - 1; ++v) {
                auto norm = std::norm(frequencyKernel[u][v]) + EPSILON;
                frequencyProduct[u][v] = frequencySignal[u][v] *
                                         std::conj(frequencyKernel[u][v]) /
                                         norm;
            }
        }
#else
        // Fallback to non-SIMD version
        for (usize u = 0; u < signalRows + kernelRows - 1; ++u) {
            for (usize v = 0; v < signalCols + kernelCols - 1; ++v) {
                auto norm = std::norm(frequencyKernel[u][v]) + EPSILON;
                frequencyProduct[u][v] = frequencySignal[u][v] *
                                         std::conj(frequencyKernel[u][v]) /
                                         norm;
            }
        }
#endif

        std::vector<std::vector<f64>> frequencyInverse =
            idft2D(frequencyProduct, numThreads);

        // Extract the relevant portion
        std::vector<std::vector<f64>> result(signalRows,
                                             std::vector<f64>(signalCols, 0.0));
        for (usize i = 0; i < signalRows; ++i) {
            for (usize j = 0; j < signalCols; ++j) {
                result[i][j] = frequencyInverse[i][j];
            }
        }

        return result;
#endif
    } catch (const std::exception& e) {
        THROW_CONVOLVE_ERROR("2D deconvolution failed: {}", e.what());
    }
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
