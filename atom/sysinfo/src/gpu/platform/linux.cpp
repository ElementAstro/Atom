/**
 * @file linux.cpp
 * @brief Linux-specific GPU functionality implementation
 *
 * This file contains Linux-specific implementations for GPU information
 * retrieval and monitoring using sysfs, DRM, and vendor-specific APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifdef __linux__

#include "linux.hpp"
#include "common.hpp"
#include <spdlog/spdlog.h>

#include <fstream>
#include <filesystem>
#include <regex>
#include <X11/Xlib.h>
#if __has_include(<X11/extensions/Xrandr.h>)
#include <X11/extensions/Xrandr.h>
#endif

namespace atom::system {

namespace {
    // Helper function to read file content
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        
        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            if (!content.empty()) content += "\n";
            content += line;
        }
        return content;
    }

    // Helper function to check if file exists
    bool fileExists(const std::string& path) {
        return std::filesystem::exists(path);
    }

    // Helper function to list directory contents
    std::vector<std::string> listDirectory(const std::string& path) {
        std::vector<std::string> entries;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                entries.push_back(entry.path().filename().string());
            }
        } catch (const std::exception& e) {
            spdlog::debug("Failed to list directory {}: {}", path, e.what());
        }
        return entries;
    }
}

auto getGPUInfoLinux() -> std::string {
    spdlog::info("Starting Linux GPU information retrieval");
    std::string gpuInfo;

    // Try NVIDIA first
    std::ifstream nvidiaFile("/proc/driver/nvidia/gpus/0/information");
    if (nvidiaFile.is_open()) {
        std::string line;
        while (std::getline(nvidiaFile, line)) {
            if (!gpuInfo.empty()) {
                gpuInfo += "\n";
            }
            gpuInfo += line;
        }
        spdlog::debug("Retrieved GPU info from NVIDIA driver");
    } else {
        // Try DRM information
        auto drmCards = listDirectory("/sys/class/drm");
        for (const auto& card : drmCards) {
            if (card.find("card") == 0 && card.find("-") == std::string::npos) {
                std::string cardPath = "/sys/class/drm/" + card;
                std::string devicePath = cardPath + "/device";
                
                if (fileExists(devicePath + "/vendor") && fileExists(devicePath + "/device")) {
                    std::string vendor = readFile(devicePath + "/vendor");
                    std::string device = readFile(devicePath + "/device");
                    
                    if (!gpuInfo.empty()) gpuInfo += "\n";
                    gpuInfo += "DRM Card: " + card + " (Vendor: " + vendor + ", Device: " + device + ")";
                }
            }
        }
        
        if (gpuInfo.empty()) {
            spdlog::warn("Failed to open NVIDIA GPU information file and no DRM cards found");
            gpuInfo = "GPU information not available";
        }
    }

    spdlog::info("Linux GPU information retrieval completed");
    return gpuInfo;
}

auto getDetailedGPUInfoLinux() -> std::vector<GPUInfo> {
    std::vector<GPUInfo> gpus;
    
    // Scan DRM cards
    auto drmCards = listDirectory("/sys/class/drm");
    int gpuIndex = 0;
    
    for (const auto& card : drmCards) {
        if (card.find("card") == 0 && card.find("-") == std::string::npos) {
            std::string cardPath = "/sys/class/drm/" + card;
            std::string devicePath = cardPath + "/device";
            
            GPUInfo gpu;
            gpu.gpuIndex = gpuIndex++;
            gpu.timestamp = std::chrono::steady_clock::now();
            
            // Read vendor ID
            std::string vendorStr = readFile(devicePath + "/vendor");
            if (!vendorStr.empty()) {
                gpu.vendorId = vendorStr;
                gpu.vendor = parseGPUVendor(vendorStr);
            }
            
            // Read device ID
            std::string deviceStr = readFile(devicePath + "/device");
            if (!deviceStr.empty()) {
                gpu.deviceId = deviceStr;
            }
            
            // Read subsystem vendor and device
            std::string subsystemVendor = readFile(devicePath + "/subsystem_vendor");
            std::string subsystemDevice = readFile(devicePath + "/subsystem_device");
            if (!subsystemVendor.empty() && !subsystemDevice.empty()) {
                gpu.subsystemId = subsystemVendor + ":" + subsystemDevice;
            }
            
            // Try to get GPU name from modalias or other sources
            std::string modalias = readFile(devicePath + "/modalias");
            if (!modalias.empty()) {
                // Parse modalias to get more information
                gpu.name = "GPU " + card;
            } else {
                gpu.name = "Unknown GPU " + card;
            }
            
            // Read PCI information
            std::string pciPath = readFile(devicePath + "/uevent");
            if (pciPath.find("PCI_SLOT_NAME=") != std::string::npos) {
                size_t start = pciPath.find("PCI_SLOT_NAME=") + 14;
                size_t end = pciPath.find('\n', start);
                if (end != std::string::npos) {
                    gpu.busId = pciPath.substr(start, end - start);
                }
            }
            
            // Determine GPU type (simplified)
            if (gpu.vendor == GPUVendor::INTEL) {
                gpu.type = GPUType::INTEGRATED;
            } else {
                gpu.type = GPUType::DISCRETE;
            }
            
            // Parse architecture
            gpu.architecture = parseGPUArchitecture(gpu.name, gpu.vendor);
            
            // Try to get memory information
            std::string memInfoPath = devicePath + "/mem_info_vram_total";
            if (fileExists(memInfoPath)) {
                std::string memStr = readFile(memInfoPath);
                if (!memStr.empty()) {
                    try {
                        gpu.memoryInfo.totalMemory = std::stoull(memStr);
                    } catch (const std::exception& e) {
                        spdlog::debug("Failed to parse memory info: {}", e.what());
                    }
                }
            }
            
            // Set timestamps
            gpu.memoryInfo.timestamp = gpu.timestamp;
            gpu.performance.timestamp = gpu.timestamp;
            gpu.compute.timestamp = gpu.timestamp;
            gpu.driver.timestamp = gpu.timestamp;
            
            gpus.push_back(gpu);
        }
    }
    
    return gpus;
}

auto getGPUCountLinux() -> int {
    auto gpus = getDetailedGPUInfoLinux();
    return static_cast<int>(gpus.size());
}

auto getGPUInfoLinux(int gpuIndex) -> GPUInfo {
    auto gpus = getDetailedGPUInfoLinux();
    if (gpuIndex >= 0 && gpuIndex < static_cast<int>(gpus.size())) {
        return gpus[gpuIndex];
    }
    return GPUInfo{};
}

auto getGPUPerformanceMetricsLinux(int gpuIndex) -> GPUPerformanceMetrics {
    GPUPerformanceMetrics metrics;
    metrics.timestamp = std::chrono::steady_clock::now();
    
    // Try to read performance metrics from sysfs
    std::string cardPath = "/sys/class/drm/card" + std::to_string(gpuIndex);
    std::string devicePath = cardPath + "/device";
    
    // Try to read GPU utilization
    std::string gpuBusyPath = devicePath + "/gpu_busy_percent";
    if (fileExists(gpuBusyPath)) {
        std::string busyStr = readFile(gpuBusyPath);
        if (!busyStr.empty()) {
            try {
                metrics.gpuUtilization = std::stod(busyStr);
            } catch (const std::exception& e) {
                spdlog::debug("Failed to parse GPU utilization: {}", e.what());
            }
        }
    }
    
    // Try to read temperature
    std::string tempPath = devicePath + "/hwmon/hwmon0/temp1_input";
    if (fileExists(tempPath)) {
        std::string tempStr = readFile(tempPath);
        if (!tempStr.empty()) {
            try {
                // Temperature is usually in millidegrees Celsius
                metrics.temperature = std::stod(tempStr) / 1000.0;
            } catch (const std::exception& e) {
                spdlog::debug("Failed to parse temperature: {}", e.what());
            }
        }
    }
    
    // Try to read power information
    std::string powerPath = devicePath + "/hwmon/hwmon0/power1_average";
    if (fileExists(powerPath)) {
        std::string powerStr = readFile(powerPath);
        if (!powerStr.empty()) {
            try {
                // Power is usually in microwatts
                metrics.powerDraw = std::stod(powerStr) / 1000000.0;
            } catch (const std::exception& e) {
                spdlog::debug("Failed to parse power draw: {}", e.what());
            }
        }
    }
    
    return metrics;
}

auto getGPUMemoryInfoLinux(int gpuIndex) -> GPUMemoryInfo {
    auto gpu = getGPUInfoLinux(gpuIndex);
    return gpu.memoryInfo;
}

auto getGPUTemperatureLinux(int gpuIndex) -> double {
    auto metrics = getGPUPerformanceMetricsLinux(gpuIndex);
    return metrics.temperature;
}

auto getGPUUtilizationLinux(int gpuIndex) -> double {
    auto metrics = getGPUPerformanceMetricsLinux(gpuIndex);
    return metrics.gpuUtilization;
}

auto getAllMonitorsInfoLinux() -> std::vector<MonitorInfo> {
    spdlog::info("Starting Linux monitor information retrieval");
    std::vector<MonitorInfo> monitors;

#if __has_include(<X11/extensions/Xrandr.h>)
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        spdlog::error("Unable to open X display");
        return monitors;
    }

    Window root = DefaultRootWindow(display);
    XRRScreenResources* screenRes = XRRGetScreenResources(display, root);
    if (!screenRes) {
        XCloseDisplay(display);
        spdlog::error("Unable to get X screen resources");
        return monitors;
    }

    for (int i = 0; i < screenRes->noutput; ++i) {
        XRROutputInfo* outputInfo = XRRGetOutputInfo(display, screenRes, screenRes->outputs[i]);
        if (!outputInfo || outputInfo->connection == RR_Disconnected) {
            if (outputInfo) XRRFreeOutputInfo(outputInfo);
            continue;
        }

        MonitorInfo info;
        info.model = std::string(outputInfo->name);
        info.identifier = std::string(outputInfo->name);
        info.timestamp = std::chrono::steady_clock::now();

        if (outputInfo->crtc) {
            XRRCrtcInfo* crtcInfo = XRRGetCrtcInfo(display, screenRes, outputInfo->crtc);
            if (crtcInfo) {
                info.width = static_cast<int>(crtcInfo->width);
                info.height = static_cast<int>(crtcInfo->height);

                if (crtcInfo->mode != None) {
                    for (int j = 0; j < screenRes->nmode; ++j) {
                        if (screenRes->modes[j].id == crtcInfo->mode) {
                            const XRRModeInfo& mode = screenRes->modes[j];
                            info.refreshRate = static_cast<int>(
                                static_cast<double>(mode.dotClock) /
                                (static_cast<double>(mode.hTotal) * mode.vTotal));
                            break;
                        }
                    }
                }

                XRRFreeCrtcInfo(crtcInfo);
                spdlog::debug("Found Linux monitor: {} ({}x{} @ {}Hz)",
                              info.model, info.width, info.height, info.refreshRate);
            }
        }

        monitors.emplace_back(std::move(info));
        XRRFreeOutputInfo(outputInfo);
    }

    XRRFreeScreenResources(screenRes);
    XCloseDisplay(display);
    spdlog::info("Linux monitor information retrieval completed, found {} monitors", monitors.size());
#else
    spdlog::error("Xrandr extension not available");
#endif
    return monitors;
}

// Placeholder implementations for additional functions
auto getGPUDriverInfoLinux(int gpuIndex) -> GPUDriverInfo {
    auto gpu = getGPUInfoLinux(gpuIndex);
    return gpu.driver;
}

auto getGPUComputeCapabilityLinux(int gpuIndex) -> GPUComputeCapability {
    auto gpu = getGPUInfoLinux(gpuIndex);
    return gpu.compute;
}

auto benchmarkGPULinux([[maybe_unused]] int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults {
    GPUBenchmarkResults results;
    results.testTime = std::chrono::steady_clock::now();
    results.testDuration = config.duration;
    // TODO: Implement GPU benchmarking
    return results;
}

auto startGPUMonitoringLinux([[maybe_unused]] int gpuIndex, [[maybe_unused]] const GPUMonitoringConfig& config,
                            [[maybe_unused]] std::function<void(const GPUPerformanceMetrics&)> callback) -> bool {
    // TODO: Implement GPU monitoring
    return false;
}

auto stopGPUMonitoringLinux([[maybe_unused]] int gpuIndex) -> bool {
    // TODO: Implement stop monitoring
    return false;
}

// Additional placeholder implementations for other functions would go here...

}  // namespace atom::system

#endif  // __linux__
