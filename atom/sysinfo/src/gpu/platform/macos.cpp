/**
 * @file macos.cpp
 * @brief macOS-specific GPU functionality implementation
 *
 * This file contains macOS-specific implementations for GPU information
 * retrieval and monitoring using IOKit, Metal, and Core Graphics APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef __APPLE__

#include "macos.hpp"
#include "common.hpp"
#include <spdlog/spdlog.h>

#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>

namespace atom::system {

namespace {
    // Helper function to get string property from IOKit service
    std::string getIOKitStringProperty(io_service_t service, CFStringRef property) {
        CFTypeRef prop = IORegistryEntryCreateCFProperty(service, property, kCFAllocatorDefault, 0);
        if (!prop) return "";

        std::string result;
        if (CFGetTypeID(prop) == CFStringGetTypeID()) {
            CFStringRef str = static_cast<CFStringRef>(prop);
            const char* cStr = CFStringGetCStringPtr(str, kCFStringEncodingUTF8);
            if (cStr) {
                result = cStr;
            } else {
                char buffer[256];
                if (CFStringGetCString(str, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                    result = buffer;
                }
            }
        }

        CFRelease(prop);
        return result;
    }

    // Helper function to get number property from IOKit service
    uint64_t getIOKitNumberProperty(io_service_t service, CFStringRef property) {
        CFTypeRef prop = IORegistryEntryCreateCFProperty(service, property, kCFAllocatorDefault, 0);
        if (!prop) return 0;

        uint64_t result = 0;
        if (CFGetTypeID(prop) == CFNumberGetTypeID()) {
            CFNumberRef num = static_cast<CFNumberRef>(prop);
            CFNumberGetValue(num, kCFNumberSInt64Type, &result);
        }

        CFRelease(prop);
        return result;
    }
}

auto getGPUInfoMacOS() -> std::string {
    spdlog::info("Starting macOS GPU information retrieval");

    io_iterator_t iterator;
    CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");

    if (!matchDict) {
        spdlog::error("Failed to create IOPCIDevice matching dictionary");
        return "Failed to get GPU information";
    }

    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iterator);
    if (kr != KERN_SUCCESS) {
        spdlog::error("IOServiceGetMatchingServices failed with error: {}", kr);
        return "Failed to get GPU information";
    }

    io_service_t service;
    std::string gpuNames;
    constexpr size_t bufferSize = 256;
    char buffer[bufferSize];

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFTypeRef classCodeProp = IORegistryEntryCreateCFProperty(
            service, CFSTR("class-code"), kCFAllocatorDefault, 0);

        bool isDisplayController = false;
        if (classCodeProp && CFGetTypeID(classCodeProp) == CFDataGetTypeID()) {
            CFDataRef classCodeData = static_cast<CFDataRef>(classCodeProp);
            if (CFDataGetLength(classCodeData) > 0) {
                const UInt8* bytes = CFDataGetBytePtr(classCodeData);
                if (bytes[0] == 0x03) {
                    isDisplayController = true;
                }
            }
        }

        if (classCodeProp) {
            CFRelease(classCodeProp);
        }

        if (isDisplayController) {
            CFTypeRef modelProp = IORegistryEntryCreateCFProperty(
                service, CFSTR("model"), kCFAllocatorDefault, 0);

            if (modelProp && CFGetTypeID(modelProp) == CFStringGetTypeID()) {
                CFStringRef modelStr = static_cast<CFStringRef>(modelProp);
                const char* cStr = CFStringGetCStringPtr(modelStr, kCFStringEncodingUTF8);

                if (cStr || CFStringGetCString(modelStr, buffer, bufferSize, kCFStringEncodingUTF8)) {
                    if (!gpuNames.empty()) {
                        gpuNames += "\n";
                    }
                    gpuNames += cStr ? cStr : buffer;
                }
            }

            if (modelProp) {
                CFRelease(modelProp);
            }
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    std::string gpuInfo = gpuNames.empty() ? "No GPU found" : gpuNames;
    spdlog::debug("macOS GPU info: {}", gpuInfo);
    spdlog::info("macOS GPU information retrieval completed");
    return gpuInfo;
}

auto getDetailedGPUInfoMacOS() -> std::vector<GPUInfo> {
    std::vector<GPUInfo> gpus;

    io_iterator_t iterator;
    CFMutableDictionaryRef matchDict = IOServiceMatching("IOPCIDevice");
    if (!matchDict) return gpus;

    kern_return_t kr = IOServiceGetMatchingServices(kIOMasterPortDefault, matchDict, &iterator);
    if (kr != KERN_SUCCESS) return gpus;

    io_service_t service;
    int gpuIndex = 0;

    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        CFTypeRef classCodeProp = IORegistryEntryCreateCFProperty(
            service, CFSTR("class-code"), kCFAllocatorDefault, 0);

        bool isDisplayController = false;
        if (classCodeProp && CFGetTypeID(classCodeProp) == CFDataGetTypeID()) {
            CFDataRef classCodeData = static_cast<CFDataRef>(classCodeProp);
            if (CFDataGetLength(classCodeData) > 0) {
                const UInt8* bytes = CFDataGetBytePtr(classCodeData);
                if (bytes[0] == 0x03) {
                    isDisplayController = true;
                }
            }
        }

        if (classCodeProp) {
            CFRelease(classCodeProp);
        }

        if (isDisplayController) {
            GPUInfo gpu;
            gpu.gpuIndex = gpuIndex++;
            gpu.timestamp = std::chrono::steady_clock::now();

            // Get GPU model/name
            gpu.name = getIOKitStringProperty(service, CFSTR("model"));
            if (gpu.name.empty()) {
                gpu.name = getIOKitStringProperty(service, CFSTR("IOName"));
            }

            // Get vendor and device IDs
            uint64_t vendorId = getIOKitNumberProperty(service, CFSTR("vendor-id"));
            uint64_t deviceId = getIOKitNumberProperty(service, CFSTR("device-id"));

            gpu.vendorId = std::to_string(vendorId);
            gpu.deviceId = std::to_string(deviceId);

            // Parse vendor
            gpu.vendor = parseGPUVendor(gpu.name);
            if (gpu.vendor == GPUVendor::UNKNOWN) {
                // Try parsing from vendor ID
                if (vendorId == 0x10de) gpu.vendor = GPUVendor::NVIDIA;
                else if (vendorId == 0x1002) gpu.vendor = GPUVendor::AMD;
                else if (vendorId == 0x8086) gpu.vendor = GPUVendor::INTEL;
                else if (gpu.name.find("Apple") != std::string::npos) gpu.vendor = GPUVendor::APPLE;
            }

            // Parse architecture
            gpu.architecture = parseGPUArchitecture(gpu.name, gpu.vendor);

            // Determine GPU type
            if (gpu.vendor == GPUVendor::INTEL ||
                gpu.name.find("Integrated") != std::string::npos ||
                gpu.vendor == GPUVendor::APPLE) {
                gpu.type = GPUType::INTEGRATED;
            } else {
                gpu.type = GPUType::DISCRETE;
            }

            // Get memory information (if available)
            uint64_t vramSize = getIOKitNumberProperty(service, CFSTR("VRAM,totalsize"));
            if (vramSize > 0) {
                gpu.memoryInfo.totalMemory = vramSize;
            }

            // Get subsystem information
            uint64_t subsystemVendor = getIOKitNumberProperty(service, CFSTR("subsystem-vendor-id"));
            uint64_t subsystemDevice = getIOKitNumberProperty(service, CFSTR("subsystem-id"));
            if (subsystemVendor > 0 && subsystemDevice > 0) {
                gpu.subsystemId = std::to_string(subsystemVendor) + ":" + std::to_string(subsystemDevice);
            }

            // Set timestamps
            gpu.memoryInfo.timestamp = gpu.timestamp;
            gpu.performance.timestamp = gpu.timestamp;
            gpu.compute.timestamp = gpu.timestamp;
            gpu.driver.timestamp = gpu.timestamp;

            gpus.push_back(gpu);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    return gpus;
}

auto getGPUCountMacOS() -> int {
    auto gpus = getDetailedGPUInfoMacOS();
    return static_cast<int>(gpus.size());
}

auto getGPUInfoMacOS(int gpuIndex) -> GPUInfo {
    auto gpus = getDetailedGPUInfoMacOS();
    if (gpuIndex >= 0 && gpuIndex < static_cast<int>(gpus.size())) {
        return gpus[gpuIndex];
    }
    return GPUInfo{};
}

auto getGPUPerformanceMetricsMacOS(int gpuIndex) -> GPUPerformanceMetrics {
    GPUPerformanceMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();

    // TODO: Implement performance metrics retrieval using IOKit or Metal
    // This would require additional macOS-specific APIs

    return metrics;
}

auto getGPUMemoryInfoMacOS(int gpuIndex) -> GPUMemoryInfo {
    auto gpu = getGPUInfoMacOS(gpuIndex);
    return gpu.memoryInfo;
}

auto getGPUTemperatureMacOS(int gpuIndex) -> double {
    // TODO: Implement temperature retrieval using IOKit sensors
    return 0.0;
}

auto getGPUUtilizationMacOS(int gpuIndex) -> double {
    // TODO: Implement utilization retrieval using Activity Monitor APIs
    return 0.0;
}

auto getAllMonitorsInfoMacOS() -> std::vector<MonitorInfo> {
    spdlog::info("Starting macOS monitor information retrieval");
    std::vector<MonitorInfo> monitors;

    uint32_t displayCount;
    CGGetActiveDisplayList(0, nullptr, &displayCount);

    if (displayCount == 0) {
        spdlog::warn("No active displays found");
        return monitors;
    }

    std::vector<CGDirectDisplayID> displays(displayCount);
    CGGetActiveDisplayList(displayCount, displays.data(), &displayCount);

    monitors.reserve(displayCount);
    for (uint32_t i = 0; i < displayCount; ++i) {
        CGDirectDisplayID displayID = displays[i];
        MonitorInfo info;

        info.identifier = std::to_string(displayID);
        info.width = static_cast<int>(CGDisplayPixelsWide(displayID));
        info.height = static_cast<int>(CGDisplayPixelsHigh(displayID));
        info.timestamp = std::chrono::steady_clock::now();

        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
        if (mode) {
            info.refreshRate = static_cast<int>(CGDisplayModeGetRefreshRate(mode));
            CGDisplayModeRelease(mode);
        }

        info.model = "Display";
        monitors.emplace_back(std::move(info));

        spdlog::debug("Found macOS monitor: {} ({}x{} @ {}Hz)", info.identifier,
                      info.width, info.height, info.refreshRate);
    }

    spdlog::info("macOS monitor information retrieval completed, found {} monitors", monitors.size());
    return monitors;
}

// Placeholder implementations for additional functions
auto getGPUDriverInfoMacOS(int gpuIndex) -> GPUDriverInfo {
    auto gpu = getGPUInfoMacOS(gpuIndex);
    return gpu.driver;
}

auto getGPUComputeCapabilityMacOS(int gpuIndex) -> GPUComputeCapability {
    auto gpu = getGPUInfoMacOS(gpuIndex);
    return gpu.compute;
}

auto benchmarkGPUMacOS(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults {
    GPUBenchmarkResults results;
    results.testTime = std::chrono::steady_clock::now();
    results.testDuration = config.duration;
    // TODO: Implement GPU benchmarking using Metal
    return results;
}

auto startGPUMonitoringMacOS(int gpuIndex, const GPUMonitoringConfig& config,
                            std::function<void(const GPUPerformanceMetrics&)> callback) -> bool {
    // TODO: Implement GPU monitoring
    return false;
}

auto stopGPUMonitoringMacOS(int gpuIndex) -> bool {
    // TODO: Implement stop monitoring
    return false;
}

// Additional placeholder implementations for other functions would go here...

}  // namespace atom::system

#endif  // __APPLE__
