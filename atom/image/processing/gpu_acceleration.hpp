#ifndef ATOM_IMAGE_GPU_ACCELERATION_HPP
#define ATOM_IMAGE_GPU_ACCELERATION_HPP

/**
 * @file gpu_acceleration.hpp
 * @brief GPU-accelerated image processing
 *
 * This module provides GPU acceleration for image processing operations
 * using CUDA, OpenCL, and other GPU computing frameworks for high-performance
 * parallel processing.
 *
 * @author Atom Framework Team
 * @date 2025
 * @version 1.0.0
 */

#include "../core/image_blob.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

namespace atom::image {

/**
 * @brief GPU computing backends
 */
enum class GPUBackend {
    CUDA,           // NVIDIA CUDA
    OPENCL,         // OpenCL
    VULKAN,         // Vulkan Compute
    METAL,          // Apple Metal
    DIRECTCOMPUTE,  // DirectCompute
    HIP,            // AMD HIP
    SYCL,           // Intel SYCL
    AUTO            // Auto-select best available
};

/**
 * @brief GPU memory types
 */
enum class GPUMemoryType {
    DEVICE,         // Device memory (GPU VRAM)
    HOST,           // Host memory (CPU RAM)
    UNIFIED,        // Unified memory (accessible by both CPU and GPU)
    PINNED          // Pinned host memory (faster transfers)
};

/**
 * @brief GPU device information
 */
struct GPUDeviceInfo {
    int deviceId;                   // Device ID
    std::string name;               // Device name
    std::string vendor;             // Vendor name
    GPUBackend backend;             // Backend type
    size_t totalMemory;             // Total memory in bytes
    size_t freeMemory;              // Free memory in bytes
    int computeUnits;               // Number of compute units
    int maxWorkGroupSize;           // Maximum work group size
    std::vector<size_t> maxWorkItemSizes; // Maximum work item sizes
    bool supportsDouble;            // Supports double precision
    bool supportsHalf;              // Supports half precision
    std::string version;            // Driver/runtime version
    std::unordered_map<std::string, std::string> extensions; // Supported extensions
};

/**
 * @brief GPU buffer for image data
 */
class GPUBuffer {
public:
    GPUBuffer() = default;
    virtual ~GPUBuffer() = default;

    /**
     * @brief Allocate GPU buffer
     * @param size Buffer size in bytes
     * @param memoryType Memory type
     * @return Success status
     */
    virtual bool allocate(size_t size, GPUMemoryType memoryType = GPUMemoryType::DEVICE) = 0;

    /**
     * @brief Upload data to GPU buffer
     * @param data Host data pointer
     * @param size Data size in bytes
     * @param offset Offset in buffer
     * @return Success status
     */
    virtual bool upload(const void* data, size_t size, size_t offset = 0) = 0;

    /**
     * @brief Download data from GPU buffer
     * @param data Host data pointer
     * @param size Data size in bytes
     * @param offset Offset in buffer
     * @return Success status
     */
    virtual bool download(void* data, size_t size, size_t offset = 0) = 0;

    /**
     * @brief Copy data between GPU buffers
     * @param src Source buffer
     * @param srcOffset Source offset
     * @param dstOffset Destination offset
     * @param size Data size
     * @return Success status
     */
    virtual bool copyFrom(const GPUBuffer& src, size_t srcOffset, size_t dstOffset, size_t size) = 0;

    /**
     * @brief Get buffer size
     * @return Buffer size in bytes
     */
    virtual size_t getSize() const = 0;

    /**
     * @brief Get memory type
     * @return Memory type
     */
    virtual GPUMemoryType getMemoryType() const = 0;

    /**
     * @brief Check if buffer is valid
     * @return True if valid
     */
    virtual bool isValid() const = 0;

    /**
     * @brief Release buffer
     */
    virtual void release() = 0;
};

/**
 * @brief GPU kernel for image processing operations
 */
class GPUKernel {
public:
    GPUKernel() = default;
    virtual ~GPUKernel() = default;

    /**
     * @brief Load kernel from source code
     * @param source Kernel source code
     * @param entryPoint Entry point function name
     * @param buildOptions Build options
     * @return Success status
     */
    virtual bool loadFromSource(const std::string& source,
                               const std::string& entryPoint,
                               const std::string& buildOptions = "") = 0;

    /**
     * @brief Load kernel from binary
     * @param binary Compiled kernel binary
     * @param entryPoint Entry point function name
     * @return Success status
     */
    virtual bool loadFromBinary(const std::vector<uint8_t>& binary,
                               const std::string& entryPoint) = 0;

    /**
     * @brief Set kernel argument
     * @param index Argument index
     * @param buffer GPU buffer
     * @return Success status
     */
    virtual bool setArgument(int index, const GPUBuffer& buffer) = 0;

    /**
     * @brief Set kernel argument (scalar value)
     * @param index Argument index
     * @param data Pointer to scalar data
     * @param size Data size
     * @return Success status
     */
    virtual bool setArgument(int index, const void* data, size_t size) = 0;

    /**
     * @brief Execute kernel
     * @param globalWorkSize Global work size
     * @param localWorkSize Local work size (empty for auto)
     * @return Success status
     */
    virtual bool execute(const std::vector<size_t>& globalWorkSize,
                        const std::vector<size_t>& localWorkSize = {}) = 0;

    /**
     * @brief Get kernel info
     * @return Kernel information
     */
    virtual std::unordered_map<std::string, std::string> getInfo() const = 0;
};

/**
 * @brief GPU context for managing GPU resources
 */
class GPUContext {
public:
    GPUContext() = default;
    virtual ~GPUContext() = default;

    /**
     * @brief Initialize GPU context
     * @param backend GPU backend
     * @param deviceId Device ID (-1 for auto-select)
     * @return Success status
     */
    virtual bool initialize(GPUBackend backend = GPUBackend::AUTO, int deviceId = -1) = 0;

    /**
     * @brief Create GPU buffer
     * @param size Buffer size in bytes
     * @param memoryType Memory type
     * @return GPU buffer pointer
     */
    virtual std::unique_ptr<GPUBuffer> createBuffer(size_t size,
                                                   GPUMemoryType memoryType = GPUMemoryType::DEVICE) = 0;

    /**
     * @brief Create GPU kernel
     * @return GPU kernel pointer
     */
    virtual std::unique_ptr<GPUKernel> createKernel() = 0;

    /**
     * @brief Synchronize GPU operations
     * @return Success status
     */
    virtual bool synchronize() = 0;

    /**
     * @brief Get device information
     * @return Device information
     */
    virtual GPUDeviceInfo getDeviceInfo() const = 0;

    /**
     * @brief Get available devices
     * @param backend GPU backend
     * @return Vector of device information
     */
    static std::vector<GPUDeviceInfo> getAvailableDevices(GPUBackend backend = GPUBackend::AUTO);

    /**
     * @brief Check if backend is available
     * @param backend GPU backend
     * @return True if available
     */
    static bool isBackendAvailable(GPUBackend backend);

    /**
     * @brief Get optimal backend for current system
     * @return Optimal GPU backend
     */
    static GPUBackend getOptimalBackend();
};

/**
 * @brief GPU-accelerated image processor
 */
class GPUImageProcessor {
public:
    GPUImageProcessor() = default;
    virtual ~GPUImageProcessor() = default;

    /**
     * @brief Initialize GPU processor
     * @param backend GPU backend
     * @param deviceId Device ID
     * @return Success status
     */
    virtual bool initialize(GPUBackend backend = GPUBackend::AUTO, int deviceId = -1);

    /**
     * @brief Upload image to GPU
     * @param image Input image blob
     * @return GPU buffer containing image data
     */
    virtual std::unique_ptr<GPUBuffer> uploadImage(const blob& image);

    /**
     * @brief Download image from GPU
     * @param buffer GPU buffer containing image data
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Downloaded image blob
     */
    virtual blob downloadImage(const GPUBuffer& buffer, int width, int height, int channels);

    /**
     * @brief Apply convolution filter on GPU
     * @param input Input image buffer
     * @param kernel Convolution kernel
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Filtered image buffer
     */
    virtual std::unique_ptr<GPUBuffer> convolve(const GPUBuffer& input,
                                               const std::vector<std::vector<float>>& kernel,
                                               int width, int height, int channels);

    /**
     * @brief Apply Gaussian blur on GPU
     * @param input Input image buffer
     * @param sigma Blur sigma
     * @param kernelSize Kernel size
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Blurred image buffer
     */
    virtual std::unique_ptr<GPUBuffer> gaussianBlur(const GPUBuffer& input,
                                                   float sigma, int kernelSize,
                                                   int width, int height, int channels);

    /**
     * @brief Resize image on GPU
     * @param input Input image buffer
     * @param srcWidth Source width
     * @param srcHeight Source height
     * @param dstWidth Destination width
     * @param dstHeight Destination height
     * @param channels Number of channels
     * @param interpolation Interpolation method
     * @return Resized image buffer
     */
    virtual std::unique_ptr<GPUBuffer> resize(const GPUBuffer& input,
                                             int srcWidth, int srcHeight,
                                             int dstWidth, int dstHeight,
                                             int channels,
                                             const std::string& interpolation = "linear");

    /**
     * @brief Apply color space conversion on GPU
     * @param input Input image buffer
     * @param fromSpace Source color space
     * @param toSpace Target color space
     * @param width Image width
     * @param height Image height
     * @return Converted image buffer
     */
    virtual std::unique_ptr<GPUBuffer> convertColorSpace(const GPUBuffer& input,
                                                        const std::string& fromSpace,
                                                        const std::string& toSpace,
                                                        int width, int height);

    /**
     * @brief Apply histogram equalization on GPU
     * @param input Input image buffer
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Equalized image buffer
     */
    virtual std::unique_ptr<GPUBuffer> equalizeHistogram(const GPUBuffer& input,
                                                        int width, int height, int channels);

    /**
     * @brief Apply morphological operation on GPU
     * @param input Input image buffer
     * @param operation Operation type ("erode", "dilate", "open", "close")
     * @param structElement Structuring element
     * @param width Image width
     * @param height Image height
     * @param channels Number of channels
     * @return Processed image buffer
     */
    virtual std::unique_ptr<GPUBuffer> morphological(const GPUBuffer& input,
                                                    const std::string& operation,
                                                    const std::vector<std::vector<int>>& structElement,
                                                    int width, int height, int channels);

    /**
     * @brief Apply edge detection on GPU
     * @param input Input image buffer
     * @param method Edge detection method ("sobel", "canny", "laplacian")
     * @param threshold1 Lower threshold
     * @param threshold2 Upper threshold
     * @param width Image width
     * @param height Image height
     * @return Edge image buffer
     */
    virtual std::unique_ptr<GPUBuffer> detectEdges(const GPUBuffer& input,
                                                  const std::string& method,
                                                  float threshold1, float threshold2,
                                                  int width, int height);

    /**
     * @brief Apply custom kernel on GPU
     * @param input Input image buffer
     * @param kernelSource Kernel source code
     * @param entryPoint Kernel entry point
     * @param globalWorkSize Global work size
     * @param localWorkSize Local work size
     * @param args Additional kernel arguments
     * @return Processed image buffer
     */
    virtual std::unique_ptr<GPUBuffer> applyCustomKernel(const GPUBuffer& input,
                                                        const std::string& kernelSource,
                                                        const std::string& entryPoint,
                                                        const std::vector<size_t>& globalWorkSize,
                                                        const std::vector<size_t>& localWorkSize = {},
                                                        const std::vector<float>& args = {});

    /**
     * @brief Batch process multiple images on GPU
     * @param inputs Vector of input image buffers
     * @param operation Operation to apply
     * @param params Operation parameters
     * @return Vector of processed image buffers
     */
    virtual std::vector<std::unique_ptr<GPUBuffer>> batchProcess(
        const std::vector<std::unique_ptr<GPUBuffer>>& inputs,
        const std::string& operation,
        const std::unordered_map<std::string, float>& params = {});

    /**
     * @brief Get GPU context
     * @return GPU context pointer
     */
    virtual GPUContext* getContext() const;

    /**
     * @brief Get performance statistics
     * @return Performance statistics
     */
    virtual std::unordered_map<std::string, double> getPerformanceStats() const;

    /**
     * @brief Benchmark GPU operations
     * @param operation Operation to benchmark
     * @param imageSize Image size for testing
     * @param iterations Number of iterations
     * @return Benchmark results
     */
    virtual std::unordered_map<std::string, double> benchmark(const std::string& operation,
                                                             const std::pair<int, int>& imageSize,
                                                             int iterations = 100);

protected:
    /**
     * @brief Load built-in kernels
     * @return Success status
     */
    virtual bool loadBuiltinKernels();

    /**
     * @brief Get kernel source for operation
     * @param operation Operation name
     * @return Kernel source code
     */
    virtual std::string getKernelSource(const std::string& operation) const;

    /**
     * @brief Optimize kernel parameters
     * @param operation Operation name
     * @param imageSize Image size
     * @return Optimal parameters
     */
    virtual std::unordered_map<std::string, size_t> optimizeKernelParams(
        const std::string& operation,
        const std::pair<int, int>& imageSize) const;

private:
    std::unique_ptr<GPUContext> context_;
    std::unordered_map<std::string, std::unique_ptr<GPUKernel>> kernels_;
    mutable std::unordered_map<std::string, double> performanceStats_;
};

/**
 * @brief Factory function to create optimal GPU processor
 * @param backend Preferred GPU backend
 * @param deviceId Device ID (-1 for auto-select)
 * @return Unique pointer to GPU processor
 */
std::unique_ptr<GPUImageProcessor> createOptimalGPUProcessor(GPUBackend backend = GPUBackend::AUTO,
                                                            int deviceId = -1);

} // namespace atom::image

#endif // ATOM_IMAGE_GPU_ACCELERATION_HPP
