/*
 * common.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Common Implementation

**************************************************/

#include "common.hpp"
#include <spdlog/spdlog.h>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <algorithm>
#include <shared_mutex>


namespace atom::system {

namespace {
// Use shared_mutex for better read concurrency on cache access
std::shared_mutex g_cacheMutex;
std::atomic<std::chrono::steady_clock::time_point> g_lastCacheRefresh{std::chrono::steady_clock::time_point::min()};
constexpr std::chrono::seconds g_cacheValidDuration{5};

std::atomic<bool> g_cacheInitialized{false};
CpuInfo g_cpuInfoCache;

// Vendor lookup table for faster string matching
const std::unordered_map<std::string_view, CpuVendor> g_vendorLookup = {
    {"intel", CpuVendor::INTEL},
    {"amd", CpuVendor::AMD},
    {"arm", CpuVendor::ARM},
    {"apple", CpuVendor::APPLE},
    {"qualcomm", CpuVendor::QUALCOMM},
    {"ibm", CpuVendor::IBM},
    {"mediatek", CpuVendor::MEDIATEK},
    {"samsung", CpuVendor::SAMSUNG},
    {"genuineintel", CpuVendor::INTEL},
    {"authenticamd", CpuVendor::AMD},
    {"centaurhauls", CpuVendor::OTHER},
    {"cyrixinstead", CpuVendor::OTHER},
    {"transmeta", CpuVendor::OTHER},
    {"geode by nsc", CpuVendor::OTHER},
    {"nexgendriven", CpuVendor::OTHER},
    {"riserise", CpuVendor::OTHER},
    {"sis sis sis", CpuVendor::OTHER},
    {"umc umc umc", CpuVendor::OTHER},
    {"via via via", CpuVendor::OTHER},
    {"vortex86 soc", CpuVendor::OTHER}
};

}  // anonymous namespace

/**
 * @brief Converts a string to bytes with improved parsing and error handling
 * @param str String like "8K", "4M", "2G", "1024", "512KB", "2.5GB"
 * @return Size in bytes, 0 on error
 */
size_t stringToBytes(const std::string& str) {
    if (str.empty()) {
        return 0;
    }

    // Remove whitespace and convert to lowercase for easier parsing
    std::string cleanStr;
    cleanStr.reserve(str.size());
    for (char c : str) {
        if (!std::isspace(c)) {
            cleanStr.push_back(std::tolower(c));
        }
    }

    if (cleanStr.empty()) {
        return 0;
    }

    // Find the first non-digit, non-decimal point character
    size_t unitPos = cleanStr.find_first_not_of("0123456789.");
    if (unitPos == std::string::npos) {
        // No unit, just a number
        try {
            return static_cast<size_t>(std::stoull(cleanStr));
        } catch (const std::exception&) {
            return 0;
        }
    }

    // Extract numeric part and unit
    std::string numStr = cleanStr.substr(0, unitPos);
    std::string unitStr = cleanStr.substr(unitPos);

    double value;
    try {
        value = std::stod(numStr);
    } catch (const std::exception&) {
        return 0;
    }

    if (value < 0) {
        return 0;
    }

    // Parse unit with support for both binary (1024) and decimal (1000) multipliers
    size_t multiplier = 1;
    if (unitStr == "k" || unitStr == "kb") {
        multiplier = 1024;
    } else if (unitStr == "m" || unitStr == "mb") {
        multiplier = 1024ULL * 1024;
    } else if (unitStr == "g" || unitStr == "gb") {
        multiplier = 1024ULL * 1024 * 1024;
    } else if (unitStr == "t" || unitStr == "tb") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    } else if (unitStr == "kib") {
        multiplier = 1024;
    } else if (unitStr == "mib") {
        multiplier = 1024ULL * 1024;
    } else if (unitStr == "gib") {
        multiplier = 1024ULL * 1024 * 1024;
    } else if (unitStr == "tib") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    } else if (unitStr == "b" || unitStr == "bytes") {
        multiplier = 1;
    } else {
        // Unknown unit
        return 0;
    }

    // Check for overflow
    if (value > static_cast<double>(SIZE_MAX) / multiplier) {
        return SIZE_MAX;
    }

    return static_cast<size_t>(value * multiplier);
}

/**
 * @brief Get vendor from CPU identifier string with optimized lookup
 * @param vendorId CPU vendor ID string
 * @return CPU vendor enum
 */
CpuVendor getVendorFromString(const std::string& vendorId) {
    if (vendorId.empty()) {
        return CpuVendor::UNKNOWN;
    }

    // Convert to lowercase for case-insensitive comparison
    std::string vendorLower;
    vendorLower.reserve(vendorId.size());
    std::transform(vendorId.begin(), vendorId.end(), std::back_inserter(vendorLower),
                   [](unsigned char c) { return std::tolower(c); });

    // First try exact match in lookup table
    auto it = g_vendorLookup.find(vendorLower);
    if (it != g_vendorLookup.end()) {
        return it->second;
    }

    // If no exact match, try substring matching for partial vendor strings
    for (const auto& [key, vendor] : g_vendorLookup) {
        if (vendorLower.find(key) != std::string::npos) {
            return vendor;
        }
    }

    // If we have a non-empty string but no match, classify as OTHER
    return CpuVendor::OTHER;
}

/**
 * @brief Check if cache needs refresh with lockless atomic operations
 * @return True if cache needs refresh
 */
bool needsCacheRefresh() {
    // Use lockless atomic operations for better performance
    auto now = std::chrono::steady_clock::now();
    auto lastRefresh = g_lastCacheRefresh.load(std::memory_order_acquire);
    auto elapsed = now - lastRefresh;

    return !g_cacheInitialized.load(std::memory_order_acquire) ||
           elapsed > g_cacheValidDuration;
}

auto cpuArchitectureToString(CpuArchitecture arch) -> std::string {
    switch (arch) {
        case CpuArchitecture::X86:
            return "x86";
        case CpuArchitecture::X86_64:
            return "x86_64";
        case CpuArchitecture::ARM:
            return "ARM";
        case CpuArchitecture::ARM64:
            return "ARM64";
        case CpuArchitecture::POWERPC:
            return "PowerPC";
        case CpuArchitecture::MIPS:
            return "MIPS";
        case CpuArchitecture::RISC_V:
            return "RISC-V";
        case CpuArchitecture::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto cpuVendorToString(CpuVendor vendor) -> std::string {
    switch (vendor) {
        case CpuVendor::INTEL:
            return "Intel";
        case CpuVendor::AMD:
            return "AMD";
        case CpuVendor::ARM:
            return "ARM";
        case CpuVendor::APPLE:
            return "Apple";
        case CpuVendor::QUALCOMM:
            return "Qualcomm";
        case CpuVendor::IBM:
            return "IBM";
        case CpuVendor::MEDIATEK:
            return "MediaTek";
        case CpuVendor::SAMSUNG:
            return "Samsung";
        case CpuVendor::OTHER:
            return "Other";
        case CpuVendor::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto cpuPowerStateToString(CpuPowerState state) -> std::string {
    switch (state) {
        case CpuPowerState::C0:
            return "C0 (Active)";
        case CpuPowerState::C1:
            return "C1 (Halt)";
        case CpuPowerState::C2:
            return "C2 (Stop-Clock)";
        case CpuPowerState::C3:
            return "C3 (Sleep)";
        case CpuPowerState::C6:
            return "C6 (Deep Sleep)";
        case CpuPowerState::C7:
            return "C7 (Deeper Sleep)";
        case CpuPowerState::C8:
            return "C8 (Deepest Sleep)";
        case CpuPowerState::C9:
            return "C9 (Enhanced Deep Sleep)";
        case CpuPowerState::C10:
            return "C10 (Package C-state)";
        case CpuPowerState::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto cpuFrequencyGovernorToString(CpuFrequencyGovernor governor) -> std::string {
    switch (governor) {
        case CpuFrequencyGovernor::PERFORMANCE:
            return "performance";
        case CpuFrequencyGovernor::POWERSAVE:
            return "powersave";
        case CpuFrequencyGovernor::USERSPACE:
            return "userspace";
        case CpuFrequencyGovernor::ONDEMAND:
            return "ondemand";
        case CpuFrequencyGovernor::CONSERVATIVE:
            return "conservative";
        case CpuFrequencyGovernor::SCHEDUTIL:
            return "schedutil";
        case CpuFrequencyGovernor::INTERACTIVE:
            return "interactive";
        case CpuFrequencyGovernor::ADAPTIVE:
            return "adaptive";
        case CpuFrequencyGovernor::UNKNOWN:
        default:
            return "unknown";
    }
}

auto thermalThrottleStateToString(ThermalThrottleState state) -> std::string {
    switch (state) {
        case ThermalThrottleState::NORMAL:
            return "Normal";
        case ThermalThrottleState::LIGHT_THROTTLE:
            return "Light Throttle";
        case ThermalThrottleState::MODERATE_THROTTLE:
            return "Moderate Throttle";
        case ThermalThrottleState::HEAVY_THROTTLE:
            return "Heavy Throttle";
        case ThermalThrottleState::CRITICAL_THROTTLE:
            return "Critical Throttle";
        default:
            return "Unknown";
    }
}

auto cpuTopologyLevelToString(CpuTopologyLevel level) -> std::string {
    switch (level) {
        case CpuTopologyLevel::THREAD:
            return "Thread";
        case CpuTopologyLevel::CORE:
            return "Core";
        case CpuTopologyLevel::MODULE:
            return "Module";
        case CpuTopologyLevel::CLUSTER:
            return "Cluster";
        case CpuTopologyLevel::PACKAGE:
            return "Package";
        case CpuTopologyLevel::NUMA_NODE:
            return "NUMA Node";
        case CpuTopologyLevel::SYSTEM:
            return "System";
        default:
            return "Unknown";
    }
}

void refreshCpuInfo() {
    spdlog::info("Manually refreshing CPU info cache");

    // Use atomic operations to invalidate cache
    g_lastCacheRefresh.store(std::chrono::steady_clock::time_point::min(),
                            std::memory_order_release);
    g_cacheInitialized.store(false, std::memory_order_release);

    // Force a refresh by calling getCpuInfo()
    [[maybe_unused]] auto result = getCpuInfo();
    spdlog::info("CPU info cache refreshed");
}

/**
 * @brief Get comprehensive CPU information with optimized caching
 * @return CpuInfo structure with all available CPU details
 */
auto getCpuInfo() -> CpuInfo {
    spdlog::debug("Starting getCpuInfo function");

    // Fast path: check if cache is valid without locking
    if (!needsCacheRefresh()) {
        std::shared_lock<std::shared_mutex> lock(g_cacheMutex);
        if (g_cacheInitialized.load(std::memory_order_acquire)) {
            spdlog::debug("Using cached CPU info");
            return g_cpuInfoCache;
        }
    }

    CpuInfo info;

    info.model = getCPUModel();
    info.identifier = getProcessorIdentifier();
    info.architecture = getCpuArchitecture();
    info.vendor = getCpuVendor();
    info.numPhysicalPackages = getNumberOfPhysicalPackages();
    info.numPhysicalCores = getNumberOfPhysicalCores();
    info.numLogicalCores = getNumberOfLogicalCores();

    info.baseFrequency = getProcessorFrequency();
    info.maxFrequency = getMaxProcessorFrequency();
    info.minFrequency = getMinProcessorFrequency();

    info.socketType = getCpuSocketType();

    info.temperature = getCurrentCpuTemperature();
    info.usage = getCurrentCpuUsage();

    info.caches = getCacheSizes();
    info.power = getCpuPowerInfo();
    info.flags = getCpuFeatureFlags();
    info.loadAverage = getCpuLoadAverage();

    // Initialize enhanced information with default values
    // These will be populated by platform-specific implementations
    info.topology = {};
    info.security = {};
    info.thermal = {};
    info.performance = {};

    // Set metadata
    info.uptime_seconds = 0;  // Will be set by platform-specific code
    info.last_updated = std::chrono::system_clock::now();

    switch (info.architecture) {
        case CpuArchitecture::X86_64:
            info.instructionSet = "x86-64";
            break;
        case CpuArchitecture::X86:
            info.instructionSet = "x86";
            break;
        case CpuArchitecture::ARM64:
            info.instructionSet = "ARMv8-A";
            break;
        case CpuArchitecture::ARM:
            info.instructionSet = "ARMv7";
            break;
        case CpuArchitecture::POWERPC:
            info.instructionSet = "PowerPC";
            break;
        case CpuArchitecture::MIPS:
            info.instructionSet = "MIPS";
            break;
        case CpuArchitecture::RISC_V:
            info.instructionSet = "RISC-V";
            break;
        default:
            info.instructionSet = "Unknown";
            break;
    }

    static const std::regex cpuIdRegex(
        ".*Family (\\d+) Model (\\d+) Stepping (\\d+).*");
    std::smatch match;

    if (std::regex_search(info.identifier, match, cpuIdRegex) &&
        match.size() > 3) {
        try {
            info.family = std::stoi(match[1]);
            info.model_id = std::stoi(match[2]);
            info.stepping = std::stoi(match[3]);
        } catch (const std::exception& e) {
            spdlog::warn("Error parsing CPU family/model/stepping: {}",
                         e.what());
            info.family = 0;
            info.model_id = 0;
            info.stepping = 0;
        }
    } else {
        info.family = 0;
        info.model_id = 0;
        info.stepping = 0;
    }

    const auto coreUsages = getPerCoreCpuUsage();
    const auto coreTemps = getPerCoreCpuTemperature();
    const auto coreFreqs = getPerCoreFrequencies();
    const auto coreGovernors = getPerCoreScalingGovernors();

    const int numCores = info.numLogicalCores;
    info.cores.resize(numCores);

    for (int i = 0; i < numCores; ++i) {
        info.cores[i].id = i;
        info.cores[i].physical_id = i;  // Will be updated by platform-specific code
        info.cores[i].package_id = 0;   // Will be updated by platform-specific code
        info.cores[i].numa_node = 0;    // Will be updated by platform-specific code

        info.cores[i].usage =
            (i < static_cast<int>(coreUsages.size())) ? coreUsages[i] : 0.0f;
        info.cores[i].temperature =
            (i < static_cast<int>(coreTemps.size())) ? coreTemps[i] : 0.0f;
        info.cores[i].currentFrequency =
            (i < static_cast<int>(coreFreqs.size())) ? coreFreqs[i] : 0.0;
        info.cores[i].maxFrequency = info.maxFrequency;
        info.cores[i].minFrequency = info.minFrequency;
        info.cores[i].governor = (i < static_cast<int>(coreGovernors.size()))
                                     ? coreGovernors[i]
                                     : "Unknown";
        info.cores[i].governor_type = CpuFrequencyGovernor::UNKNOWN;

        // Initialize enhanced per-core information with defaults
        info.cores[i].thermal = {};
        info.cores[i].performance = {};
        info.cores[i].power_mgmt = {};
        info.cores[i].is_online = true;
        info.cores[i].hyperthreading_enabled = false;
    }

    // Update cache with exclusive lock
    {
        std::unique_lock<std::shared_mutex> lock(g_cacheMutex);
        g_cpuInfoCache = info;
        g_lastCacheRefresh.store(std::chrono::steady_clock::now(),
                                std::memory_order_release);
        g_cacheInitialized.store(true, std::memory_order_release);
    }

    spdlog::debug("Finished getCpuInfo function");
    return info;
}

/**
 * @brief Check if a specific CPU feature is supported with optimized matching
 * @param feature The CPU feature to check for support
 * @return CpuFeatureSupport indicating the support status
 */
auto isCpuFeatureSupported(const std::string& feature) -> CpuFeatureSupport {
    if (feature.empty()) {
        return CpuFeatureSupport::UNKNOWN;
    }

    spdlog::debug("Checking if CPU feature {} is supported", feature);

    // Convert to lowercase once for all comparisons
    std::string featureLower;
    featureLower.reserve(feature.size());
    std::transform(feature.begin(), feature.end(), std::back_inserter(featureLower),
                   [](unsigned char c) { return std::tolower(c); });

    const auto flags = getCpuFeatureFlags();

    // Create a set for O(1) lookup of exact matches
    static thread_local std::unordered_set<std::string> flagSet;
    static thread_local std::vector<std::string> lastFlags;

    // Update flag set if flags have changed
    if (flags != lastFlags) {
        flagSet.clear();
        for (const auto& flag : flags) {
            std::string flagLower;
            flagLower.reserve(flag.size());
            std::transform(flag.begin(), flag.end(), std::back_inserter(flagLower),
                          [](unsigned char c) { return std::tolower(c); });
            flagSet.insert(flagLower);
        }
        lastFlags = flags;
    }

    // Check for exact match first
    if (flagSet.count(featureLower)) {
        spdlog::debug("Feature {} is directly supported", feature);
        return CpuFeatureSupport::SUPPORTED;
    }

    // Special case handling for common feature aliases
    static const std::unordered_map<std::string, std::vector<std::string>> featureAliases = {
        {"avx512", {"avx512f", "avx512cd", "avx512er", "avx512pf", "avx512bw",
                   "avx512dq", "avx512vl", "avx512ifma", "avx512vbmi"}},
        {"vt", {"vmx", "svm"}},
        {"virtualization", {"vmx", "svm"}},
        {"aes", {"aes", "aesni"}},
        {"sse", {"sse", "sse2", "sse3", "ssse3", "sse4_1", "sse4_2"}},
        {"avx", {"avx", "avx2"}},
        {"fma", {"fma", "fma3", "fma4"}},
        {"sha", {"sha", "sha_ni", "sha1", "sha256"}}
    };

    auto aliasIt = featureAliases.find(featureLower);
    if (aliasIt != featureAliases.end()) {
        for (const auto& alias : aliasIt->second) {
            if (flagSet.count(alias)) {
                spdlog::debug("Feature {} is supported via {}", feature, alias);
                return CpuFeatureSupport::SUPPORTED;
            }
        }
    }

    // Fallback: substring matching for partial feature names
    for (const auto& flag : flagSet) {
        if (flag.find(featureLower) != std::string::npos ||
            featureLower.find(flag) != std::string::npos) {
            spdlog::debug("Feature {} is supported via substring match with {}", feature, flag);
            return CpuFeatureSupport::SUPPORTED;
        }
    }

    spdlog::debug("Feature {} is not supported", feature);
    return CpuFeatureSupport::NOT_SUPPORTED;
}

// Advanced CPU feature implementations

/**
 * @brief Get CPU vulnerabilities information
 * @return Vector of CPU vulnerability information
 */
auto getCpuVulnerabilities() -> std::vector<CpuVulnerabilityInfo> {
    spdlog::debug("getCpuVulnerabilities: Reading CPU vulnerability information");

    try {
        auto security = getCpuSecurityInfo();
        return security.vulnerabilities;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuVulnerabilities: {}", e.what());
        return {};
    }
}

/**
 * @brief Get CPU microcode information
 * @return CPU microcode information structure
 */
auto getCpuMicrocodeInfo() -> CpuMicrocodeInfo {
    spdlog::debug("getCpuMicrocodeInfo: Reading CPU microcode information");

    try {
        auto security = getCpuSecurityInfo();
        return security.microcode;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuMicrocodeInfo: {}", e.what());
        return {};
    }
}

/**
 * @brief Get CPU performance counters
 * @return CPU performance counters structure
 */
auto getCpuPerformanceCounters() -> CpuPerformanceCounters {
    spdlog::debug("getCpuPerformanceCounters: Reading CPU performance counters");

    CpuPerformanceCounters counters{};

    try {
        // Platform-specific implementation would populate these
        // For now, return default values
        counters.instructions_retired = 0;
        counters.cycles_elapsed = 0;
        counters.cache_misses = 0;
        counters.cache_hits = 0;
        counters.branch_mispredictions = 0;
        counters.context_switches = 0;
        counters.interrupts = 0;
        counters.instructions_per_cycle = 0.0;
        counters.cache_hit_ratio = 0.0;
        counters.branch_prediction_accuracy = 0.0;

        spdlog::debug("CPU performance counters retrieved (placeholder values)");

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuPerformanceCounters: {}", e.what());
    }

    return counters;
}

/**
 * @brief Get current CPU power state
 * @return Current CPU power state
 */
auto getCurrentCpuPowerState() -> CpuPowerState {
    spdlog::debug("getCurrentCpuPowerState: Reading current CPU power state");

    try {
        auto states = getCpuPowerStates();
        if (!states.empty()) {
            // Return the first available state as current (simplified)
            return states[0];
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in getCurrentCpuPowerState: {}", e.what());
    }

    return CpuPowerState::UNKNOWN;
}

/**
 * @brief Get available CPU frequency governors
 * @return Vector of available CPU frequency governors
 */
auto getCpuFrequencyGovernors() -> std::vector<CpuFrequencyGovernor> {
    spdlog::debug("getCpuFrequencyGovernors: Reading available CPU frequency governors");

    std::vector<CpuFrequencyGovernor> governors;

    try {
        // Common governors available on most systems
        governors.push_back(CpuFrequencyGovernor::PERFORMANCE);
        governors.push_back(CpuFrequencyGovernor::POWERSAVE);
        governors.push_back(CpuFrequencyGovernor::ONDEMAND);
        governors.push_back(CpuFrequencyGovernor::CONSERVATIVE);

        // Platform-specific implementation would check actual availability
        spdlog::debug("Found {} CPU frequency governors", governors.size());

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuFrequencyGovernors: {}", e.what());
    }

    return governors;
}

/**
 * @brief Get NUMA topology information
 * @return Vector of NUMA node CPU mappings
 */
auto getNumaTopology() -> std::vector<std::vector<int>> {
    spdlog::debug("getNumaTopology: Reading NUMA topology information");

    try {
        auto topology = getCpuTopology();
        return topology.numa_node_cpus;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getNumaTopology: {}", e.what());
        return {};
    }
}

/**
 * @brief Get CPU cache topology
 * @return Vector of cache level information
 */
auto getCpuCacheTopology() -> std::vector<CacheLevel> {
    spdlog::debug("getCpuCacheTopology: Reading CPU cache topology");

    try {
        auto caches = getCacheSizes();
        return caches.cache_levels;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuCacheTopology: {}", e.what());
        return {};
    }
}

/**
 * @brief Get per-core thermal information
 * @return Vector of per-core thermal information
 */
auto getPerCoreThermalInfo() -> std::vector<ThermalInfo> {
    spdlog::debug("getPerCoreThermalInfo: Reading per-core thermal information");

    std::vector<ThermalInfo> thermals;

    try {
        int num_cores = getNumberOfLogicalCores();
        auto core_temps = getPerCoreCpuTemperature();

        thermals.resize(num_cores);
        for (int i = 0; i < num_cores; ++i) {
            thermals[i].current_temp = (i < static_cast<int>(core_temps.size())) ? core_temps[i] : 0.0f;
            thermals[i].thermal_throttling_active = false;
            thermals[i].throttle_state = ThermalThrottleState::NORMAL;
            thermals[i].thermal_zone = "Core " + std::to_string(i);
        }

        spdlog::debug("Retrieved thermal information for {} cores", num_cores);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getPerCoreThermalInfo: {}", e.what());
    }

    return thermals;
}

/**
 * @brief Get per-core power states
 * @return Vector of per-core power states
 */
auto getPerCorePowerStates() -> std::vector<CpuPowerState> {
    spdlog::debug("getPerCorePowerStates: Reading per-core power states");

    std::vector<CpuPowerState> states;

    try {
        int num_cores = getNumberOfLogicalCores();
        states.resize(num_cores, CpuPowerState::C0); // Default to active state

        spdlog::debug("Retrieved power states for {} cores", num_cores);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getPerCorePowerStates: {}", e.what());
    }

    return states;
}

/**
 * @brief Get per-core performance counters
 * @return Vector of per-core performance counters
 */
auto getPerCorePerformanceCounters() -> std::vector<CpuPerformanceCounters> {
    spdlog::debug("getPerCorePerformanceCounters: Reading per-core performance counters");

    std::vector<CpuPerformanceCounters> counters;

    try {
        int num_cores = getNumberOfLogicalCores();
        counters.resize(num_cores);

        // Initialize with default values
        for (auto& counter : counters) {
            counter = {};
        }

        spdlog::debug("Retrieved performance counters for {} cores", num_cores);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getPerCorePerformanceCounters: {}", e.what());
    }

    return counters;
}

/**
 * @brief Get CPU efficiency rating
 * @return CPU efficiency rating (performance per watt)
 */
auto getCpuEfficiencyRating() -> double {
    spdlog::debug("getCpuEfficiencyRating: Calculating CPU efficiency rating");

    try {
        auto power_info = getCpuPowerInfo();
        float cpu_usage = getCurrentCpuUsage();

        if (power_info.currentWatts > 0.0) {
            double efficiency = cpu_usage / power_info.currentWatts;
            spdlog::debug("CPU efficiency rating: {:.2f} performance/watt", efficiency);
            return efficiency;
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuEfficiencyRating: {}", e.what());
    }

    return 0.0;
}

/**
 * @brief Get CPU benchmark score (simplified)
 * @return CPU benchmark score
 */
auto getCpuBenchmarkScore() -> double {
    spdlog::debug("getCpuBenchmarkScore: Calculating CPU benchmark score");

    try {
        auto info = getCpuInfo();

        // Simplified benchmark score based on cores, frequency, and architecture
        double score = info.numLogicalCores * info.maxFrequency;

        // Architecture multipliers
        switch (info.architecture) {
            case CpuArchitecture::X86_64:
                score *= 1.0;
                break;
            case CpuArchitecture::ARM64:
                score *= 0.8; // ARM typically lower per-core performance
                break;
            case CpuArchitecture::ARM:
                score *= 0.6;
                break;
            default:
                score *= 0.5;
                break;
        }

        // Vendor multipliers
        switch (info.vendor) {
            case CpuVendor::INTEL:
            case CpuVendor::AMD:
                score *= 1.0;
                break;
            case CpuVendor::APPLE:
                score *= 1.2; // Apple Silicon efficiency
                break;
            default:
                score *= 0.8;
                break;
        }

        spdlog::debug("CPU benchmark score: {:.0f}", score);
        return score;

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuBenchmarkScore: {}", e.what());
    }

    return 0.0;
}

/**
 * @brief Estimate CPU lifespan based on usage and thermal conditions
 * @return Estimated CPU lifespan in hours
 */
auto estimateCpuLifespan() -> std::chrono::hours {
    spdlog::debug("estimateCpuLifespan: Estimating CPU lifespan");

    try {
        auto thermal = getCpuThermalInfo();
        float cpu_usage = getCurrentCpuUsage();

        // Base lifespan: 10 years (87,600 hours)
        int base_hours = 87600;

        // Temperature factor (higher temperature reduces lifespan)
        double temp_factor = 1.0;
        if (thermal.current_temp > 80.0f) {
            temp_factor = 0.7; // High temperature
        } else if (thermal.current_temp > 70.0f) {
            temp_factor = 0.85; // Moderate temperature
        } else if (thermal.current_temp > 60.0f) {
            temp_factor = 0.95; // Normal temperature
        }

        // Usage factor (higher usage reduces lifespan)
        double usage_factor = 1.0;
        if (cpu_usage > 80.0f) {
            usage_factor = 0.8; // Heavy usage
        } else if (cpu_usage > 50.0f) {
            usage_factor = 0.9; // Moderate usage
        }

        // Throttling factor
        double throttle_factor = thermal.thermal_throttling_active ? 0.9 : 1.0;

        int estimated_hours = static_cast<int>(base_hours * temp_factor * usage_factor * throttle_factor);

        spdlog::debug("Estimated CPU lifespan: {} hours ({:.1f} years)",
                     estimated_hours, estimated_hours / 8760.0);

        return std::chrono::hours(estimated_hours);

    } catch (const std::exception& e) {
        spdlog::error("Exception in estimateCpuLifespan: {}", e.what());
    }

    return std::chrono::hours(87600); // Default: 10 years
}

}  // namespace atom::system
