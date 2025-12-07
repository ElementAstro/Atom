/*
 * opencl_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * Example demonstrating OpenCL utilities from
 * atom/algorithm/core/opencl_utils.hpp
 */

#include "atom/algorithm/core/opencl_utils.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

using namespace atom::algorithm;
using namespace atom::algorithm::opencl;

#if ATOM_OPENCL_AVAILABLE

// Demonstrate platform and device enumeration
void demonstratePlatformEnumeration() {
    std::cout << "\n=== OpenCL Platform Enumeration ===\n";

    auto platforms = Platform::getPlatforms();
    std::cout << "Found " << platforms.size() << " OpenCL platform(s)\n";

    for (usize i = 0; i < platforms.size(); ++i) {
        std::cout << "\nPlatform " << i << ":\n";

        auto devices = Platform::getDevices(platforms[i], DeviceType::ALL);
        std::cout << "  Found " << devices.size() << " device(s)\n";

        for (usize j = 0; j < devices.size(); ++j) {
            auto info = Platform::getDeviceInfo(devices[j]);
            std::cout << "\n  Device " << j << ":\n";
            std::cout << "    Name: " << info.name << "\n";
            std::cout << "    Vendor: " << info.vendor << "\n";
            std::cout << "    Version: " << info.version << "\n";
            std::cout << "    Type: ";
            switch (info.type) {
                case DeviceType::CPU:
                    std::cout << "CPU";
                    break;
                case DeviceType::GPU:
                    std::cout << "GPU";
                    break;
                case DeviceType::ACCELERATOR:
                    std::cout << "Accelerator";
                    break;
                default:
                    std::cout << "Unknown";
                    break;
            }
            std::cout << "\n";
            std::cout << "    Max compute units: " << info.max_compute_units
                      << "\n";
            std::cout << "    Max work group size: " << info.max_work_group_size
                      << "\n";
            std::cout << "    Global memory: "
                      << (info.global_memory_size / (1024 * 1024)) << " MB\n";
            std::cout << "    Local memory: " << (info.local_memory_size / 1024)
                      << " KB\n";
            std::cout << "    Double precision: "
                      << (info.supports_double ? "Yes" : "No") << "\n";
        }
    }
}

// Demonstrate context and command queue creation
void demonstrateContextCreation() {
    std::cout << "\n=== OpenCL Context Creation ===\n";

    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        std::cout << "No OpenCL platforms available\n";
        return;
    }

    // Try to get GPU devices first, fall back to CPU
    auto devices = Platform::getDevices(platforms[0], DeviceType::GPU);
    if (devices.empty()) {
        devices = Platform::getDevices(platforms[0], DeviceType::CPU);
    }

    if (devices.empty()) {
        std::cout << "No OpenCL devices available\n";
        return;
    }

    std::cout << "Creating context with " << devices.size()
              << " device(s)...\n";
    auto context = Platform::createContext(devices);

    if (context.valid()) {
        std::cout << "Context created successfully\n";

        std::cout << "Creating command queue...\n";
        auto queue = Platform::createCommandQueue(context, devices[0]);

        if (queue.valid()) {
            std::cout << "Command queue created successfully\n";
        } else {
            std::cout << "Failed to create command queue\n";
        }
    } else {
        std::cout << "Failed to create context\n";
    }
}

// Demonstrate buffer operations
void demonstrateBufferOperations() {
    std::cout << "\n=== OpenCL Buffer Operations ===\n";

    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        std::cout << "No OpenCL platforms available\n";
        return;
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::GPU);
    if (devices.empty()) {
        devices = Platform::getDevices(platforms[0], DeviceType::CPU);
    }

    if (devices.empty()) {
        std::cout << "No OpenCL devices available\n";
        return;
    }

    auto context = Platform::createContext(devices);
    if (!context.valid()) {
        std::cout << "Failed to create context\n";
        return;
    }

    // Create buffers
    constexpr usize BUFFER_SIZE = 1024 * sizeof(f32);

    std::cout << "Creating read-only buffer (" << BUFFER_SIZE << " bytes)...\n";
    auto read_buffer =
        Platform::createBuffer(context, MemoryFlags::READ_ONLY, BUFFER_SIZE);

    std::cout << "Creating write-only buffer (" << BUFFER_SIZE
              << " bytes)...\n";
    auto write_buffer =
        Platform::createBuffer(context, MemoryFlags::WRITE_ONLY, BUFFER_SIZE);

    std::cout << "Creating read-write buffer (" << BUFFER_SIZE
              << " bytes)...\n";
    auto rw_buffer =
        Platform::createBuffer(context, MemoryFlags::READ_WRITE, BUFFER_SIZE);

    std::cout << "Read buffer valid: " << (read_buffer.valid() ? "Yes" : "No")
              << "\n";
    std::cout << "Write buffer valid: " << (write_buffer.valid() ? "Yes" : "No")
              << "\n";
    std::cout << "RW buffer valid: " << (rw_buffer.valid() ? "Yes" : "No")
              << "\n";
}

// Demonstrate kernel compilation and execution
void demonstrateKernelExecution() {
    std::cout << "\n=== OpenCL Kernel Execution ===\n";

    auto platforms = Platform::getPlatforms();
    if (platforms.empty()) {
        std::cout << "No OpenCL platforms available\n";
        return;
    }

    auto devices = Platform::getDevices(platforms[0], DeviceType::GPU);
    if (devices.empty()) {
        devices = Platform::getDevices(platforms[0], DeviceType::CPU);
    }

    if (devices.empty()) {
        std::cout << "No OpenCL devices available\n";
        return;
    }

    auto context = Platform::createContext(devices);
    if (!context.valid()) {
        std::cout << "Failed to create context\n";
        return;
    }

    // Simple vector addition kernel
    const std::string kernel_source = R"(
        __kernel void vector_add(__global const float* a,
                                 __global const float* b,
                                 __global float* result,
                                 const unsigned int n) {
            int id = get_global_id(0);
            if (id < n) {
                result[id] = a[id] + b[id];
            }
        }
    )";

    std::cout << "Building kernel...\n";
    auto kernel =
        Platform::buildKernel(context, devices, kernel_source, "vector_add");

    if (kernel.valid()) {
        std::cout << "Kernel built successfully\n";
    } else {
        std::cout << "Failed to build kernel\n";
    }
}

// Demonstrate ComputeManager singleton
void demonstrateComputeManager() {
    std::cout << "\n=== OpenCL Compute Manager ===\n";

    auto& manager = ComputeManager::getInstance();

    std::cout << "Initializing compute manager...\n";
    bool initialized = manager.initialize(DeviceType::GPU);

    if (initialized) {
        std::cout << "Compute manager initialized successfully\n";
        std::cout << "OpenCL available: "
                  << (manager.isAvailable() ? "Yes" : "No") << "\n";

        const auto& info = manager.getDeviceInfo();
        std::cout << "Using device: " << info.name << "\n";
        std::cout << "Vendor: " << info.vendor << "\n";
        std::cout << "Compute units: " << info.max_compute_units << "\n";
    } else {
        std::cout << "Failed to initialize compute manager (GPU may not be "
                     "available)\n";

        // Try CPU fallback
        std::cout << "Trying CPU fallback...\n";
        initialized = manager.initialize(DeviceType::CPU);
        if (initialized) {
            std::cout << "Compute manager initialized with CPU\n";
        } else {
            std::cout << "No OpenCL devices available\n";
        }
    }
}

#endif  // ATOM_OPENCL_AVAILABLE

int main() {
    std::cout << "========================================\n";
    std::cout << "   OpenCL Utilities Example\n";
    std::cout << "========================================\n";

#if ATOM_OPENCL_AVAILABLE
    try {
        demonstratePlatformEnumeration();
        demonstrateContextCreation();
        demonstrateBufferOperations();
        demonstrateKernelExecution();
        demonstrateComputeManager();

        std::cout << "\n========================================\n";
        std::cout << "   All examples completed successfully!\n";
        std::cout << "========================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cout << "\nOpenCL support is not enabled in this build.\n";
    std::cout << "To enable OpenCL, define ATOM_USE_OPENCL and link against "
                 "OpenCL library.\n";

    // Still demonstrate the stub ComputeManager
    auto& manager = ComputeManager::getInstance();
    std::cout << "\nComputeManager stub:\n";
    std::cout << "  isAvailable(): " << (manager.isAvailable() ? "Yes" : "No")
              << "\n";
    std::cout << "  initialize(): " << (manager.initialize() ? "Yes" : "No")
              << "\n";
#endif

    return 0;
}
