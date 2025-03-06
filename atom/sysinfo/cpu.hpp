/*
 * cpu.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - Enhanced CPU

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_CPU_HPP
#define ATOM_SYSTEM_MODULE_CPU_HPP

#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Constants representing different CPU architectures
 */
enum class CpuArchitecture {
    UNKNOWN,
    X86,
    X86_64,
    ARM,
    ARM64,
    POWERPC,
    MIPS,
    RISC_V
};

/**
 * @brief Constants representing different CPU vendors
 */
enum class CpuVendor {
    UNKNOWN,
    INTEL,
    AMD,
    ARM,
    APPLE,
    QUALCOMM,
    IBM,
    MEDIATEK,
    SAMSUNG,
    OTHER
};

/**
 * @brief CPU core information structure containing per-core data
 */
struct CpuCoreInfo {
    int id;                   ///< Core ID
    double currentFrequency;  ///< Current frequency in GHz
    double maxFrequency;      ///< Maximum frequency in GHz
    double minFrequency;      ///< Minimum frequency in GHz
    float temperature;        ///< Temperature in Celsius
    float usage;              ///< Usage percentage (0-100%)
    std::string governor;     ///< CPU frequency governor (Linux)
} ATOM_ALIGNAS(32);

/**
 * @brief A structure to hold the sizes and details of the CPU caches.
 */
struct CacheSizes {
    // Cache sizes in bytes
    size_t l1d;  ///< L1 data cache size
    size_t l1i;  ///< L1 instruction cache size
    size_t l2;   ///< L2 cache size
    size_t l3;   ///< L3 cache size

    // Additional cache details
    size_t l1d_line_size;  ///< L1 data cache line size
    size_t l1i_line_size;  ///< L1 instruction cache line size
    size_t l2_line_size;   ///< L2 cache line size
    size_t l3_line_size;   ///< L3 cache line size

    size_t l1d_associativity;  ///< L1 data cache associativity
    size_t l1i_associativity;  ///< L1 instruction cache associativity
    size_t l2_associativity;   ///< L2 cache associativity
    size_t l3_associativity;   ///< L3 cache associativity
} ATOM_ALIGNAS(32);

/**
 * @brief System load average information
 */
struct LoadAverage {
    double oneMinute;       ///< 1 minute load average
    double fiveMinutes;     ///< 5 minute load average
    double fifteenMinutes;  ///< 15 minute load average
} ATOM_ALIGNAS(16);

/**
 * @brief CPU power information
 */
struct CpuPowerInfo {
    double currentWatts;  ///< Current power consumption in watts
    double maxTDP;        ///< Maximum thermal design power
    double energyImpact;  ///< Energy impact (where supported)
} ATOM_ALIGNAS(16);

/**
 * @brief Comprehensive CPU information structure
 */
struct CpuInfo {
    std::string model;               ///< CPU model name
    std::string identifier;          ///< CPU identifier
    CpuArchitecture architecture;    ///< CPU architecture
    CpuVendor vendor;                ///< CPU vendor
    int numPhysicalPackages;         ///< Number of physical CPU packages
    int numPhysicalCores;            ///< Number of physical CPU cores
    int numLogicalCores;             ///< Number of logical CPU cores
    double baseFrequency;            ///< Base frequency in GHz
    double maxFrequency;             ///< Max turbo frequency in GHz
    std::string socketType;          ///< CPU socket type
    float temperature;               ///< Current temperature
    float usage;                     ///< Current usage percentage
    CacheSizes caches;               ///< Cache sizes
    CpuPowerInfo power;              ///< Power information
    std::vector<std::string> flags;  ///< CPU feature flags
    std::vector<CpuCoreInfo> cores;  ///< Per-core information
    LoadAverage loadAverage;         ///< System load average
    std::string instructionSet;      ///< Instruction set (e.g., x86-64, ARMv8)
    int stepping;                    ///< CPU stepping
    int family;                      ///< CPU family
    int model_id;                    ///< CPU model ID
};

/**
 * @brief CPU feature flag check result
 */
enum class CpuFeatureSupport { UNKNOWN, SUPPORTED, NOT_SUPPORTED };

/**
 * @brief Retrieves the current CPU usage percentage.
 * @return A float representing the current CPU usage as a percentage (0.0 to
 * 100.0).
 */
[[nodiscard]] auto getCurrentCpuUsage() -> float;

/**
 * @brief Retrieves per-core CPU usage percentages.
 * @return A vector of floats representing each core's usage percentage.
 */
[[nodiscard]] auto getPerCoreCpuUsage() -> std::vector<float>;

/**
 * @brief Retrieves the current CPU temperature.
 * @return A float representing the CPU temperature in degrees Celsius.
 */
[[nodiscard]] auto getCurrentCpuTemperature() -> float;

/**
 * @brief Retrieves per-core CPU temperatures.
 * @return A vector of floats representing each core's temperature.
 */
[[nodiscard]] auto getPerCoreCpuTemperature() -> std::vector<float>;

/**
 * @brief Retrieves the CPU model name.
 * @return A string representing the CPU model name.
 */
[[nodiscard]] auto getCPUModel() -> std::string;

/**
 * @brief Retrieves the CPU identifier.
 * @return A string representing the CPU identifier.
 */
[[nodiscard]] auto getProcessorIdentifier() -> std::string;

/**
 * @brief Retrieves the current CPU frequency.
 * @return A double representing the CPU frequency in gigahertz (GHz).
 */
[[nodiscard]] auto getProcessorFrequency() -> double;

/**
 * @brief Retrieves the minimum CPU frequency.
 * @return A double representing the minimum CPU frequency in gigahertz (GHz).
 */
[[nodiscard]] auto getMinProcessorFrequency() -> double;

/**
 * @brief Retrieves the maximum CPU frequency.
 * @return A double representing the maximum CPU frequency in gigahertz (GHz).
 */
[[nodiscard]] auto getMaxProcessorFrequency() -> double;

/**
 * @brief Retrieves per-core CPU frequencies.
 * @return A vector of doubles representing each core's current frequency in
 * GHz.
 */
[[nodiscard]] auto getPerCoreFrequencies() -> std::vector<double>;

/**
 * @brief Retrieves the number of physical CPU packages.
 * @return An integer representing the number of physical CPU packages.
 */
[[nodiscard]] auto getNumberOfPhysicalPackages() -> int;

/**
 * @brief Retrieves the number of physical CPU cores.
 * @return An integer representing the total number of physical CPU cores.
 */
[[nodiscard]] auto getNumberOfPhysicalCores() -> int;

/**
 * @brief Retrieves the number of logical CPUs (cores).
 * @return An integer representing the total number of logical CPUs (cores).
 */
[[nodiscard]] auto getNumberOfLogicalCores() -> int;

/**
 * @brief Retrieves the sizes of the CPU caches (L1, L2, L3).
 * @return A `CacheSizes` structure containing the sizes of the L1, L2, and L3
 * caches.
 */
[[nodiscard]] auto getCacheSizes() -> CacheSizes;

/**
 * @brief Retrieves the CPU load average.
 * @return A `LoadAverage` structure with 1, 5, and 15-minute load averages.
 */
[[nodiscard]] auto getCpuLoadAverage() -> LoadAverage;

/**
 * @brief Retrieves CPU power consumption information.
 * @return A `CpuPowerInfo` structure with power consumption details.
 */
[[nodiscard]] auto getCpuPowerInfo() -> CpuPowerInfo;

/**
 * @brief Retrieves all CPU feature flags.
 * @return A vector of strings representing all CPU feature flags.
 */
[[nodiscard]] auto getCpuFeatureFlags() -> std::vector<std::string>;

/**
 * @brief Checks if a specific CPU feature is supported.
 * @param feature The name of the feature to check.
 * @return A `CpuFeatureSupport` enum indicating if the feature is supported.
 */
[[nodiscard]] auto isCpuFeatureSupported(const std::string& feature)
    -> CpuFeatureSupport;

/**
 * @brief Retrieves the CPU architecture.
 * @return A `CpuArchitecture` enum representing the CPU architecture.
 */
[[nodiscard]] auto getCpuArchitecture() -> CpuArchitecture;

/**
 * @brief Retrieves the CPU vendor.
 * @return A `CpuVendor` enum representing the CPU vendor.
 */
[[nodiscard]] auto getCpuVendor() -> CpuVendor;

/**
 * @brief Retrieves the CPU socket type.
 * @return A string representing the CPU socket type.
 */
[[nodiscard]] auto getCpuSocketType() -> std::string;

/**
 * @brief Retrieves the CPU scaling governor (Linux) or power mode
 * (Windows/macOS).
 * @return A string representing the current CPU scaling governor or power mode.
 */
[[nodiscard]] auto getCpuScalingGovernor() -> std::string;

/**
 * @brief Retrieves per-core CPU scaling governors (Linux only).
 * @return A vector of strings representing each core's scaling governor.
 */
[[nodiscard]] auto getPerCoreScalingGovernors() -> std::vector<std::string>;

/**
 * @brief Retrieves comprehensive CPU information.
 * @return A `CpuInfo` structure containing detailed CPU information.
 */
[[nodiscard]] auto getCpuInfo() -> CpuInfo;

/**
 * @brief Convert CPU architecture enum to string.
 * @param arch The CPU architecture enum.
 * @return A string representation of the CPU architecture.
 */
[[nodiscard]] auto cpuArchitectureToString(CpuArchitecture arch) -> std::string;

/**
 * @brief Convert CPU vendor enum to string.
 * @param vendor The CPU vendor enum.
 * @return A string representation of the CPU vendor.
 */
[[nodiscard]] auto cpuVendorToString(CpuVendor vendor) -> std::string;

/**
 * @brief Refreshes all cached CPU information.
 * Forces a refresh of any cached CPU information.
 */
void refreshCpuInfo();

}  // namespace atom::system

#endif /* ATOM_SYSTEM_MODULE_CPU_HPP */