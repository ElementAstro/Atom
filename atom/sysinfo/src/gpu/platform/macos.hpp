/**
 * @file macos.hpp
 * @brief macOS-specific GPU functionality
 *
 * This file contains macOS-specific implementations for GPU information
 * retrieval and monitoring using IOKit, Metal, and Core Graphics APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_MACOS_HPP
#define ATOM_SYSTEM_MODULE_GPU_MACOS_HPP

#ifdef __APPLE__

#include "gpu.hpp"
#include "common.hpp"
#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Get GPU information using macOS APIs
 * @return String containing GPU information
 */
auto getGPUInfoMacOS() -> std::string;

/**
 * @brief Get detailed GPU information for all GPUs (macOS)
 * @return Vector of GPUInfo structures
 */
auto getDetailedGPUInfoMacOS() -> std::vector<GPUInfo>;

/**
 * @brief Get GPU count using macOS APIs
 * @return Number of GPUs detected
 */
auto getGPUCountMacOS() -> int;

/**
 * @brief Get specific GPU information by index (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return GPUInfo structure for the specified GPU
 */
auto getGPUInfoMacOS(int gpuIndex) -> GPUInfo;

/**
 * @brief Get GPU performance metrics (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return GPUPerformanceMetrics structure
 */
auto getGPUPerformanceMetricsMacOS(int gpuIndex) -> GPUPerformanceMetrics;

/**
 * @brief Get GPU memory information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return GPUMemoryInfo structure
 */
auto getGPUMemoryInfoMacOS(int gpuIndex) -> GPUMemoryInfo;

/**
 * @brief Get GPU temperature (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Temperature in Celsius
 */
auto getGPUTemperatureMacOS(int gpuIndex) -> double;

/**
 * @brief Get GPU utilization (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Utilization percentage
 */
auto getGPUUtilizationMacOS(int gpuIndex) -> double;

/**
 * @brief Get monitor information using macOS APIs
 * @return Vector of MonitorInfo structures
 */
auto getAllMonitorsInfoMacOS() -> std::vector<MonitorInfo>;

/**
 * @brief Get GPU driver information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return GPUDriverInfo structure
 */
auto getGPUDriverInfoMacOS(int gpuIndex) -> GPUDriverInfo;

/**
 * @brief Get GPU compute capabilities (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return GPUComputeCapability structure
 */
auto getGPUComputeCapabilityMacOS(int gpuIndex) -> GPUComputeCapability;

/**
 * @brief Benchmark GPU performance (macOS)
 * @param gpuIndex GPU index (0-based)
 * @param config Benchmark configuration
 * @return GPUBenchmarkResults structure
 */
auto benchmarkGPUMacOS(int gpuIndex, const GPUBenchmarkConfig& config) -> GPUBenchmarkResults;

/**
 * @brief Start GPU monitoring (macOS)
 * @param gpuIndex GPU index (0-based)
 * @param config Monitoring configuration
 * @param callback Callback function for monitoring updates
 * @return True if monitoring started successfully
 */
auto startGPUMonitoringMacOS(int gpuIndex, const GPUMonitoringConfig& config,
                            std::function<void(const GPUPerformanceMetrics&)> callback) -> bool;

/**
 * @brief Stop GPU monitoring (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return True if monitoring stopped successfully
 */
auto stopGPUMonitoringMacOS(int gpuIndex) -> bool;

/**
 * @brief Get GPU power information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Power draw in watts
 */
auto getGPUPowerDrawMacOS(int gpuIndex) -> double;

/**
 * @brief Get GPU fan speed (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Fan speed percentage
 */
auto getGPUFanSpeedMacOS(int gpuIndex) -> double;

/**
 * @brief Get GPU clock speeds (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Map of clock type to speed in MHz
 */
auto getGPUClockSpeedsMacOS(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Get GPU Metal feature set (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Metal feature set string
 */
auto getGPUMetalFeatureSetMacOS(int gpuIndex) -> std::string;

/**
 * @brief Get GPU Metal performance shaders support (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of supported MPS features
 */
auto getGPUMetalPerformanceShadersMacOS(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU IOKit properties (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Map of IOKit property to value
 */
auto getGPUIOKitPropertiesMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU Core Graphics information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Map of Core Graphics property to value
 */
auto getGPUCoreGraphicsInfoMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Check GPU health status (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Health status string
 */
auto checkGPUHealthMacOS(int gpuIndex) -> std::string;

/**
 * @brief Get GPU error log (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of error messages
 */
auto getGPUErrorLogMacOS(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU thermal state (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Thermal state string
 */
auto getGPUThermalStateMacOS(int gpuIndex) -> std::string;

/**
 * @brief Get GPU power state (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Power state string
 */
auto getGPUPowerStateMacOS(int gpuIndex) -> std::string;

/**
 * @brief Get GPU Apple Silicon specific information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Map of Apple Silicon property to value
 */
auto getGPUAppleSiliconInfoMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU unified memory information (macOS Apple Silicon)
 * @param gpuIndex GPU index (0-based)
 * @return Unified memory information
 */
auto getGPUUnifiedMemoryInfoMacOS(int gpuIndex) -> GPUMemoryInfo;

/**
 * @brief Get GPU Neural Engine information (macOS Apple Silicon)
 * @param gpuIndex GPU index (0-based)
 * @return Neural Engine information map
 */
auto getGPUNeuralEngineInfoMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU Media Engine information (macOS Apple Silicon)
 * @param gpuIndex GPU index (0-based)
 * @return Media Engine information map
 */
auto getGPUMediaEngineInfoMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Stress test GPU (macOS)
 * @param gpuIndex GPU index (0-based)
 * @param duration Test duration in seconds
 * @return Stress test results
 */
auto stressTestGPUMacOS(int gpuIndex, int duration) -> GPUBenchmarkResults;

/**
 * @brief Get GPU video codec support (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of supported codecs
 */
auto getGPUVideoCodecSupportMacOS(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU display information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of connected display information
 */
auto getGPUDisplayInfoMacOS(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU memory bandwidth (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Memory bandwidth in GB/s
 */
auto getGPUMemoryBandwidthMacOS(int gpuIndex) -> double;

/**
 * @brief Get GPU compute units information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Map of compute unit type to count
 */
auto getGPUComputeUnitsMacOS(int gpuIndex) -> std::map<std::string, int>;

/**
 * @brief Get GPU Metal command queue information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Command queue information map
 */
auto getGPUMetalCommandQueueInfoMacOS(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Get GPU Metal buffer information (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Buffer information map
 */
auto getGPUMetalBufferInfoMacOS(int gpuIndex) -> std::map<std::string, size_t>;

/**
 * @brief Get GPU Metal texture support (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Vector of supported texture formats
 */
auto getGPUMetalTextureSupportMacOS(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU Metal compute pipeline support (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Compute pipeline support information
 */
auto getGPUMetalComputePipelineSupportMacOS(int gpuIndex) -> std::map<std::string, bool>;

/**
 * @brief Get GPU Metal render pipeline support (macOS)
 * @param gpuIndex GPU index (0-based)
 * @return Render pipeline support information
 */
auto getGPUMetalRenderPipelineSupportMacOS(int gpuIndex) -> std::map<std::string, bool>;

}  // namespace atom::system

#endif  // __APPLE__

#endif  // ATOM_SYSTEM_MODULE_GPU_MACOS_HPP
