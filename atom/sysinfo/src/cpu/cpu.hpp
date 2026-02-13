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
#include <cstdint>
#include <chrono>
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

// New enums for enhanced CPU information
enum class CpuPowerState {
    UNKNOWN,
    C0,     // Active
    C1,     // Halt
    C2,     // Stop-Clock
    C3,     // Sleep
    C6,     // Deep Sleep
    C7,     // Deeper Sleep
    C8,     // Deepest Sleep
    C9,     // Enhanced Deep Sleep
    C10     // Package C-state
};

enum class CpuFrequencyGovernor {
    UNKNOWN,
    PERFORMANCE,
    POWERSAVE,
    USERSPACE,
    ONDEMAND,
    CONSERVATIVE,
    SCHEDUTIL,
    INTERACTIVE,
    ADAPTIVE
};

enum class ThermalThrottleState {
    NORMAL,
    LIGHT_THROTTLE,
    MODERATE_THROTTLE,
    HEAVY_THROTTLE,
    CRITICAL_THROTTLE
};

enum class CpuTopologyLevel {
    THREAD,
    CORE,
    MODULE,
    CLUSTER,
    PACKAGE,
    NUMA_NODE,
    SYSTEM
};

// Enhanced structures for CPU topology and management
struct CpuTopologyNode {
    int id;
    CpuTopologyLevel level;
    int parent_id;
    std::vector<int> children;
    std::string name;
    bool is_online;
} ATOM_ALIGNAS(32);

struct ThermalInfo {
    float current_temp;
    float max_temp;
    float critical_temp;
    float throttle_temp;
    ThermalThrottleState throttle_state;
    bool thermal_throttling_active;
    float thermal_margin;
    std::string thermal_zone;
} ATOM_ALIGNAS(32);

struct CpuPerformanceCounters {
    uint64_t instructions_retired;
    uint64_t cycles_elapsed;
    uint64_t cache_misses;
    uint64_t cache_hits;
    uint64_t branch_mispredictions;
    uint64_t context_switches;
    uint64_t interrupts;
    double instructions_per_cycle;
    double cache_hit_ratio;
    double branch_prediction_accuracy;
} ATOM_ALIGNAS(64);

struct CpuPowerManagement {
    CpuPowerState current_state;
    std::vector<CpuPowerState> available_states;
    double state_residency_percent;
    uint64_t state_transitions;
    bool turbo_boost_enabled;
    bool speed_step_enabled;
    double voltage;
    std::string power_profile;
} ATOM_ALIGNAS(32);

struct CpuCoreInfo {
    int id;
    int physical_id;
    int package_id;
    int numa_node;
    double currentFrequency;
    double maxFrequency;
    double minFrequency;
    float temperature;
    float usage;
    std::string governor;
    CpuFrequencyGovernor governor_type;
    ThermalInfo thermal;
    CpuPerformanceCounters performance;
    CpuPowerManagement power_mgmt;
    bool is_online;
    bool hyperthreading_enabled;
} ATOM_ALIGNAS(64);

struct CacheLevel {
    size_t size;
    size_t line_size;
    size_t associativity;
    size_t sets;
    std::string type;  // "Data", "Instruction", "Unified"
    std::string policy; // "Write-back", "Write-through"
    bool is_shared;
    int shared_cores;
} ATOM_ALIGNAS(32);

struct CacheSizes {
    size_t l1d, l1i, l2, l3;
    size_t l1d_line_size, l1i_line_size, l2_line_size, l3_line_size;
    size_t l1d_associativity, l1i_associativity, l2_associativity,
        l3_associativity;

    // Enhanced cache information
    std::vector<CacheLevel> cache_levels;
    size_t total_cache_size;
    bool has_victim_cache;
    bool has_trace_cache;
    size_t prefetch_size;
} ATOM_ALIGNAS(32);

struct CpuVulnerabilityInfo {
    std::string name;
    std::string status;  // "Vulnerable", "Not affected", "Mitigation"
    std::string mitigation;
    bool hardware_mitigation;
    bool software_mitigation;
} ATOM_ALIGNAS(32);

struct CpuMicrocodeInfo {
    std::string version;
    std::string date;
    std::string signature;
    bool update_available;
    std::string latest_version;
} ATOM_ALIGNAS(32);

struct CpuSecurityInfo {
    std::vector<CpuVulnerabilityInfo> vulnerabilities;
    CpuMicrocodeInfo microcode;
    bool secure_boot_enabled;
    bool smep_enabled;  // Supervisor Mode Execution Prevention
    bool smap_enabled;  // Supervisor Mode Access Prevention
    bool cet_enabled;   // Control-flow Enforcement Technology
    bool ibrs_enabled;  // Indirect Branch Restricted Speculation
    bool stibp_enabled; // Single Thread Indirect Branch Predictors
} ATOM_ALIGNAS(64);

struct LoadAverage {
    double oneMinute, fiveMinutes, fifteenMinutes;
    int running_processes;
    int total_processes;
} ATOM_ALIGNAS(16);

struct CpuPowerInfo {
    double currentWatts, maxTDP, energyImpact;
    double baseWatts;
    double peakWatts;
    double averageWatts;
    double voltage;
    double current;
    std::string power_profile;
    bool power_limit_throttling;
    double power_efficiency;  // Performance per watt
    uint64_t energy_consumed; // Total energy consumed in microjoules
} ATOM_ALIGNAS(32);

struct CpuTopology {
    int numa_nodes;
    int packages;
    int cores_per_package;
    int threads_per_core;
    std::vector<CpuTopologyNode> topology_tree;
    std::vector<std::vector<int>> numa_node_cpus;
    std::vector<std::vector<int>> package_cpus;
    std::vector<std::vector<int>> core_siblings;
    bool hyperthreading_enabled;
    bool numa_enabled;
} ATOM_ALIGNAS(64);

struct CpuInfo {
    // Basic CPU information
    std::string model;
    std::string identifier;
    CpuArchitecture architecture;
    CpuVendor vendor;
    int numPhysicalPackages;
    int numPhysicalCores;
    int numLogicalCores;
    double baseFrequency;
    double maxFrequency;
    double minFrequency;
    std::string socketType;
    float temperature;
    float usage;

    // Cache and memory information
    CacheSizes caches;

    // Power management
    CpuPowerInfo power;

    // Feature flags and capabilities
    std::vector<std::string> flags;

    // Per-core information
    std::vector<CpuCoreInfo> cores;

    // System load information
    LoadAverage loadAverage;

    // CPU identification
    std::string instructionSet;
    int stepping;
    int family;
    int model_id;

    // Enhanced information
    CpuTopology topology;
    CpuSecurityInfo security;
    ThermalInfo thermal;
    CpuPerformanceCounters performance;

    // Additional metadata
    std::string manufacturer_date;
    std::string serial_number;
    std::string bios_version;
    std::string firmware_version;
    uint64_t uptime_seconds;
    std::chrono::system_clock::time_point last_updated;
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

// Enhanced CPU information functions
[[nodiscard]] auto getCpuTopology() -> CpuTopology;
[[nodiscard]] auto getCpuSecurityInfo() -> CpuSecurityInfo;
[[nodiscard]] auto getCpuVulnerabilities() -> std::vector<CpuVulnerabilityInfo>;
[[nodiscard]] auto getCpuMicrocodeInfo() -> CpuMicrocodeInfo;
[[nodiscard]] auto getCpuPerformanceCounters() -> CpuPerformanceCounters;
[[nodiscard]] auto getCpuThermalInfo() -> ThermalInfo;
[[nodiscard]] auto getCpuPowerStates() -> std::vector<CpuPowerState>;
[[nodiscard]] auto getCurrentCpuPowerState() -> CpuPowerState;
[[nodiscard]] auto getCpuFrequencyGovernors() -> std::vector<CpuFrequencyGovernor>;
[[nodiscard]] auto getNumaTopology() -> std::vector<std::vector<int>>;
[[nodiscard]] auto getCpuCacheTopology() -> std::vector<CacheLevel>;

// Enhanced per-core functions
[[nodiscard]] auto getPerCoreThermalInfo() -> std::vector<ThermalInfo>;
[[nodiscard]] auto getPerCorePowerStates() -> std::vector<CpuPowerState>;
[[nodiscard]] auto getPerCorePerformanceCounters() -> std::vector<CpuPerformanceCounters>;

// Utility functions for new enums
[[nodiscard]] auto cpuPowerStateToString(CpuPowerState state) -> std::string;
[[nodiscard]] auto cpuFrequencyGovernorToString(CpuFrequencyGovernor governor) -> std::string;
[[nodiscard]] auto thermalThrottleStateToString(ThermalThrottleState state) -> std::string;
[[nodiscard]] auto cpuTopologyLevelToString(CpuTopologyLevel level) -> std::string;

// Advanced CPU monitoring
[[nodiscard]] auto isCpuThrottling() -> bool;
[[nodiscard]] auto getCpuEfficiencyRating() -> double;
[[nodiscard]] auto getCpuBenchmarkScore() -> double;
[[nodiscard]] auto estimateCpuLifespan() -> std::chrono::hours;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_CPU_HPP
