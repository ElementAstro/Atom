/**
 * @file common.hpp
 * @brief Common GPU functionality and utilities
 *
 * This file contains common definitions, utilities, and helper functions
 * shared across different platform implementations of the GPU module.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_COMMON_HPP
#define ATOM_SYSTEM_MODULE_GPU_COMMON_HPP

#include "gpu.hpp"
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>

namespace atom::system {

/**
 * @struct GPUBenchmarkConfig
 * @brief Configuration for GPU benchmarking
 */
struct GPUBenchmarkConfig {
    // Test parameters
    std::chrono::seconds duration{30};     /**< Benchmark duration */
    int iterations{10};                    /**< Number of iterations */
    bool testCompute{true};                /**< Test compute performance */
    bool testMemory{true};                 /**< Test memory bandwidth */
    bool testGraphics{true};               /**< Test graphics performance */

    // Memory test parameters
    size_t memoryTestSize{256 * 1024 * 1024}; /**< Memory test size in bytes */
    std::string memoryPattern{"sequential"};   /**< Memory access pattern */

    // Compute test parameters
    std::string computeWorkload{"matrix_multiply"}; /**< Compute workload type */
    int workgroupSize{256};                /**< Compute workgroup size */

    // Graphics test parameters
    int renderWidth{1920};                 /**< Render target width */
    int renderHeight{1080};                /**< Render target height */
    std::string shaderComplexity{"medium"}; /**< Shader complexity level */

    GPUBenchmarkConfig() = default;
} ATOM_ALIGNAS(32);

/**
 * @struct GPUBenchmarkResults
 * @brief Results from GPU benchmarking
 */
struct GPUBenchmarkResults {
    // Overall scores
    double overallScore{0.0};              /**< Overall benchmark score */
    double computeScore{0.0};              /**< Compute performance score */
    double memoryScore{0.0};               /**< Memory performance score */
    double graphicsScore{0.0};             /**< Graphics performance score */

    // Detailed metrics
    double memoryBandwidth{0.0};           /**< Memory bandwidth in GB/s */
    double computeThroughput{0.0};         /**< Compute throughput in GFLOPS */
    double fillRate{0.0};                  /**< Fill rate in Mpixels/s */
    double triangleRate{0.0};              /**< Triangle rate in Mtriangles/s */

    // Performance characteristics
    double averageFrameTime{0.0};          /**< Average frame time in ms */
    double minFrameTime{0.0};              /**< Minimum frame time in ms */
    double maxFrameTime{0.0};              /**< Maximum frame time in ms */
    double frameTimeVariance{0.0};         /**< Frame time variance */

    // Thermal and power during test
    double peakTemperature{0.0};           /**< Peak temperature during test */
    double averagePowerDraw{0.0};          /**< Average power draw during test */
    double peakPowerDraw{0.0};             /**< Peak power draw during test */

    // Test metadata
    std::chrono::steady_clock::time_point testTime; /**< When test was performed */
    std::chrono::seconds testDuration;     /**< Actual test duration */
    std::string testConfiguration;         /**< Test configuration summary */

    std::vector<double> frameTimeHistory;  /**< Frame time history */
    std::vector<double> temperatureHistory; /**< Temperature history */
    std::vector<double> powerHistory;      /**< Power draw history */
} ATOM_ALIGNAS(64);

/**
 * @struct GPUMonitoringConfig
 * @brief Configuration for GPU monitoring
 */
struct GPUMonitoringConfig {
    std::chrono::milliseconds updateInterval{1000}; /**< Update interval */
    bool monitorPerformance{true};         /**< Monitor performance metrics */
    bool monitorTemperature{true};         /**< Monitor temperature */
    bool monitorPower{true};               /**< Monitor power consumption */
    bool monitorMemory{true};              /**< Monitor memory usage */
    bool monitorFanSpeed{true};            /**< Monitor fan speed */

    // Thresholds for alerts
    double temperatureThreshold{85.0};     /**< Temperature alert threshold */
    double memoryThreshold{90.0};          /**< Memory usage alert threshold */
    double powerThreshold{95.0};           /**< Power usage alert threshold */

    GPUMonitoringConfig() = default;
} ATOM_ALIGNAS(32);

/**
 * @struct GPUAlert
 * @brief GPU monitoring alert information
 */
struct GPUAlert {
    enum class Type {
        TEMPERATURE_HIGH,
        MEMORY_HIGH,
        POWER_HIGH,
        UTILIZATION_HIGH,
        FAN_FAILURE,
        DRIVER_ERROR,
        PERFORMANCE_DEGRADATION
    };

    Type type;                             /**< Alert type */
    std::string message;                   /**< Alert message */
    double value;                          /**< Alert trigger value */
    double threshold;                      /**< Alert threshold */
    int gpuIndex;                          /**< GPU index */
    std::chrono::steady_clock::time_point timestamp; /**< When alert occurred */
} ATOM_ALIGNAS(32);

// Common utility functions

/**
 * @brief Convert GPU vendor enum to string
 * @param vendor GPU vendor enum
 * @return String representation of vendor
 */
auto gpuVendorToString(GPUVendor vendor) -> std::string;

/**
 * @brief Convert GPU type enum to string
 * @param type GPU type enum
 * @return String representation of type
 */
auto gpuTypeToString(GPUType type) -> std::string;

/**
 * @brief Convert GPU architecture enum to string
 * @param arch GPU architecture enum
 * @return String representation of architecture
 */
auto gpuArchitectureToString(GPUArchitecture arch) -> std::string;

/**
 * @brief Parse vendor ID to determine GPU vendor
 * @param vendorId Vendor ID string
 * @return GPU vendor enum
 */
auto parseGPUVendor(const std::string& vendorId) -> GPUVendor;

/**
 * @brief Parse device name to determine GPU architecture
 * @param deviceName GPU device name
 * @param vendor GPU vendor
 * @return GPU architecture enum
 */
auto parseGPUArchitecture(const std::string& deviceName, GPUVendor vendor) -> GPUArchitecture;

/**
 * @brief Format memory size to human-readable string
 * @param bytes Memory size in bytes
 * @return Formatted string (e.g., "8.0 GB")
 */
auto formatMemorySize(size_t bytes) -> std::string;

/**
 * @brief Format clock speed to human-readable string
 * @param mhz Clock speed in MHz
 * @return Formatted string (e.g., "1.5 GHz")
 */
auto formatClockSpeed(double mhz) -> std::string;

/**
 * @brief Format temperature to string with unit
 * @param celsius Temperature in Celsius
 * @return Formatted string (e.g., "65°C")
 */
auto formatTemperature(double celsius) -> std::string;

/**
 * @brief Format power to string with unit
 * @param watts Power in watts
 * @return Formatted string (e.g., "150W")
 */
auto formatPower(double watts) -> std::string;

/**
 * @brief Calculate GPU performance score
 * @param info GPU information
 * @return Performance score (0-100)
 */
auto calculateGPUScore(const GPUInfo& info) -> double;

/**
 * @brief Validate GPU information structure
 * @param info GPU information to validate
 * @return True if valid, false otherwise
 */
auto validateGPUInfo(const GPUInfo& info) -> bool;

/**
 * @brief Get GPU information summary string
 * @param info GPU information
 * @return Formatted summary string
 */
auto getGPUSummary(const GPUInfo& info) -> std::string;

/**
 * @brief Compare two GPU performance metrics
 * @param a First GPU performance metrics
 * @param b Second GPU performance metrics
 * @return Comparison result (-1, 0, 1)
 */
auto compareGPUPerformance(const GPUPerformanceMetrics& a, const GPUPerformanceMetrics& b) -> int;

/**
 * @brief Check if GPU supports specific feature
 * @param info GPU information
 * @param feature Feature name to check
 * @return True if supported, false otherwise
 */
auto supportsFeature(const GPUInfo& info, const std::string& feature) -> bool;

/**
 * @brief Get recommended settings for GPU
 * @param info GPU information
 * @return Map of setting names to recommended values
 */
auto getRecommendedSettings(const GPUInfo& info) -> std::map<std::string, std::string>;

/**
 * @brief Detect GPU thermal throttling
 * @param metrics GPU performance metrics
 * @return True if thermal throttling detected
 */
auto detectThermalThrottling(const GPUPerformanceMetrics& metrics) -> bool;

/**
 * @brief Estimate GPU power efficiency
 * @param metrics GPU performance metrics
 * @return Power efficiency score
 */
auto calculatePowerEfficiency(const GPUPerformanceMetrics& metrics) -> double;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_GPU_COMMON_HPP
