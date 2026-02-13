/**
 * @file gpu.cpp
 * @brief Main GPU module implementation
 *
 * This file contains the main implementation of the GPU module that delegates
 * to platform-specific implementations and provides enhanced GPU functionality.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "gpu.hpp"
#include "common.hpp"

#ifdef _WIN32
#include "platform/windows.hpp"
#elif defined(__linux__)
#include "platform/linux.hpp"
#elif defined(__APPLE__)
#include "platform/macos.hpp"
#endif

#include <spdlog/spdlog.h>
#include <algorithm>
#include <thread>
#include <mutex>

namespace atom::system {

namespace {
    // Global monitoring state
    std::mutex monitoringMutex;
    std::map<int, std::thread> monitoringThreads;
    std::map<int, bool> monitoringActive;
    std::map<int, GPUMonitoringConfig> monitoringConfigs;
    std::map<int, std::function<void(const GPUPerformanceMetrics&)>> monitoringCallbacks;
}

auto getGPUInfo() -> std::string {
    spdlog::info("Getting GPU information");

#ifdef _WIN32
    return getGPUInfoWindows();
#elif defined(__linux__)
    return getGPUInfoLinux();
#elif defined(__APPLE__)
    return getGPUInfoMacOS();
#else
    spdlog::warn("GPU information not supported on this platform");
    return "GPU information not supported on this platform";
#endif
}

auto getAllMonitorsInfo() -> std::vector<MonitorInfo> {
    spdlog::info("Getting monitor information");

#ifdef _WIN32
    return getAllMonitorsInfoWindows();
#elif defined(__linux__)
    return getAllMonitorsInfoLinux();
#elif defined(__APPLE__)
    return getAllMonitorsInfoMacOS();
#else
    spdlog::warn("Monitor information not supported on this platform");
    return {};
#endif
}

auto getDetailedGPUInfo() -> std::vector<GPUInfo> {
    spdlog::info("Getting detailed GPU information");

#ifdef _WIN32
    return getDetailedGPUInfoWindows();
#elif defined(__linux__)
    return getDetailedGPUInfoLinux();
#elif defined(__APPLE__)
    return getDetailedGPUInfoMacOS();
#else
    spdlog::warn("Detailed GPU information not supported on this platform");
    return {};
#endif
}

auto getGPUCount() -> int {
    spdlog::debug("Getting GPU count");

#ifdef _WIN32
    return getGPUCountWindows();
#elif defined(__linux__)
    return getGPUCountLinux();
#elif defined(__APPLE__)
    return getGPUCountMacOS();
#else
    return 0;
#endif
}

auto getGPUInfo(int gpuIndex) -> GPUInfo {
    spdlog::debug("Getting GPU info for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return GPUInfo{};
    }

#ifdef _WIN32
    return getGPUInfoWindows(gpuIndex);
#elif defined(__linux__)
    return getGPUInfoLinux(gpuIndex);
#elif defined(__APPLE__)
    return getGPUInfoMacOS(gpuIndex);
#else
    return GPUInfo{};
#endif
}

auto getGPUPerformanceMetrics(int gpuIndex) -> GPUPerformanceMetrics {
    spdlog::debug("Getting GPU performance metrics for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return GPUPerformanceMetrics{};
    }

#ifdef _WIN32
    return getGPUPerformanceMetricsWindows(gpuIndex);
#elif defined(__linux__)
    return getGPUPerformanceMetricsLinux(gpuIndex);
#elif defined(__APPLE__)
    return getGPUPerformanceMetricsMacOS(gpuIndex);
#else
    return GPUPerformanceMetrics{};
#endif
}

auto getGPUMemoryInfo(int gpuIndex) -> GPUMemoryInfo {
    spdlog::debug("Getting GPU memory info for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return GPUMemoryInfo{};
    }

#ifdef _WIN32
    return getGPUMemoryInfoWindows(gpuIndex);
#elif defined(__linux__)
    return getGPUMemoryInfoLinux(gpuIndex);
#elif defined(__APPLE__)
    return getGPUMemoryInfoMacOS(gpuIndex);
#else
    return GPUMemoryInfo{};
#endif
}

auto getGPUTemperature(int gpuIndex) -> double {
    spdlog::debug("Getting GPU temperature for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return 0.0;
    }

#ifdef _WIN32
    return getGPUTemperatureWindows(gpuIndex);
#elif defined(__linux__)
    return getGPUTemperatureLinux(gpuIndex);
#elif defined(__APPLE__)
    return getGPUTemperatureMacOS(gpuIndex);
#else
    return 0.0;
#endif
}

auto getGPUUtilization(int gpuIndex) -> double {
    spdlog::debug("Getting GPU utilization for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return 0.0;
    }

#ifdef _WIN32
    return getGPUUtilizationWindows(gpuIndex);
#elif defined(__linux__)
    return getGPUUtilizationLinux(gpuIndex);
#elif defined(__APPLE__)
    return getGPUUtilizationMacOS(gpuIndex);
#else
    return 0.0;
#endif
}

// Enhanced GPU functions

auto benchmarkGPU(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults {
    spdlog::info("Starting GPU benchmark for index {}", gpuIndex);

    if (gpuIndex < 0) {
        spdlog::error("Invalid GPU index: {}", gpuIndex);
        return GPUBenchmarkResults{};
    }

#ifdef _WIN32
    return benchmarkGPUWindows(gpuIndex, config);
#elif defined(__linux__)
    return benchmarkGPULinux(gpuIndex, config);
#elif defined(__APPLE__)
    return benchmarkGPUMacOS(gpuIndex, config);
#else
    spdlog::warn("GPU benchmarking not supported on this platform");
    return GPUBenchmarkResults{};
#endif
}

auto startGPUMonitoring(int gpuIndex, const GPUMonitoringConfig& config,
                       std::function<void(const GPUPerformanceMetrics&)> callback) -> bool {
    std::lock_guard<std::mutex> lock(monitoringMutex);

    if (monitoringActive[gpuIndex]) {
        spdlog::warn("GPU monitoring already active for index {}", gpuIndex);
        return false;
    }

    spdlog::info("Starting GPU monitoring for index {}", gpuIndex);

    monitoringActive[gpuIndex] = true;
    monitoringConfigs[gpuIndex] = config;
    monitoringCallbacks[gpuIndex] = callback;

    monitoringThreads[gpuIndex] = std::thread([gpuIndex, config, callback]() {
        while (monitoringActive[gpuIndex]) {
            auto metrics = getGPUPerformanceMetrics(gpuIndex);

            // Check for alerts
            if (config.monitorTemperature && metrics.temperature > config.temperatureThreshold) {
                spdlog::warn("GPU {} temperature alert: {}°C > {}°C",
                           gpuIndex, metrics.temperature, config.temperatureThreshold);
            }

            if (config.monitorMemory) {
                auto memInfo = getGPUMemoryInfo(gpuIndex);
                if (memInfo.memoryUsagePercent > config.memoryThreshold) {
                    spdlog::warn("GPU {} memory usage alert: {}% > {}%",
                               gpuIndex, memInfo.memoryUsagePercent, config.memoryThreshold);
                }
            }

            if (config.monitorPower && metrics.powerDraw > 0 &&
                metrics.maxPowerLimit > 0 &&
                (metrics.powerDraw / metrics.maxPowerLimit * 100.0) > config.powerThreshold) {
                spdlog::warn("GPU {} power usage alert: {}W ({}%)",
                           gpuIndex, metrics.powerDraw,
                           metrics.powerDraw / metrics.maxPowerLimit * 100.0);
            }

            callback(metrics);
            std::this_thread::sleep_for(config.updateInterval);
        }
    });

    return true;
}

auto stopGPUMonitoring(int gpuIndex) -> bool {
    std::lock_guard<std::mutex> lock(monitoringMutex);

    if (!monitoringActive[gpuIndex]) {
        spdlog::warn("GPU monitoring not active for index {}", gpuIndex);
        return false;
    }

    spdlog::info("Stopping GPU monitoring for index {}", gpuIndex);

    monitoringActive[gpuIndex] = false;

    if (monitoringThreads[gpuIndex].joinable()) {
        monitoringThreads[gpuIndex].join();
    }

    monitoringThreads.erase(gpuIndex);
    monitoringConfigs.erase(gpuIndex);
    monitoringCallbacks.erase(gpuIndex);

    return true;
}

auto getGPUDriverInfo(int gpuIndex) -> GPUDriverInfo {
    auto gpu = getGPUInfo(gpuIndex);
    return gpu.driver;
}

auto getGPUComputeCapability(int gpuIndex) -> GPUComputeCapability {
    auto gpu = getGPUInfo(gpuIndex);
    return gpu.compute;
}

auto getPrimaryGPU() -> GPUInfo {
    auto gpus = getDetailedGPUInfo();
    if (gpus.empty()) return GPUInfo{};

    // Find primary GPU (first discrete GPU or first GPU if no discrete found)
    auto discreteGPU = std::find_if(gpus.begin(), gpus.end(),
                                   [](const GPUInfo& gpu) { return gpu.type == GPUType::DISCRETE; });

    if (discreteGPU != gpus.end()) {
        return *discreteGPU;
    }

    return gpus[0];
}

auto getGPUsByVendor(GPUVendor vendor) -> std::vector<GPUInfo> {
    auto allGPUs = getDetailedGPUInfo();
    std::vector<GPUInfo> filteredGPUs;

    std::copy_if(allGPUs.begin(), allGPUs.end(), std::back_inserter(filteredGPUs),
                [vendor](const GPUInfo& gpu) { return gpu.vendor == vendor; });

    return filteredGPUs;
}

auto getGPUsByType(GPUType type) -> std::vector<GPUInfo> {
    auto allGPUs = getDetailedGPUInfo();
    std::vector<GPUInfo> filteredGPUs;

    std::copy_if(allGPUs.begin(), allGPUs.end(), std::back_inserter(filteredGPUs),
                [type](const GPUInfo& gpu) { return gpu.type == type; });

    return filteredGPUs;
}

auto getGPUPowerDraw(int gpuIndex) -> double {
    auto metrics = getGPUPerformanceMetrics(gpuIndex);
    return metrics.powerDraw;
}

auto getGPUFanSpeed(int gpuIndex) -> double {
    auto metrics = getGPUPerformanceMetrics(gpuIndex);
    return metrics.fanSpeed;
}

auto getGPUClockSpeeds(int gpuIndex) -> std::map<std::string, double> {
    auto metrics = getGPUPerformanceMetrics(gpuIndex);
    std::map<std::string, double> clocks;

    clocks["core"] = metrics.coreClockSpeed;
    clocks["memory"] = metrics.memoryClockSpeed;
    clocks["base"] = metrics.baseClock;
    clocks["boost"] = metrics.boostClock;

    return clocks;
}

auto supportsGPUFeature(int gpuIndex, const std::string& feature) -> bool {
    auto gpu = getGPUInfo(gpuIndex);
    return supportsFeature(gpu, feature);
}

auto getGPURecommendedSettings(int gpuIndex) -> std::map<std::string, std::string> {
    auto gpu = getGPUInfo(gpuIndex);
    return getRecommendedSettings(gpu);
}

auto isGPUThermalThrottling(int gpuIndex) -> bool {
    auto metrics = getGPUPerformanceMetrics(gpuIndex);
    return detectThermalThrottling(metrics);
}

auto getGPUPowerEfficiency(int gpuIndex) -> double {
    auto metrics = getGPUPerformanceMetrics(gpuIndex);
    return calculatePowerEfficiency(metrics);
}

auto getGPUHealthStatus(int gpuIndex) -> std::string {
    auto gpu = getGPUInfo(gpuIndex);
    auto metrics = getGPUPerformanceMetrics(gpuIndex);

    std::vector<std::string> issues;

    // Check temperature
    if (metrics.temperature > 85.0) {
        issues.push_back("High temperature");
    }

    // Check memory usage
    if (gpu.memoryInfo.memoryUsagePercent > 95.0) {
        issues.push_back("High memory usage");
    }

    // Check power draw
    if (metrics.powerDraw > metrics.maxPowerLimit * 0.95) {
        issues.push_back("High power consumption");
    }

    // Check thermal throttling
    if (detectThermalThrottling(metrics)) {
        issues.push_back("Thermal throttling detected");
    }

    if (issues.empty()) {
        return "Healthy";
    } else {
        std::string status = "Issues detected: ";
        for (size_t i = 0; i < issues.size(); ++i) {
            if (i > 0) status += ", ";
            status += issues[i];
        }
        return status;
    }
}

auto getGPUErrorLog([[maybe_unused]] int gpuIndex) -> std::vector<std::string> {
    // TODO: Implement platform-specific error log retrieval
    return {};
}

auto getGPUPerformanceScore(int gpuIndex) -> double {
    auto gpu = getGPUInfo(gpuIndex);
    return calculateGPUScore(gpu);
}

auto getGPUSummary(int gpuIndex) -> std::string {
    auto gpu = getGPUInfo(gpuIndex);
    return getGPUSummary(gpu);
}

}  // namespace atom::system
