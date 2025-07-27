/**
 * @file gpu.hpp
 * @brief Enhanced GPU information and monitoring functionality
 *
 * This file contains comprehensive definitions for retrieving and monitoring GPU
 * information across different platforms. It provides utilities for querying
 * GPU specifications, performance metrics, temperature monitoring, and multi-GPU support.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_GPU_HPP
#define ATOM_SYSTEM_MODULE_GPU_HPP

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @enum GPUVendor
 * @brief GPU vendor identification
 */
enum class GPUVendor {
    UNKNOWN = 0,
    NVIDIA = 1,
    AMD = 2,
    INTEL = 3,
    APPLE = 4,
    QUALCOMM = 5,
    ARM = 6,
    IMAGINATION = 7
};

/**
 * @enum GPUType
 * @brief GPU type classification
 */
enum class GPUType {
    UNKNOWN = 0,
    DISCRETE = 1,      /**< Dedicated GPU card */
    INTEGRATED = 2,    /**< Integrated GPU */
    VIRTUAL = 3,       /**< Virtual GPU */
    EXTERNAL = 4       /**< External GPU (eGPU) */
};

/**
 * @enum GPUArchitecture
 * @brief GPU architecture identification
 */
enum class GPUArchitecture {
    UNKNOWN = 0,
    // NVIDIA architectures
    TESLA = 1,
    FERMI = 2,
    KEPLER = 3,
    MAXWELL = 4,
    PASCAL = 5,
    VOLTA = 6,
    TURING = 7,
    AMPERE = 8,
    ADA_LOVELACE = 9,
    HOPPER = 10,
    // AMD architectures
    TERASCALE = 20,
    GCN = 21,
    RDNA = 22,
    RDNA2 = 23,
    RDNA3 = 24,
    CDNA = 25,
    // Intel architectures
    GEN7 = 40,
    GEN8 = 41,
    GEN9 = 42,
    GEN11 = 43,
    GEN12 = 44,
    XE = 45,
    ARC = 46
};

/**
 * @struct GPUMemoryInfo
 * @brief Detailed GPU memory information
 */
struct GPUMemoryInfo {
    size_t totalMemory;           /**< Total GPU memory in bytes */
    size_t freeMemory;            /**< Free GPU memory in bytes */
    size_t usedMemory;            /**< Used GPU memory in bytes */
    size_t sharedMemory;          /**< Shared system memory in bytes */
    double memoryUsagePercent;    /**< Memory usage percentage */
    
    // Memory specifications
    std::string memoryType;       /**< Memory type (GDDR6, HBM2, etc.) */
    int memoryBusWidth;           /**< Memory bus width in bits */
    double memoryClockSpeed;      /**< Memory clock speed in MHz */
    double memoryBandwidth;       /**< Memory bandwidth in GB/s */
    
    std::chrono::steady_clock::time_point timestamp; /**< When measured */
} ATOM_ALIGNAS(64);

/**
 * @struct GPUPerformanceMetrics
 * @brief Comprehensive GPU performance metrics
 */
struct GPUPerformanceMetrics {
    // Core performance
    double gpuUtilization;        /**< GPU utilization percentage */
    double memoryUtilization;     /**< Memory controller utilization */
    double encoderUtilization;    /**< Video encoder utilization */
    double decoderUtilization;    /**< Video decoder utilization */
    
    // Clock speeds
    double coreClockSpeed;        /**< Current core clock in MHz */
    double memoryClockSpeed;      /**< Current memory clock in MHz */
    double baseClock;             /**< Base clock speed in MHz */
    double boostClock;            /**< Boost clock speed in MHz */
    
    // Thermal information
    double temperature;           /**< GPU temperature in Celsius */
    double hotspotTemperature;    /**< Hotspot temperature in Celsius */
    double memoryTemperature;     /**< Memory temperature in Celsius */
    double maxTemperature;        /**< Maximum safe temperature */
    
    // Power information
    double powerDraw;             /**< Current power draw in watts */
    double maxPowerLimit;         /**< Maximum power limit in watts */
    double powerLimit;            /**< Current power limit in watts */
    double powerEfficiency;       /**< Performance per watt */
    
    // Fan information
    double fanSpeed;              /**< Fan speed percentage */
    int fanRPM;                   /**< Fan speed in RPM */
    
    std::chrono::steady_clock::time_point timestamp; /**< When measured */
} ATOM_ALIGNAS(64);

/**
 * @struct GPUComputeCapability
 * @brief GPU compute capability information
 */
struct GPUComputeCapability {
    // Compute units
    int computeUnits;             /**< Number of compute units/SMs */
    int shaderCores;              /**< Number of shader cores */
    int rtCores;                  /**< Number of RT cores (if available) */
    int tensorCores;              /**< Number of Tensor cores (if available) */
    
    // Compute capability
    std::string computeCapability; /**< Compute capability version */
    std::vector<std::string> supportedAPIs; /**< Supported graphics APIs */
    std::vector<std::string> supportedComputeAPIs; /**< Supported compute APIs */
    
    // Performance characteristics
    double peakFP32Performance;   /**< Peak FP32 performance in TFLOPS */
    double peakFP16Performance;   /**< Peak FP16 performance in TFLOPS */
    double peakINT8Performance;   /**< Peak INT8 performance in TOPS */
    
    // Memory hierarchy
    size_t l1CacheSize;           /**< L1 cache size per SM */
    size_t l2CacheSize;           /**< L2 cache size */
    size_t sharedMemorySize;      /**< Shared memory size per block */
    
    std::chrono::steady_clock::time_point timestamp; /**< When measured */
} ATOM_ALIGNAS(64);

/**
 * @struct GPUDriverInfo
 * @brief GPU driver information
 */
struct GPUDriverInfo {
    std::string driverVersion;    /**< Driver version string */
    std::string driverDate;       /**< Driver release date */
    std::string driverProvider;   /**< Driver provider/vendor */
    std::string driverPath;       /**< Driver file path */
    
    // API support
    std::string openGLVersion;    /**< OpenGL version */
    std::string vulkanVersion;    /**< Vulkan version */
    std::string directXVersion;   /**< DirectX version */
    std::string openCLVersion;    /**< OpenCL version */
    std::string cudaVersion;      /**< CUDA version (NVIDIA) */
    
    // Driver features
    std::vector<std::string> supportedFeatures; /**< Supported driver features */
    bool isWHQLSigned;            /**< Whether driver is WHQL signed */
    
    std::chrono::steady_clock::time_point timestamp; /**< When retrieved */
} ATOM_ALIGNAS(64);

/**
 * @struct GPUInfo
 * @brief Comprehensive GPU information structure
 */
struct GPUInfo {
    // Basic identification
    std::string name;             /**< GPU name/model */
    std::string deviceId;         /**< Device ID */
    std::string vendorId;         /**< Vendor ID */
    GPUVendor vendor;             /**< GPU vendor */
    GPUType type;                 /**< GPU type */
    GPUArchitecture architecture; /**< GPU architecture */
    
    // Hardware specifications
    std::string biosVersion;      /**< GPU BIOS version */
    std::string subsystemId;      /**< Subsystem ID */
    std::string busId;            /**< PCI bus ID */
    int pcieLanes;                /**< PCIe lanes */
    std::string pcieVersion;      /**< PCIe version */
    
    // Memory and performance
    GPUMemoryInfo memoryInfo;     /**< Memory information */
    GPUPerformanceMetrics performance; /**< Performance metrics */
    GPUComputeCapability compute; /**< Compute capabilities */
    GPUDriverInfo driver;         /**< Driver information */
    
    // Multi-GPU information
    int gpuIndex;                 /**< GPU index in multi-GPU setup */
    bool isPrimary;               /**< Whether this is the primary GPU */
    std::vector<std::string> connectedDisplays; /**< Connected displays */
    
    std::chrono::steady_clock::time_point timestamp; /**< When information was gathered */
} ATOM_ALIGNAS(128);

/**
 * @struct MonitorInfo
 * @brief Enhanced monitor information structure
 */
struct alignas(128) MonitorInfo {
    std::string model;            /**< Monitor model name */
    std::string identifier;       /**< Monitor identifier */
    std::string manufacturer;     /**< Monitor manufacturer */
    std::string serialNumber;     /**< Monitor serial number */
    
    // Display specifications
    int width{0};                 /**< Screen width in pixels */
    int height{0};                /**< Screen height in pixels */
    int refreshRate{0};           /**< Refresh rate in Hz */
    double diagonalSize;          /**< Diagonal size in inches */
    double pixelDensity;          /**< Pixel density in PPI */
    
    // Color and HDR support
    std::string colorSpace;       /**< Color space (sRGB, DCI-P3, etc.) */
    int bitDepth;                 /**< Color bit depth */
    bool hdrSupported;            /**< HDR support */
    std::vector<std::string> hdrFormats; /**< Supported HDR formats */
    
    // Connection information
    std::string connectionType;   /**< Connection type (HDMI, DP, etc.) */
    std::string connectedGPU;     /**< Connected GPU identifier */
    bool isPrimary;               /**< Whether this is the primary monitor */
    
    std::chrono::steady_clock::time_point timestamp; /**< When information was gathered */
};

// Main API functions
/**
 * @brief Get basic GPU information string
 * @return String containing GPU information
 */
[[nodiscard]] auto getGPUInfo() -> std::string;

/**
 * @brief Get information for all connected monitors
 * @return Vector containing all monitor information
 */
[[nodiscard]] auto getAllMonitorsInfo() -> std::vector<MonitorInfo>;

/**
 * @brief Get detailed information for all GPUs
 * @return Vector of GPUInfo structures for all detected GPUs
 */
[[nodiscard]] auto getDetailedGPUInfo() -> std::vector<GPUInfo>;

/**
 * @brief Get the number of GPUs in the system
 * @return Number of GPUs detected
 */
[[nodiscard]] auto getGPUCount() -> int;

/**
 * @brief Get detailed information for a specific GPU
 * @param gpuIndex GPU index (0-based)
 * @return GPUInfo structure for the specified GPU
 */
[[nodiscard]] auto getGPUInfo(int gpuIndex) -> GPUInfo;

/**
 * @brief Get performance metrics for a specific GPU
 * @param gpuIndex GPU index (0-based, defaults to 0)
 * @return GPUPerformanceMetrics structure
 */
[[nodiscard]] auto getGPUPerformanceMetrics(int gpuIndex = 0) -> GPUPerformanceMetrics;

/**
 * @brief Get memory information for a specific GPU
 * @param gpuIndex GPU index (0-based, defaults to 0)
 * @return GPUMemoryInfo structure
 */
[[nodiscard]] auto getGPUMemoryInfo(int gpuIndex = 0) -> GPUMemoryInfo;

/**
 * @brief Get temperature for a specific GPU
 * @param gpuIndex GPU index (0-based, defaults to 0)
 * @return Temperature in Celsius
 */
[[nodiscard]] auto getGPUTemperature(int gpuIndex = 0) -> double;

/**
 * @brief Get utilization for a specific GPU
 * @param gpuIndex GPU index (0-based, defaults to 0)
 * @return Utilization percentage (0-100)
 */
[[nodiscard]] auto getGPUUtilization(int gpuIndex = 0) -> double;

// Enhanced GPU functions

/**
 * @brief Get GPU driver information
 * @param gpuIndex GPU index (0-based)
 * @return GPUDriverInfo structure
 */
[[nodiscard]] auto getGPUDriverInfo(int gpuIndex) -> GPUDriverInfo;

/**
 * @brief Get GPU compute capabilities
 * @param gpuIndex GPU index (0-based)
 * @return GPUComputeCapability structure
 */
[[nodiscard]] auto getGPUComputeCapability(int gpuIndex) -> GPUComputeCapability;

/**
 * @brief Get the primary GPU (usually the most powerful discrete GPU)
 * @return GPUInfo structure for the primary GPU
 */
[[nodiscard]] auto getPrimaryGPU() -> GPUInfo;

/**
 * @brief Get all GPUs from a specific vendor
 * @param vendor GPU vendor to filter by
 * @return Vector of GPUInfo structures for GPUs from the specified vendor
 */
[[nodiscard]] auto getGPUsByVendor(GPUVendor vendor) -> std::vector<GPUInfo>;

/**
 * @brief Get all GPUs of a specific type
 * @param type GPU type to filter by
 * @return Vector of GPUInfo structures for GPUs of the specified type
 */
[[nodiscard]] auto getGPUsByType(GPUType type) -> std::vector<GPUInfo>;

/**
 * @brief Get GPU power consumption
 * @param gpuIndex GPU index (0-based)
 * @return Power consumption in watts
 */
[[nodiscard]] auto getGPUPowerDraw(int gpuIndex) -> double;

/**
 * @brief Get GPU fan speed
 * @param gpuIndex GPU index (0-based)
 * @return Fan speed percentage (0-100)
 */
[[nodiscard]] auto getGPUFanSpeed(int gpuIndex) -> double;

/**
 * @brief Get GPU clock speeds
 * @param gpuIndex GPU index (0-based)
 * @return Map of clock type to speed in MHz
 */
[[nodiscard]] auto getGPUClockSpeeds(int gpuIndex) -> std::map<std::string, double>;

/**
 * @brief Check if GPU supports a specific feature
 * @param gpuIndex GPU index (0-based)
 * @param feature Feature name to check
 * @return True if the feature is supported
 */
[[nodiscard]] auto supportsGPUFeature(int gpuIndex, const std::string& feature) -> bool;

/**
 * @brief Get recommended settings for optimal GPU performance
 * @param gpuIndex GPU index (0-based)
 * @return Map of setting names to recommended values
 */
[[nodiscard]] auto getGPURecommendedSettings(int gpuIndex) -> std::map<std::string, std::string>;

/**
 * @brief Detect if GPU is thermal throttling
 * @param gpuIndex GPU index (0-based)
 * @return True if thermal throttling is detected
 */
[[nodiscard]] auto isGPUThermalThrottling(int gpuIndex) -> bool;

/**
 * @brief Calculate GPU power efficiency
 * @param gpuIndex GPU index (0-based)
 * @return Power efficiency score (performance per watt)
 */
[[nodiscard]] auto getGPUPowerEfficiency(int gpuIndex) -> double;

/**
 * @brief Get GPU health status
 * @param gpuIndex GPU index (0-based)
 * @return Health status string
 */
[[nodiscard]] auto getGPUHealthStatus(int gpuIndex) -> std::string;

/**
 * @brief Get GPU error log
 * @param gpuIndex GPU index (0-based)
 * @return Vector of error messages
 */
[[nodiscard]] auto getGPUErrorLog(int gpuIndex) -> std::vector<std::string>;

/**
 * @brief Get GPU performance score
 * @param gpuIndex GPU index (0-based)
 * @return Performance score (0-100)
 */
[[nodiscard]] auto getGPUPerformanceScore(int gpuIndex) -> double;

/**
 * @brief Get GPU summary information
 * @param gpuIndex GPU index (0-based)
 * @return Formatted summary string
 */
[[nodiscard]] auto getGPUSummary(int gpuIndex) -> std::string;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_GPU_HPP
