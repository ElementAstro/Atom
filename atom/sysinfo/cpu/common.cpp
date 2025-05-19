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
#include <regex>

namespace atom::system {

namespace {
// Cache variables with a validity duration
std::mutex g_cacheMutex;
std::chrono::steady_clock::time_point g_lastCacheRefresh;
const std::chrono::seconds g_cacheValidDuration(
    5);  // Cache valid for 5 seconds

// Cached CPU info
std::atomic<bool> g_cacheInitialized{false};
CpuInfo g_cpuInfoCache;

/**
 * @brief Converts a string to bytes
 * @param str String like "8K" or "4M"
 * @return Size in bytes
 */
size_t stringToBytes(const std::string& str) {
    size_t size = 0;
    std::stringstream ss(str);

    ss >> size;

    if (ss.fail()) {
        return 0;
    }

    char unit = '\0';
    ss >> unit;

    switch (std::toupper(unit)) {
        case 'K':
            size *= 1024;
            break;
        case 'M':
            size *= 1024 * 1024;
            break;
        case 'G':
            size *= 1024 * 1024 * 1024;
            break;
    }    return size;
}

/**
 * @brief Get vendor from CPU identifier string
 * @param vendorId CPU vendor ID string
 * @return CPU vendor enum
 */
CpuVendor getVendorFromString(const std::string& vendorId) {
    std::string vendorLower = vendorId;
    std::transform(vendorLower.begin(), vendorLower.end(), vendorLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (vendorLower.find("intel") != std::string::npos) {
        return CpuVendor::INTEL;
    } else if (vendorLower.find("amd") != std::string::npos) {
        return CpuVendor::AMD;
    } else if (vendorLower.find("arm") != std::string::npos) {
        return CpuVendor::ARM;
    } else if (vendorLower.find("apple") != std::string::npos) {
        return CpuVendor::APPLE;
    } else if (vendorLower.find("qualcomm") != std::string::npos) {
        return CpuVendor::QUALCOMM;
    } else if (vendorLower.find("ibm") != std::string::npos) {
        return CpuVendor::IBM;
    } else if (vendorLower.find("mediatek") != std::string::npos) {
        return CpuVendor::MEDIATEK;
    } else if (vendorLower.find("samsung") != std::string::npos) {
        return CpuVendor::SAMSUNG;
    } else if (!vendorLower.empty()) {
        return CpuVendor::OTHER;
    }

    return CpuVendor::UNKNOWN;
}

/**
 * @brief Check if cache needs refresh
 * @return True if cache needs refresh
 */
bool needsCacheRefresh() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - g_lastCacheRefresh;

    return !g_cacheInitialized || elapsed > g_cacheValidDuration;
}

}  // anonymous namespace

// Common implementation of CPU functions that are not platform-specific

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

void refreshCpuInfo() {
    LOG_F(INFO, "Manually refreshing CPU info cache");
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_lastCacheRefresh = std::chrono::steady_clock::time_point::min();
        g_cacheInitialized = false;
    }

    // Force a refresh by calling getCpuInfo()
    [[maybe_unused]] auto result = getCpuInfo();
    LOG_F(INFO, "CPU info cache refreshed");
}

// Common implementation for getCpuInfo that calls all relevant functions
auto getCpuInfo() -> CpuInfo {
    LOG_F(INFO, "Starting getCpuInfo function");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized) {
            LOG_F(INFO, "Using cached CPU info");
            return g_cpuInfoCache;
        }
    }

    CpuInfo info;

    // Basic information
    info.model = getCPUModel();
    info.identifier = getProcessorIdentifier();
    info.architecture = getCpuArchitecture();
    info.vendor = getCpuVendor();
    info.numPhysicalPackages = getNumberOfPhysicalPackages();
    info.numPhysicalCores = getNumberOfPhysicalCores();
    info.numLogicalCores = getNumberOfLogicalCores();

    // Frequencies
    info.baseFrequency = getProcessorFrequency();
    info.maxFrequency = getMaxProcessorFrequency();

    // Socket information
    info.socketType = getCpuSocketType();

    // Temperature and usage
    info.temperature = getCurrentCpuTemperature();
    info.usage = getCurrentCpuUsage();

    // Cache sizes
    info.caches = getCacheSizes();

    // Power information
    info.power = getCpuPowerInfo();

    // CPU flags
    info.flags = getCpuFeatureFlags();

    // Load average
    info.loadAverage = getCpuLoadAverage();

    // Instruction set
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

    // Parse family/model/stepping from identifier if possible
    std::regex cpuIdRegex(".*Family (\\d+) Model (\\d+) Stepping (\\d+).*");
    std::smatch match;

    if (std::regex_search(info.identifier, match, cpuIdRegex) &&
        match.size() > 3) {
        try {
            info.family = std::stoi(match[1]);
            info.model_id = std::stoi(match[2]);
            info.stepping = std::stoi(match[3]);
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Error parsing CPU family/model/stepping: {}",
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

    // Get per-core information
    std::vector<float> coreUsages = getPerCoreCpuUsage();
    std::vector<float> coreTemps = getPerCoreCpuTemperature();
    std::vector<double> coreFreqs = getPerCoreFrequencies();
    std::vector<std::string> coreGovernors = getPerCoreScalingGovernors();

    int numCores = info.numLogicalCores;
    info.cores.resize(numCores);

    for (int i = 0; i < numCores; ++i) {
        info.cores[i].id = i;
        info.cores[i].usage =
            (i < static_cast<int>(coreUsages.size())) ? coreUsages[i] : 0.0f;
        info.cores[i].temperature =
            (i < static_cast<int>(coreTemps.size())) ? coreTemps[i] : 0.0f;
        info.cores[i].currentFrequency =
            (i < static_cast<int>(coreFreqs.size())) ? coreFreqs[i] : 0.0;
        info.cores[i].maxFrequency = info.maxFrequency;
        info.cores[i].minFrequency = getMinProcessorFrequency();
        info.cores[i].governor = (i < static_cast<int>(coreGovernors.size()))
                                     ? coreGovernors[i]
                                     : "Unknown";
    }

    // Update cache
    {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cpuInfoCache = info;
        g_lastCacheRefresh = std::chrono::steady_clock::now();
        g_cacheInitialized = true;
    }

    LOG_F(INFO, "Finished getCpuInfo function");
    return info;
}

// Implementation for isCpuFeatureSupported that relies on cached feature flags
auto isCpuFeatureSupported(const std::string& feature) -> CpuFeatureSupport {
    LOG_F(INFO, "Checking if CPU feature {} is supported", feature);

    std::string featureLower = feature;
    std::transform(featureLower.begin(), featureLower.end(),
                   featureLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto flags = getCpuFeatureFlags();

    // Direct match
    auto it = std::find(flags.begin(), flags.end(), featureLower);
    if (it != flags.end()) {
        LOG_F(INFO, "Feature {} is directly supported", feature);
        return CpuFeatureSupport::SUPPORTED;
    }

    // Handle special cases and aliases
    if (featureLower == "avx512") {
        // Check for any AVX-512 variant
        for (const auto& flag : flags) {
            if (flag.find("avx512") != std::string::npos) {
                LOG_F(INFO, "AVX-512 feature is supported via {}", flag);
                return CpuFeatureSupport::SUPPORTED;
            }
        }
    } else if (featureLower == "vt" || featureLower == "virtualization") {
        // Check for Intel VT-x or AMD-V
        for (const auto& flag : flags) {
            if (flag == "vmx" || flag == "svm") {
                LOG_F(INFO, "Virtualization is supported via {}", flag);
                return CpuFeatureSupport::SUPPORTED;
            }
        }
    } else if (featureLower == "aes") {
        // Check for AES-NI
        auto aesni = std::find(flags.begin(), flags.end(), "aes");
        if (aesni != flags.end()) {
            LOG_F(INFO, "AES is supported via AES-NI");
            return CpuFeatureSupport::SUPPORTED;
        }
    }

    LOG_F(INFO, "Feature {} is not supported", feature);
    return CpuFeatureSupport::NOT_SUPPORTED;
}

}  // namespace atom::system
