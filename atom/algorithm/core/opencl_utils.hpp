#ifndef ATOM_ALGORITHM_CORE_OPENCL_UTILS_HPP
#define ATOM_ALGORITHM_CORE_OPENCL_UTILS_HPP

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "rust_numeric.hpp"

// OpenCL availability check
#ifdef ATOM_USE_OPENCL
    #ifdef __APPLE__
        #include <OpenCL/opencl.h>
    #else
        #include <CL/cl.h>
    #endif
    #define ATOM_OPENCL_AVAILABLE 1
#else
    #define ATOM_OPENCL_AVAILABLE 0
#endif

namespace atom::algorithm::opencl {

#if ATOM_OPENCL_AVAILABLE

/**
 * @brief OpenCL device types
 */
enum class DeviceType {
    CPU = CL_DEVICE_TYPE_CPU,
    GPU = CL_DEVICE_TYPE_GPU,
    ACCELERATOR = CL_DEVICE_TYPE_ACCELERATOR,
    ALL = CL_DEVICE_TYPE_ALL
};

/**
 * @brief OpenCL memory flags
 */
enum class MemoryFlags {
    READ_ONLY = CL_MEM_READ_ONLY,
    WRITE_ONLY = CL_MEM_WRITE_ONLY,
    READ_WRITE = CL_MEM_READ_WRITE,
    USE_HOST_PTR = CL_MEM_USE_HOST_PTR,
    ALLOC_HOST_PTR = CL_MEM_ALLOC_HOST_PTR,
    COPY_HOST_PTR = CL_MEM_COPY_HOST_PTR
};

/**
 * @brief RAII wrapper for OpenCL context
 */
class Context {
public:
    Context() = default;
    explicit Context(cl_context context) : context_(context) {}
    
    ~Context() {
        if (context_) {
            clReleaseContext(context_);
        }
    }
    
    // Move semantics
    Context(Context&& other) noexcept : context_(other.context_) {
        other.context_ = nullptr;
    }
    
    Context& operator=(Context&& other) noexcept {
        if (this != &other) {
            if (context_) {
                clReleaseContext(context_);
            }
            context_ = other.context_;
            other.context_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy semantics
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    
    [[nodiscard]] cl_context get() const noexcept { return context_; }
    [[nodiscard]] bool valid() const noexcept { return context_ != nullptr; }

private:
    cl_context context_ = nullptr;
};

/**
 * @brief RAII wrapper for OpenCL command queue
 */
class CommandQueue {
public:
    CommandQueue() = default;
    explicit CommandQueue(cl_command_queue queue) : queue_(queue) {}
    
    ~CommandQueue() {
        if (queue_) {
            clReleaseCommandQueue(queue_);
        }
    }
    
    // Move semantics
    CommandQueue(CommandQueue&& other) noexcept : queue_(other.queue_) {
        other.queue_ = nullptr;
    }
    
    CommandQueue& operator=(CommandQueue&& other) noexcept {
        if (this != &other) {
            if (queue_) {
                clReleaseCommandQueue(queue_);
            }
            queue_ = other.queue_;
            other.queue_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy semantics
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    
    [[nodiscard]] cl_command_queue get() const noexcept { return queue_; }
    [[nodiscard]] bool valid() const noexcept { return queue_ != nullptr; }

private:
    cl_command_queue queue_ = nullptr;
};

/**
 * @brief RAII wrapper for OpenCL memory buffer
 */
class Buffer {
public:
    Buffer() = default;
    explicit Buffer(cl_mem buffer) : buffer_(buffer) {}
    
    ~Buffer() {
        if (buffer_) {
            clReleaseMemObject(buffer_);
        }
    }
    
    // Move semantics
    Buffer(Buffer&& other) noexcept : buffer_(other.buffer_) {
        other.buffer_ = nullptr;
    }
    
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            if (buffer_) {
                clReleaseMemObject(buffer_);
            }
            buffer_ = other.buffer_;
            other.buffer_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy semantics
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    
    [[nodiscard]] cl_mem get() const noexcept { return buffer_; }
    [[nodiscard]] bool valid() const noexcept { return buffer_ != nullptr; }

private:
    cl_mem buffer_ = nullptr;
};

/**
 * @brief RAII wrapper for OpenCL kernel
 */
class Kernel {
public:
    Kernel() = default;
    explicit Kernel(cl_kernel kernel) : kernel_(kernel) {}
    
    ~Kernel() {
        if (kernel_) {
            clReleaseKernel(kernel_);
        }
    }
    
    // Move semantics
    Kernel(Kernel&& other) noexcept : kernel_(other.kernel_) {
        other.kernel_ = nullptr;
    }
    
    Kernel& operator=(Kernel&& other) noexcept {
        if (this != &other) {
            if (kernel_) {
                clReleaseKernel(kernel_);
            }
            kernel_ = other.kernel_;
            other.kernel_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy semantics
    Kernel(const Kernel&) = delete;
    Kernel& operator=(const Kernel&) = delete;
    
    [[nodiscard]] cl_kernel get() const noexcept { return kernel_; }
    [[nodiscard]] bool valid() const noexcept { return kernel_ != nullptr; }

private:
    cl_kernel kernel_ = nullptr;
};

/**
 * @brief OpenCL device information
 */
struct DeviceInfo {
    std::string name;
    std::string vendor;
    std::string version;
    DeviceType type;
    usize max_compute_units;
    usize max_work_group_size;
    usize global_memory_size;
    usize local_memory_size;
    bool supports_double;
};

/**
 * @brief OpenCL platform manager and utility functions
 */
class Platform {
public:
    /**
     * @brief Get available OpenCL platforms
     * @return Vector of platform IDs
     */
    [[nodiscard]] static auto getPlatforms() -> std::vector<cl_platform_id>;
    
    /**
     * @brief Get devices for a platform
     * @param platform Platform ID
     * @param device_type Type of devices to query
     * @return Vector of device IDs
     */
    [[nodiscard]] static auto getDevices(cl_platform_id platform, 
                                        DeviceType device_type = DeviceType::ALL) 
                                        -> std::vector<cl_device_id>;
    
    /**
     * @brief Get device information
     * @param device Device ID
     * @return Device information structure
     */
    [[nodiscard]] static auto getDeviceInfo(cl_device_id device) -> DeviceInfo;
    
    /**
     * @brief Create OpenCL context
     * @param devices Vector of device IDs
     * @return Context wrapper
     */
    [[nodiscard]] static auto createContext(const std::vector<cl_device_id>& devices) -> Context;
    
    /**
     * @brief Create command queue
     * @param context OpenCL context
     * @param device Device ID
     * @return CommandQueue wrapper
     */
    [[nodiscard]] static auto createCommandQueue(const Context& context, 
                                                 cl_device_id device) -> CommandQueue;
    
    /**
     * @brief Create buffer
     * @param context OpenCL context
     * @param flags Memory flags
     * @param size Buffer size in bytes
     * @param host_ptr Optional host pointer
     * @return Buffer wrapper
     */
    [[nodiscard]] static auto createBuffer(const Context& context, 
                                          MemoryFlags flags, 
                                          usize size, 
                                          void* host_ptr = nullptr) -> Buffer;
    
    /**
     * @brief Build kernel from source
     * @param context OpenCL context
     * @param devices Vector of device IDs
     * @param source Kernel source code
     * @param kernel_name Name of the kernel function
     * @param build_options Optional build options
     * @return Kernel wrapper
     */
    [[nodiscard]] static auto buildKernel(const Context& context,
                                          const std::vector<cl_device_id>& devices,
                                          const std::string& source,
                                          const std::string& kernel_name,
                                          const std::string& build_options = "") -> Kernel;
};

/**
 * @brief High-level OpenCL compute manager
 */
class ComputeManager {
public:
    /**
     * @brief Initialize OpenCL with best available device
     * @param preferred_type Preferred device type
     * @return true if initialization succeeded
     */
    [[nodiscard]] auto initialize(DeviceType preferred_type = DeviceType::GPU) -> bool;
    
    /**
     * @brief Check if OpenCL is available and initialized
     * @return true if available
     */
    [[nodiscard]] auto isAvailable() const noexcept -> bool;
    
    /**
     * @brief Get device information
     * @return Device information
     */
    [[nodiscard]] auto getDeviceInfo() const -> const DeviceInfo&;
    
    /**
     * @brief Execute a simple kernel with automatic buffer management
     * @param kernel_source OpenCL kernel source code
     * @param kernel_name Name of the kernel function
     * @param global_work_size Global work size
     * @param local_work_size Local work size (optional)
     * @param args Kernel arguments
     * @return true if execution succeeded
     */
    template <typename... Args>
    [[nodiscard]] auto executeKernel(const std::string& kernel_source,
                                     const std::string& kernel_name,
                                     usize global_work_size,
                                     usize local_work_size,
                                     Args&&... args) -> bool;
    
    /**
     * @brief Get singleton instance
     * @return Reference to singleton instance
     */
    [[nodiscard]] static auto getInstance() -> ComputeManager&;

private:
    ComputeManager() = default;
    
    Context context_;
    CommandQueue queue_;
    cl_device_id device_ = nullptr;
    DeviceInfo device_info_;
    bool initialized_ = false;
    
    std::unordered_map<std::string, Kernel> kernel_cache_;
};

#else // !ATOM_OPENCL_AVAILABLE

/**
 * @brief Stub implementations when OpenCL is not available
 */
class ComputeManager {
public:
    [[nodiscard]] auto initialize(int = 0) -> bool { return false; }
    [[nodiscard]] auto isAvailable() const noexcept -> bool { return false; }
    [[nodiscard]] static auto getInstance() -> ComputeManager& {
        static ComputeManager instance;
        return instance;
    }
};

#endif // ATOM_OPENCL_AVAILABLE

} // namespace atom::algorithm::opencl

#endif // ATOM_ALGORITHM_CORE_OPENCL_UTILS_HPP
