/**
 * @file linux.hpp
 * @brief Linux-specific GPU functionality
 *
 * This file contains Linux-specific implementations for GPU information
 * retrieval and monitoring using sysfs, DRM, and vendor-specific APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_LINUX_HPP
#define ATOM_SYSTEM_MODULE_GPU_LINUX_HPP

#ifdef __linux__

#include "gpu.hpp"
#include "common.hpp"
#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Get GPU information using Linux APIs
 * @return String containing GPU information
 */
auto getGPUInfoLinux() -> std::string;

/**
 * @brief Get detailed GPU information for all GPUs (Linux)
 * @return Vector of GPUInfo structures
 */
auto getDetailedGPUInfoLinux() -> std::vector<GPUInfo>;

/**
 * @brief Get GPU count using Linux APIs
 * @return Number of GPUs detected
 */
auto getGPUCountLinux() -> int;

/**
 * @brief Get specific GPU information by index (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return GPUInfo structure for the specified GPU
 */
auto getGPUInfoLinux(int gpuIndex) -> GPUInfo;

/**
 * @brief Get GPU performance metrics (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return GPUPerformanceMetrics structure
 */
auto getGPUPerformanceMetricsLinux(int gpuIndex) -> GPUPerformanceMetrics;

/**
 * @brief Get GPU memory information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return GPUMemoryInfo structure
 */
auto getGPUMemoryInfoLinux(int gpuIndex) -> GPUMemoryInfo;

/**
 * @brief Get GPU temperature (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Temperature in Celsius
 */
auto getGPUTemperatureLinux(int gpuIndex) -> double;

/**
 * @brief Get GPU utilization (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Utilization percentage
 */
auto getGPUUtilizationLinux(int gpuIndex) -> double;

/**
 * @brief Get monitor information using Linux APIs
 * @return Vector of MonitorInfo structures
 */
auto getAllMonitorsInfoLinux() -> std::vector<MonitorInfo>;

/**
 * @brief Get GPU driver information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return GPUDriverInfo structure
 */
auto getGPUDriverInfoLinux(int gpuIndex) -> GPUDriverInfo;

/**
 * @brief Get GPU compute capabilities (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return GPUComputeCapability structure
 */
auto getGPUComputeCapabilityLinux(int gpuIndex) -> GPUComputeCapability;

/**
 * @brief Benchmark GPU performance (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param config Benchmark configuration
 * @return GPUBenchmarkResults structure
 */
auto benchmarkGPULinux(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults;

/**
 * @brief Start GPU monitoring (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param config Monitoring configuration
 * @param callback Callback function for monitoring updates
 * @return True if monitoring started successfully
 */
auto startGPUMonitoringLinux(int gpuIndex, const GPUMonitoringConfig& config,
                            std::function<void(const GPUPerformanceMetrics&)> callback) -> bool;

/**
 * @brief Stop GPU monitoring (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return True if monitoring stopped successfully
 */
auto stopGPUMonitoringLinux(int gpuIndex) -> bool;

/**
 * @brief Get GPU power information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Power draw in watts
 */
auto getGPUPowerDrawLinux(int gpuIndex) -> double;

/**
 * @brief Get GPU fan speed (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Fan speed percentage
 */
auto getGPUFanSpeedLinux(int gpuIndex) -> double;

/**
 * @brief Get GPU clock speeds (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of clock type to speed in MHz
 */
auto getGPUClockSpeedsLinux(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Get GPU VRAM usage by process (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of process ID to VRAM usage in bytes
 */
auto getGPUVRAMUsageByProcessLinux(int gpuIndex) -> std::map<int, size_t>;

/**
 * @brief Get GPU performance counters (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of counter name to value
 */
auto getGPUPerformanceCountersLinux(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Set GPU power limit (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param powerLimit Power limit in watts
 * @return True if successful
 */
auto setGPUPowerLimitLinux(int gpuIndex, double powerLimit) -> bool;

/**
 * @brief Set GPU fan curve (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param fanCurve Map of temperature to fan speed percentage
 * @return True if successful
 */
auto setGPUFanCurveLinux(int gpuIndex, const std::map<int, int>& fanCurve) -> bool;

/**
 * @brief Get GPU overclocking capabilities (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of parameter to max offset
 */
auto getGPUOverclockingCapabilitiesLinux(int gpuIndex) -> std::map<std::string, int>;

/**
 * @brief Apply GPU overclock settings (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param settings Map of parameter to offset value
 * @return True if successful
 */
auto applyGPUOverclockLinux(int gpuIndex, const std::map<std::string, int>& settings) -> bool;

/**
 * @brief Reset GPU to default settings (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return True if successful
 */
auto resetGPUSettingsLinux(int gpuIndex) -> bool;

/**
 * @brief Get GPU DRM information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of DRM property to value
 */
auto getGPUDRMInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU sysfs information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of sysfs attribute to value
 */
auto getGPUSysfsInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU PCI information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of PCI property to value
 */
auto getGPUPCIInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Check GPU health status (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Health status string
 */
auto checkGPUHealthLinux(int gpuIndex) -> std::string;

/**
 * @brief Get GPU error log (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of error messages
 */
auto getGPUErrorLogLinux(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU NVIDIA-specific information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of NVIDIA property to value
 */
auto getGPUNVIDIAInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU AMD-specific information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of AMD property to value
 */
auto getGPUAMDInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU Intel-specific information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of Intel property to value
 */
auto getGPUIntelInfoLinux(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Stress test GPU (Linux)
 * @param gpuIndex GPU index (0-based)
 * @param duration Test duration in seconds
 * @return Stress test results
 */
auto stressTestGPULinux(int gpuIndex, int duration) -> GPUBenchmarkResults;

/**
 * @brief Get GPU video codec support (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of supported codecs
 */
auto getGPUVideoCodecSupportLinux(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU display connector information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of connector information
 */
auto getGPUDisplayConnectorsLinux(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU memory bandwidth (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Memory bandwidth in GB/s
 */
auto getGPUMemoryBandwidthLinux(int gpuIndex) -> double;

/**
 * @brief Get GPU compute units information (Linux)
 * @param gpuIndex GPU index (0-based)
 * @return Map of compute unit type to count
 */
auto getGPUComputeUnitsLinux(int gpuIndex) -> std::map<std::string, int>;

}  // namespace atom::system

#endif  // __linux__

#endif  // ATOM_SYSTEM_MODULE_GPU_LINUX_HPP
