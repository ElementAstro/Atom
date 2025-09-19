#include "opencl_utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <sstream>

#include "../../error/exception.hpp"

namespace atom::algorithm::opencl {

#if ATOM_OPENCL_AVAILABLE

auto Platform::getPlatforms() -> std::vector<cl_platform_id> {
    cl_uint num_platforms;
    cl_int err = clGetPlatformIDs(0, nullptr, &num_platforms);
    if (err != CL_SUCCESS || num_platforms == 0) {
        return {};
    }
    
    std::vector<cl_platform_id> platforms(num_platforms);
    err = clGetPlatformIDs(num_platforms, platforms.data(), nullptr);
    if (err != CL_SUCCESS) {
        return {};
    }
    
    return platforms;
}

auto Platform::getDevices(cl_platform_id platform, DeviceType device_type) -> std::vector<cl_device_id> {
    cl_uint num_devices;
    cl_int err = clGetDeviceIDs(platform, static_cast<cl_device_type>(device_type), 
                               0, nullptr, &num_devices);
    if (err != CL_SUCCESS || num_devices == 0) {
        return {};
    }
    
    std::vector<cl_device_id> devices(num_devices);
    err = clGetDeviceIDs(platform, static_cast<cl_device_type>(device_type), 
                        num_devices, devices.data(), nullptr);
    if (err != CL_SUCCESS) {
        return {};
    }
    
    return devices;
}

auto Platform::getDeviceInfo(cl_device_id device) -> DeviceInfo {
    DeviceInfo info;
    
    // Get device name
    usize name_size;
    clGetDeviceInfo(device, CL_DEVICE_NAME, 0, nullptr, &name_size);
    std::string name(name_size, '\0');
    clGetDeviceInfo(device, CL_DEVICE_NAME, name_size, name.data(), nullptr);
    info.name = name.c_str(); // Remove null terminator
    
    // Get vendor
    usize vendor_size;
    clGetDeviceInfo(device, CL_DEVICE_VENDOR, 0, nullptr, &vendor_size);
    std::string vendor(vendor_size, '\0');
    clGetDeviceInfo(device, CL_DEVICE_VENDOR, vendor_size, vendor.data(), nullptr);
    info.vendor = vendor.c_str();
    
    // Get version
    usize version_size;
    clGetDeviceInfo(device, CL_DEVICE_VERSION, 0, nullptr, &version_size);
    std::string version(version_size, '\0');
    clGetDeviceInfo(device, CL_DEVICE_VERSION, version_size, version.data(), nullptr);
    info.version = version.c_str();
    
    // Get device type
    cl_device_type type;
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(type), &type, nullptr);
    info.type = static_cast<DeviceType>(type);
    
    // Get compute units
    cl_uint compute_units;
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, nullptr);
    info.max_compute_units = compute_units;
    
    // Get max work group size
    usize work_group_size;
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(work_group_size), &work_group_size, nullptr);
    info.max_work_group_size = work_group_size;
    
    // Get global memory size
    cl_ulong global_mem_size;
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem_size), &global_mem_size, nullptr);
    info.global_memory_size = global_mem_size;
    
    // Get local memory size
    cl_ulong local_mem_size;
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(local_mem_size), &local_mem_size, nullptr);
    info.local_memory_size = local_mem_size;
    
    // Check double precision support
    usize extensions_size;
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &extensions_size);
    std::string extensions(extensions_size, '\0');
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, extensions_size, extensions.data(), nullptr);
    info.supports_double = extensions.find("cl_khr_fp64") != std::string::npos;
    
    return info;
}

auto Platform::createContext(const std::vector<cl_device_id>& devices) -> Context {
    cl_int err;
    cl_context context = clCreateContext(nullptr, static_cast<cl_uint>(devices.size()), 
                                        devices.data(), nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL context: error {}", err);
    }
    
    return Context(context);
}

auto Platform::createCommandQueue(const Context& context, cl_device_id device) -> CommandQueue {
    cl_int err;
    cl_command_queue queue = clCreateCommandQueue(context.get(), device, 0, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL command queue: error {}", err);
    }
    
    return CommandQueue(queue);
}

auto Platform::createBuffer(const Context& context, MemoryFlags flags, usize size, void* host_ptr) -> Buffer {
    cl_int err;
    cl_mem buffer = clCreateBuffer(context.get(), static_cast<cl_mem_flags>(flags), 
                                  size, host_ptr, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL buffer: error {}", err);
    }
    
    return Buffer(buffer);
}

auto Platform::buildKernel(const Context& context,
                          const std::vector<cl_device_id>& devices,
                          const std::string& source,
                          const std::string& kernel_name,
                          const std::string& build_options) -> Kernel {
    cl_int err;
    
    // Create program from source
    const char* source_ptr = source.c_str();
    usize source_size = source.length();
    cl_program program = clCreateProgramWithSource(context.get(), 1, &source_ptr, &source_size, &err);
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL program: error {}", err);
    }
    
    // Build program
    err = clBuildProgram(program, static_cast<cl_uint>(devices.size()), devices.data(), 
                        build_options.empty() ? nullptr : build_options.c_str(), nullptr, nullptr);
    
    if (err != CL_SUCCESS) {
        // Get build log for debugging
        usize log_size;
        clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size);
        std::string build_log(log_size, '\0');
        clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, log_size, build_log.data(), nullptr);
        
        clReleaseProgram(program);
        THROW_RUNTIME_ERROR("Failed to build OpenCL program: error {}\nBuild log: {}", err, build_log);
    }
    
    // Create kernel
    cl_kernel kernel = clCreateKernel(program, kernel_name.c_str(), &err);
    clReleaseProgram(program); // Release program as kernel holds reference
    
    if (err != CL_SUCCESS) {
        THROW_RUNTIME_ERROR("Failed to create OpenCL kernel '{}': error {}", kernel_name, err);
    }
    
    return Kernel(kernel);
}

auto ComputeManager::initialize(DeviceType preferred_type) -> bool {
    if (initialized_) {
        return true;
    }
    
    try {
        auto platforms = Platform::getPlatforms();
        if (platforms.empty()) {
            return false;
        }
        
        // Try to find a device of the preferred type
        cl_device_id best_device = nullptr;
        std::vector<cl_device_id> context_devices;
        
        for (auto platform : platforms) {
            auto devices = Platform::getDevices(platform, preferred_type);
            if (!devices.empty()) {
                best_device = devices[0];
                context_devices = {best_device};
                break;
            }
        }
        
        // If preferred type not found, try any device
        if (!best_device) {
            for (auto platform : platforms) {
                auto devices = Platform::getDevices(platform, DeviceType::ALL);
                if (!devices.empty()) {
                    best_device = devices[0];
                    context_devices = {best_device};
                    break;
                }
            }
        }
        
        if (!best_device) {
            return false;
        }
        
        // Create context and command queue
        context_ = Platform::createContext(context_devices);
        queue_ = Platform::createCommandQueue(context_, best_device);
        device_ = best_device;
        device_info_ = Platform::getDeviceInfo(best_device);
        
        initialized_ = true;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

auto ComputeManager::isAvailable() const noexcept -> bool {
    return initialized_;
}

auto ComputeManager::getDeviceInfo() const -> const DeviceInfo& {
    return device_info_;
}

auto ComputeManager::getInstance() -> ComputeManager& {
    static ComputeManager instance;
    return instance;
}

#endif // ATOM_OPENCL_AVAILABLE

} // namespace atom::algorithm::opencl
