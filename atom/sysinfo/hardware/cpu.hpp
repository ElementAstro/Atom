/*
 * cpu.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - Enhanced CPU (compat header)

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_CPU_HPP
#define ATOM_SYSTEM_MODULE_CPU_HPP

#include <string>
#include <vector>
#include "atom/macro.hpp"

namespace atom::system {

// 枚举类型
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

struct CpuCoreInfo {
    int id;
    double currentFrequency;
    double maxFrequency;
    double minFrequency;
    float temperature;
    float usage;
    std::string governor;
} ATOM_ALIGNAS(32);

struct CacheSizes {
    size_t l1d, l1i, l2, l3;
    size_t l1d_line_size, l1i_line_size, l2_line_size, l3_line_size;
    size_t l1d_associativity, l1i_associativity, l2_associativity,
        l3_associativity;
} ATOM_ALIGNAS(32);

struct LoadAverage {
    double oneMinute, fiveMinutes, fifteenMinutes;
} ATOM_ALIGNAS(16);

struct CpuPowerInfo {
    double currentWatts, maxTDP, energyImpact;
} ATOM_ALIGNAS(16);

struct CpuInfo {
    std::string model;
    std::string identifier;
    CpuArchitecture architecture;
    CpuVendor vendor;
    int numPhysicalPackages;
    int numPhysicalCores;
    int numLogicalCores;
    double baseFrequency;
    double maxFrequency;
    std::string socketType;
    float temperature;
    float usage;
    CacheSizes caches;
    CpuPowerInfo power;
    std::vector<std::string> flags;
    std::vector<CpuCoreInfo> cores;
    LoadAverage loadAverage;
    std::string instructionSet;
    int stepping;
    int family;
    int model_id;
};

enum class CpuFeatureSupport { UNKNOWN, SUPPORTED, NOT_SUPPORTED };

[[nodiscard]] auto getCurrentCpuUsage() -> float;
[[nodiscard]] auto getPerCoreCpuUsage() -> std::vector<float>;
[[nodiscard]] auto getCurrentCpuTemperature() -> float;
[[nodiscard]] auto getPerCoreCpuTemperature() -> std::vector<float>;
[[nodiscard]] auto getCPUModel() -> std::string;
[[nodiscard]] auto getProcessorIdentifier() -> std::string;
[[nodiscard]] auto getProcessorFrequency() -> double;
[[nodiscard]] auto getMinProcessorFrequency() -> double;
[[nodiscard]] auto getMaxProcessorFrequency() -> double;
[[nodiscard]] auto getPerCoreFrequencies() -> std::vector<double>;
[[nodiscard]] auto getNumberOfPhysicalPackages() -> int;
[[nodiscard]] auto getNumberOfPhysicalCores() -> int;
[[nodiscard]] auto getNumberOfLogicalCores() -> int;
[[nodiscard]] auto getCacheSizes() -> CacheSizes;
[[nodiscard]] auto getCpuLoadAverage() -> LoadAverage;
[[nodiscard]] auto getCpuPowerInfo() -> CpuPowerInfo;
[[nodiscard]] auto getCpuFeatureFlags() -> std::vector<std::string>;
[[nodiscard]] auto isCpuFeatureSupported(const std::string& feature)
    -> CpuFeatureSupport;
[[nodiscard]] auto getCpuArchitecture() -> CpuArchitecture;
[[nodiscard]] auto getCpuVendor() -> CpuVendor;
[[nodiscard]] auto getCpuSocketType() -> std::string;
[[nodiscard]] auto getCpuScalingGovernor() -> std::string;
[[nodiscard]] auto getPerCoreScalingGovernors() -> std::vector<std::string>;
[[nodiscard]] auto getCpuInfo() -> CpuInfo;
[[nodiscard]] auto cpuArchitectureToString(CpuArchitecture arch) -> std::string;
[[nodiscard]] auto cpuVendorToString(CpuVendor vendor) -> std::string;
void refreshCpuInfo();

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_CPU_HPP