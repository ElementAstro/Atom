/**
 * @file gpu_info_example.cpp
 * @brief Example demonstrating GPU information retrieval
 *
 * This example shows how to use the GPU module to retrieve comprehensive
 * information about GPUs and monitors in the system.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "atom/sysinfo/gpu/gpu.hpp"
#include "atom/sysinfo/gpu/common.hpp"
#include <iostream>
#include <iomanip>

void printGPUBasicInfo() {
    std::cout << "=== Basic GPU Information ===" << std::endl;

    // Get basic GPU info string
    std::string basicInfo = atom::system::getGPUInfo();
    std::cout << "Basic GPU Info:\n" << basicInfo << std::endl << std::endl;

    // Get GPU count
    int gpuCount = atom::system::getGPUCount();
    std::cout << "Number of GPUs detected: " << gpuCount << std::endl << std::endl;
}

void printDetailedGPUInfo() {
    std::cout << "=== Detailed GPU Information ===" << std::endl;

    auto gpus = atom::system::getDetailedGPUInfo();

    for (const auto& gpu : gpus) {
        std::cout << "GPU " << gpu.gpuIndex << ":" << std::endl;
        std::cout << "  Name: " << gpu.name << std::endl;
        std::cout << "  Vendor: " << atom::system::gpuVendorToString(gpu.vendor) << std::endl;
        std::cout << "  Type: " << atom::system::gpuTypeToString(gpu.type) << std::endl;
        std::cout << "  Architecture: " << atom::system::gpuArchitectureToString(gpu.architecture) << std::endl;
        std::cout << "  Device ID: " << gpu.deviceId << std::endl;
        std::cout << "  Vendor ID: " << gpu.vendorId << std::endl;
        std::cout << "  Bus ID: " << gpu.busId << std::endl;

        // Memory information
        std::cout << "  Memory:" << std::endl;
        std::cout << "    Total: " << atom::system::formatMemorySize(gpu.memoryInfo.totalMemory) << std::endl;
        std::cout << "    Type: " << gpu.memoryInfo.memoryType << std::endl;
        std::cout << "    Bus Width: " << gpu.memoryInfo.memoryBusWidth << " bits" << std::endl;

        // Driver information
        std::cout << "  Driver:" << std::endl;
        std::cout << "    Version: " << gpu.driver.driverVersion << std::endl;
        std::cout << "    Date: " << gpu.driver.driverDate << std::endl;
        std::cout << "    OpenGL: " << gpu.driver.openGLVersion << std::endl;
        std::cout << "    Vulkan: " << gpu.driver.vulkanVersion << std::endl;
        std::cout << "    DirectX: " << gpu.driver.directXVersion << std::endl;

        // Compute capabilities
        std::cout << "  Compute:" << std::endl;
        std::cout << "    Compute Units: " << gpu.compute.computeUnits << std::endl;
        std::cout << "    Shader Cores: " << gpu.compute.shaderCores << std::endl;
        std::cout << "    RT Cores: " << gpu.compute.rtCores << std::endl;
        std::cout << "    Tensor Cores: " << gpu.compute.tensorCores << std::endl;
        std::cout << "    FP32 Performance: " << gpu.compute.peakFP32Performance << " TFLOPS" << std::endl;

        std::cout << std::endl;
    }
}

void printPerformanceMetrics() {
    std::cout << "=== GPU Performance Metrics ===" << std::endl;

    int gpuCount = atom::system::getGPUCount();

    for (int i = 0; i < gpuCount; ++i) {
        auto metrics = atom::system::getGPUPerformanceMetrics(i);

        std::cout << "GPU " << i << " Performance:" << std::endl;
        std::cout << "  Utilization: " << std::fixed << std::setprecision(1)
                  << metrics.gpuUtilization << "%" << std::endl;
        std::cout << "  Memory Utilization: " << metrics.memoryUtilization << "%" << std::endl;
        std::cout << "  Temperature: " << atom::system::formatTemperature(metrics.temperature) << std::endl;
        std::cout << "  Power Draw: " << atom::system::formatPower(metrics.powerDraw) << std::endl;
        std::cout << "  Fan Speed: " << metrics.fanSpeed << "%" << std::endl;

        std::cout << "  Clock Speeds:" << std::endl;
        std::cout << "    Core: " << atom::system::formatClockSpeed(metrics.coreClockSpeed) << std::endl;
        std::cout << "    Memory: " << atom::system::formatClockSpeed(metrics.memoryClockSpeed) << std::endl;
        std::cout << "    Base: " << atom::system::formatClockSpeed(metrics.baseClock) << std::endl;
        std::cout << "    Boost: " << atom::system::formatClockSpeed(metrics.boostClock) << std::endl;

        // Check for thermal throttling
        if (atom::system::isGPUThermalThrottling(i)) {
            std::cout << "  ⚠️  Thermal throttling detected!" << std::endl;
        }

        // Calculate power efficiency
        double efficiency = atom::system::getGPUPowerEfficiency(i);
        std::cout << "  Power Efficiency: " << std::fixed << std::setprecision(2)
                  << efficiency << " util%/W" << std::endl;

        std::cout << std::endl;
    }
}

void printMonitorInfo() {
    std::cout << "=== Monitor Information ===" << std::endl;

    auto monitors = atom::system::getAllMonitorsInfo();

    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& monitor = monitors[i];

        std::cout << "Monitor " << i << ":" << std::endl;
        std::cout << "  Model: " << monitor.model << std::endl;
        std::cout << "  Manufacturer: " << monitor.manufacturer << std::endl;
        std::cout << "  Identifier: " << monitor.identifier << std::endl;
        std::cout << "  Resolution: " << monitor.width << "x" << monitor.height << std::endl;
        std::cout << "  Refresh Rate: " << monitor.refreshRate << " Hz" << std::endl;
        std::cout << "  Diagonal Size: " << monitor.diagonalSize << " inches" << std::endl;
        std::cout << "  Pixel Density: " << std::fixed << std::setprecision(1)
                  << monitor.pixelDensity << " PPI" << std::endl;
        std::cout << "  Color Space: " << monitor.colorSpace << std::endl;
        std::cout << "  Bit Depth: " << monitor.bitDepth << " bits" << std::endl;
        std::cout << "  HDR Support: " << (monitor.hdrSupported ? "Yes" : "No") << std::endl;
        std::cout << "  Connection: " << monitor.connectionType << std::endl;
        std::cout << "  Primary: " << (monitor.isPrimary ? "Yes" : "No") << std::endl;

        std::cout << std::endl;
    }
}

void printGPURecommendations() {
    std::cout << "=== GPU Recommendations ===" << std::endl;

    int gpuCount = atom::system::getGPUCount();

    for (int i = 0; i < gpuCount; ++i) {
        auto recommendations = atom::system::getGPURecommendedSettings(i);

        if (!recommendations.empty()) {
            std::cout << "GPU " << i << " Recommendations:" << std::endl;
            for (const auto& [setting, value] : recommendations) {
                std::cout << "  " << setting << ": " << value << std::endl;
            }
            std::cout << std::endl;
        }
    }
}

void printGPUHealthStatus() {
    std::cout << "=== GPU Health Status ===" << std::endl;

    int gpuCount = atom::system::getGPUCount();

    for (int i = 0; i < gpuCount; ++i) {
        std::string health = atom::system::getGPUHealthStatus(i);
        double score = atom::system::getGPUPerformanceScore(i);

        std::cout << "GPU " << i << ":" << std::endl;
        std::cout << "  Health: " << health << std::endl;
        std::cout << "  Performance Score: " << std::fixed << std::setprecision(1)
                  << score << "/100" << std::endl;

        // Get error log
        auto errors = atom::system::getGPUErrorLog(i);
        if (!errors.empty()) {
            std::cout << "  Recent Errors:" << std::endl;
            for (const auto& error : errors) {
                std::cout << "    - " << error << std::endl;
            }
        }

        std::cout << std::endl;
    }
}

int main() {
    try {
        std::cout << "GPU Information Example" << std::endl;
        std::cout << "======================" << std::endl << std::endl;

        printGPUBasicInfo();
        printDetailedGPUInfo();
        printPerformanceMetrics();
        printMonitorInfo();
        printGPURecommendations();
        printGPUHealthStatus();

        // Print summary for primary GPU
        auto primaryGPU = atom::system::getPrimaryGPU();
        if (!primaryGPU.name.empty()) {
            std::cout << "=== Primary GPU Summary ===" << std::endl;
            std::cout << atom::system::getGPUSummary(primaryGPU.gpuIndex) << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
