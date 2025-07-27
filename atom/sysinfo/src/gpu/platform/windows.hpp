/**
 * @file windows.hpp
 * @brief Windows-specific GPU functionality
 *
 * This file contains Windows-specific implementations for GPU information
 * retrieval and monitoring using Windows APIs, WMI, and DirectX.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_WINDOWS_HPP
#define ATOM_SYSTEM_MODULE_GPU_WINDOWS_HPP

#ifdef _WIN32

#include "gpu.hpp"
#include "common.hpp"
#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Get GPU information using Windows APIs
 * @return String containing GPU information
 */
auto getGPUInfoWindows() -> std::string;

/**
 * @brief Get detailed GPU information for all GPUs (Windows)
 * @return Vector of GPUInfo structures
 */
auto getDetailedGPUInfoWindows() -> std::vector<GPUInfo>;

/**
 * @brief Get GPU count using Windows APIs
 * @return Number of GPUs detected
 */
auto getGPUCountWindows() -> int;

/**
 * @brief Get specific GPU information by index (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return GPUInfo structure for the specified GPU
 */
auto getGPUInfoWindows(int gpuIndex) -> GPUInfo;

/**
 * @brief Get GPU performance metrics (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return GPUPerformanceMetrics structure
 */
auto getGPUPerformanceMetricsWindows(int gpuIndex) -> GPUPerformanceMetrics;

/**
 * @brief Get GPU memory information (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return GPUMemoryInfo structure
 */
auto getGPUMemoryInfoWindows(int gpuIndex) -> GPUMemoryInfo;

/**
 * @brief Get GPU temperature (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Temperature in Celsius
 */
auto getGPUTemperatureWindows(int gpuIndex) -> double;

/**
 * @brief Get GPU utilization (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Utilization percentage
 */
auto getGPUUtilizationWindows(int gpuIndex) -> double;

/**
 * @brief Get monitor information using Windows APIs
 * @return Vector of MonitorInfo structures
 */
auto getAllMonitorsInfoWindows() -> std::vector<MonitorInfo>;

/**
 * @brief Get GPU driver information (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return GPUDriverInfo structure
 */
auto getGPUDriverInfoWindows(int gpuIndex) -> GPUDriverInfo;

/**
 * @brief Get GPU compute capabilities (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return GPUComputeCapability structure
 */
auto getGPUComputeCapabilityWindows(int gpuIndex) -> GPUComputeCapability;

/**
 * @brief Benchmark GPU performance (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param config Benchmark configuration
 * @return GPUBenchmarkResults structure
 */
auto benchmarkGPUWindows(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults;

/**
 * @brief Start GPU monitoring (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param config Monitoring configuration
 * @param callback Callback function for monitoring updates
 * @return True if monitoring started successfully
 */
auto startGPUMonitoringWindows(int gpuIndex, const GPUMonitoringConfig& config,
                              std::function<void(const GPUPerformanceMetrics&)> callback) -> bool;

/**
 * @brief Stop GPU monitoring (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return True if monitoring stopped successfully
 */
auto stopGPUMonitoringWindows(int gpuIndex) -> bool;

/**
 * @brief Get GPU power information (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Power draw in watts
 */
auto getGPUPowerDrawWindows(int gpuIndex) -> double;

/**
 * @brief Get GPU fan speed (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Fan speed percentage
 */
auto getGPUFanSpeedWindows(int gpuIndex) -> double;

/**
 * @brief Get GPU clock speeds (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Map of clock type to speed in MHz
 */
auto getGPUClockSpeedsWindows(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Check if GPU supports DirectX feature (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param feature DirectX feature name
 * @return True if supported
 */
auto supportsDirectXFeatureWindows(int gpuIndex, const std::string& feature) -> bool;

/**
 * @brief Get GPU VRAM usage by process (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Map of process ID to VRAM usage in bytes
 */
auto getGPUVRAMUsageByProcessWindows(int gpuIndex) -> std::map<int, size_t>;

/**
 * @brief Get GPU performance counters (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Map of counter name to value
 */
auto getGPUPerformanceCountersWindows(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Set GPU power limit (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param powerLimit Power limit in watts
 * @return True if successful
 */
auto setGPUPowerLimitWindows(int gpuIndex, double powerLimit) -> bool;

/**
 * @brief Set GPU fan curve (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param fanCurve Map of temperature to fan speed percentage
 * @return True if successful
 */
auto setGPUFanCurveWindows(int gpuIndex, const std::map<int, int>& fanCurve) -> bool;

/**
 * @brief Get GPU overclocking capabilities (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Map of parameter to max offset
 */
auto getGPUOverclockingCapabilitiesWindows(int gpuIndex) -> std::map<std::string, int>;

/**
 * @brief Apply GPU overclock settings (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param settings Map of parameter to offset value
 * @return True if successful
 */
auto applyGPUOverclockWindows(int gpuIndex, const std::map<std::string, int>& settings) -> bool;

/**
 * @brief Reset GPU to default settings (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return True if successful
 */
auto resetGPUSettingsWindows(int gpuIndex) -> bool;

/**
 * @brief Get GPU video engine utilization (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Map of engine name to utilization percentage
 */
auto getGPUVideoEngineUtilizationWindows(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Get GPU display information (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of connected display information
 */
auto getGPUDisplayInfoWindows(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Check GPU health status (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Health status string
 */
auto checkGPUHealthWindows(int gpuIndex) -> std::string;

/**
 * @brief Get GPU error log (Windows)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of error messages
 */
auto getGPUErrorLogWindows(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Stress test GPU (Windows)
 * @param gpuIndex GPU index (0-based)
 * @param duration Test duration in seconds
 * @return Stress test results
 */
auto stressTestGPUWindows(int gpuIndex, int duration) -> GPUBenchmarkResults;

}  // namespace atom::system

#endif  // _WIN32

#endif  // ATOM_SYSTEM_MODULE_GPU_WINDOWS_HPP
