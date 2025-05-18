/*
 * windows.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU Windows Implementation

**************************************************/

#include <thread>
#ifdef _WIN32

#include "common.hpp"

#include <powersetting.h>
#include <powrprof.h>  // Add this header for PowerGetActiveScheme

#pragma comment(lib, "PowrProf.lib")  // Link against the PowerProf library

namespace atom::system {

// 添加Windows特定函数前向声明
auto getCurrentCpuUsage_Windows() -> float;
auto getPerCoreCpuUsage_Windows() -> std::vector<float>;
auto getCurrentCpuTemperature_Windows() -> float;
auto getPerCoreCpuTemperature_Windows() -> std::vector<float>;
auto getCPUModel_Windows() -> std::string;
// 这里应该添加所有函数的前向声明

auto getCurrentCpuUsage_Windows() -> float {
    LOG_F(INFO, "Starting getCurrentCpuUsage function on Windows");

    static PDH_HQUERY cpuQuery = nullptr;
    static PDH_HCOUNTER cpuTotal = nullptr;
    static bool initialized = false;

    float cpuUsage = 0.0F;

    if (!initialized) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        PdhAddEnglishCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time",
                             0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;

        // First call will not return valid data, need to wait and call again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    // Get the CPU usage
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, nullptr, &counterVal);
    cpuUsage = static_cast<float>(counterVal.doubleValue);

    // Clamp the value between 0 and 100
    cpuUsage = std::max(0.0F, std::min(100.0F, cpuUsage));

    LOG_F(INFO, "Windows CPU Usage: {}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuUsage function on Windows");

    static PDH_HQUERY cpuQuery = nullptr;
    static std::vector<PDH_HCOUNTER> cpuCounters;
    static bool initialized = false;

    int numCores = getNumberOfLogicalCores();
    std::vector<float> coreUsages(numCores, 0.0F);

    if (!initialized) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        cpuCounters.resize(numCores);

        for (int i = 0; i < numCores; i++) {
            std::string counterPath =
                "\\Processor(" + std::to_string(i) + ")\\% Processor Time";
            PdhAddEnglishCounter(cpuQuery, counterPath.c_str(), 0,
                                 &cpuCounters[i]);
        }

        PdhCollectQueryData(cpuQuery);
        initialized = true;

        // First call will not return valid data, need to wait and call again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    // Get the CPU usage for each core
    PdhCollectQueryData(cpuQuery);

    for (int i = 0; i < numCores; i++) {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhGetFormattedCounterValue(cpuCounters[i], PDH_FMT_DOUBLE, nullptr,
                                    &counterVal);
        coreUsages[i] = static_cast<float>(counterVal.doubleValue);
        coreUsages[i] = std::max(0.0F, std::min(100.0F, coreUsages[i]));
    }

    LOG_F(INFO, "Windows Per-Core CPU Usage collected for {} cores", numCores);
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    LOG_F(INFO, "Starting getCurrentCpuTemperature function on Windows");

    // Windows doesn't provide a direct API for CPU temperature
    // This would require WMI or third-party libraries like OpenHardwareMonitor
    // A simplified placeholder implementation is provided

    float temperature = 0.0F;

    LOG_F(INFO, "Windows CPU Temperature: {}°C (placeholder value)",
          temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuTemperature function on Windows");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0F);

    // As with getCurrentCpuTemperature, this is a placeholder

    LOG_F(INFO,
          "Windows Per-Core CPU Temperature collected for {} cores "
          "(placeholder values)",
          numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    LOG_F(INFO, "Starting getCPUModel function on Windows");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    int cpuInfo[4] = {-1};
    char cpuBrandString[64] = {0};

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000004) {
        // Get the brand string from EAX=8000000[2,3,4]
        for (unsigned int i = 0x80000002; i <= 0x80000004; i++) {
            __cpuid(cpuInfo, i);
            memcpy(cpuBrandString + (i - 0x80000002) * 16, cpuInfo,
                   sizeof(cpuInfo));
        }
        cpuModel = cpuBrandString;
    }

    // Trim whitespace
    cpuModel.erase(0, cpuModel.find_first_not_of(" \t\n\r\f\v"));
    cpuModel.erase(cpuModel.find_last_not_of(" \t\n\r\f\v") + 1);

    LOG_F(INFO, "Windows CPU Model: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    LOG_F(INFO, "Starting getProcessorIdentifier function on Windows");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier = "Unknown";

    int cpuInfo[4] = {0};
    char vendorID[13] = {0};

    // Get vendor ID
    __cpuid(cpuInfo, 0);
    memcpy(vendorID, &cpuInfo[1], sizeof(int));
    memcpy(vendorID + 4, &cpuInfo[3], sizeof(int));
    memcpy(vendorID + 8, &cpuInfo[2], sizeof(int));
    vendorID[12] = '\0';

    // Get family, model, stepping
    __cpuid(cpuInfo, 1);
    int family = (cpuInfo[0] >> 8) & 0xF;
    int model = (cpuInfo[0] >> 4) & 0xF;
    int extModel = (cpuInfo[0] >> 16) & 0xF;
    int extFamily = (cpuInfo[0] >> 20) & 0xFF;
    int stepping = cpuInfo[0] & 0xF;

    if (family == 0xF) {
        family += extFamily;
    }

    if (family == 0x6 || family == 0xF) {
        model = (extModel << 4) | model;
    }

    identifier = std::string(vendorID) + " Family " + std::to_string(family) +
                 " Model " + std::to_string(model) + " Stepping " +
                 std::to_string(stepping);

    LOG_F(INFO, "Windows CPU Identifier: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getProcessorFrequency function on Windows");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    // Get current frequency (in MHz)
    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        LOG_F(INFO, "Windows CPU Frequency: {} GHz", frequency);
        return frequency;
    }

    LOG_F(INFO, "Failed to get Windows CPU Frequency");
    return 0.0;
}

auto getMinProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMinProcessorFrequency function on Windows");

    // Windows doesn't provide a direct API for minimum CPU frequency
    // This would require reading from the registry or using WMI
    // A placeholder implementation is provided

    double minFreq = 0.0;

    // As a fallback, we can try to get processor information from WMIC
    // For simplicity, we'll return a default value or a fraction of the current
    // frequency
    double currentFreq = getProcessorFrequency();
    if (currentFreq > 0) {
        minFreq = currentFreq * 0.5;  // Estimate as half the current frequency
    }

    LOG_F(INFO, "Windows CPU Min Frequency: {} GHz (estimated)", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMaxProcessorFrequency function on Windows");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    // Try to get the max frequency from registry
    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        LOG_F(INFO, "Windows CPU Max Frequency: {} GHz", frequency);
        return frequency;
    }

    LOG_F(INFO, "Failed to get Windows CPU Max Frequency");
    return getProcessorFrequency();  // Fallback to current frequency
}

auto getPerCoreFrequencies() -> std::vector<double> {
    LOG_F(INFO, "Starting getPerCoreFrequencies function on Windows");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    // Windows doesn't provide an easy way to get per-core frequencies
    // This would require platform-specific hardware monitoring
    // For simplicity, we'll use the same frequency for all cores
    double frequency = getProcessorFrequency();

    for (int i = 0; i < numCores; i++) {
        frequencies[i] = frequency;
    }

    LOG_F(INFO, "Windows Per-Core CPU Frequencies: {} GHz (all cores)",
          frequency);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalPackages function on Windows");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 0;

    // Use WMI to get physical package information
    // This is a simplified placeholder implementation

    // Most desktop/laptop systems have 1 physical package
    numberOfPackages = 1;

    LOG_F(INFO, "Windows Physical CPU Packages: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalCores function on Windows");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    // GetSystemInfo returns logical cores, not physical cores
    // For a more accurate count, we would need to use WMI or similar
    // This is a simplified approximation
    int numberOfCores = sysInfo.dwNumberOfProcessors;

    // Try to account for hyperthreading by dividing by 2
    // This is a very rough approximation
    bool hasHyperthreading = false;

    // Check for hyperthreading capability using CPUID
    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    hasHyperthreading = (cpuInfo[3] & (1 << 28)) != 0;

    if (hasHyperthreading && numberOfCores > 1) {
        numberOfCores = numberOfCores / 2;
    }

    // Ensure we have at least 1 core
    numberOfCores = std::max(1, numberOfCores);

    LOG_F(INFO, "Windows Physical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfLogicalCores function on Windows");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numberOfCores = sysInfo.dwNumberOfProcessors;

    LOG_F(INFO, "Windows Logical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    LOG_F(INFO, "Starting getCacheSizes function on Windows");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // L1 cache size - use CPUID
    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 0);
    int maxFunc = cpuInfo[0];

    if (maxFunc >= 4) {
        // Get cache info using CPUID function 4
        for (int i = 0;; i++) {
            __cpuidex(cpuInfo, 4, i);

            // If no more caches
            if ((cpuInfo[0] & 0x1F) == 0)
                break;

            int level = (cpuInfo[0] >> 5) & 0x7;
            int type = cpuInfo[0] & 0x1F;
            int lineSize = (cpuInfo[1] & 0xFFF) + 1;
            int associativity = ((cpuInfo[1] >> 22) & 0x3FF) + 1;
            int sets = cpuInfo[2] + 1;
            int totalSize = (associativity * lineSize * sets);

            // Type: 1=data, 2=instruction, 3=unified
            switch (level) {
                case 1:
                    if (type == 1) {  // Data cache
                        cacheSizes.l1d = totalSize;
                        cacheSizes.l1d_line_size = lineSize;
                        cacheSizes.l1d_associativity = associativity;
                    } else if (type == 2) {  // Instruction cache
                        cacheSizes.l1i = totalSize;
                        cacheSizes.l1i_line_size = lineSize;
                        cacheSizes.l1i_associativity = associativity;
                    }
                    break;
                case 2:
                    cacheSizes.l2 = totalSize;
                    cacheSizes.l2_line_size = lineSize;
                    cacheSizes.l2_associativity = associativity;
                    break;
                case 3:
                    cacheSizes.l3 = totalSize;
                    cacheSizes.l3_line_size = lineSize;
                    cacheSizes.l3_associativity = associativity;
                    break;
            }
        }
    }

    LOG_F(INFO, "Windows Cache Sizes: L1d={}KB, L1i={}KB, L2={}KB, L3={}KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024,
          cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    LOG_F(INFO, "Starting getCpuLoadAverage function on Windows");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    // Windows doesn't have a direct equivalent to Unix load average
    // Instead, we can use CPU usage as an approximation
    float cpuUsage = getCurrentCpuUsage();

    // Convert to a load-like value
    int numCores = getNumberOfLogicalCores();
    double load = (cpuUsage / 100.0) * numCores;

    // For simplicity, use the same value for all time periods
    loadAvg.oneMinute = load;
    loadAvg.fiveMinutes = load;
    loadAvg.fifteenMinutes = load;

    LOG_F(INFO,
          "Windows Load Average (approximated from CPU usage): {}, {}, {}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    LOG_F(INFO, "Starting getCpuPowerInfo function on Windows");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    // Windows doesn't provide direct CPU power consumption without hardware
    // monitoring This would require platform-specific hardware monitoring
    // libraries

    // For TDP, we could try to read from WMI or simply set a typical value
    // based on the processor model

    LOG_F(INFO,
          "Windows CPU Power Info: currentWatts={}, maxTDP={}, energyImpact={} "
          "(placeholder values)",
          powerInfo.currentWatts, powerInfo.maxTDP, powerInfo.energyImpact);

    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getCpuFeatureFlags function on Windows");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    // CPU feature flags using CPUID
    int cpuInfo[4] = {0};

    // Get standard feature flags
    __cpuid(cpuInfo, 1);

    // EDX register flags
    if (cpuInfo[3] & (1 << 0))
        flags.push_back("fpu");
    if (cpuInfo[3] & (1 << 1))
        flags.push_back("vme");
    if (cpuInfo[3] & (1 << 2))
        flags.push_back("de");
    if (cpuInfo[3] & (1 << 3))
        flags.push_back("pse");
    if (cpuInfo[3] & (1 << 4))
        flags.push_back("tsc");
    if (cpuInfo[3] & (1 << 5))
        flags.push_back("msr");
    if (cpuInfo[3] & (1 << 6))
        flags.push_back("pae");
    if (cpuInfo[3] & (1 << 7))
        flags.push_back("mce");
    if (cpuInfo[3] & (1 << 8))
        flags.push_back("cx8");
    if (cpuInfo[3] & (1 << 9))
        flags.push_back("apic");
    if (cpuInfo[3] & (1 << 11))
        flags.push_back("sep");
    if (cpuInfo[3] & (1 << 12))
        flags.push_back("mtrr");
    if (cpuInfo[3] & (1 << 13))
        flags.push_back("pge");
    if (cpuInfo[3] & (1 << 14))
        flags.push_back("mca");
    if (cpuInfo[3] & (1 << 15))
        flags.push_back("cmov");
    if (cpuInfo[3] & (1 << 16))
        flags.push_back("pat");
    if (cpuInfo[3] & (1 << 17))
        flags.push_back("pse36");
    if (cpuInfo[3] & (1 << 18))
        flags.push_back("psn");
    if (cpuInfo[3] & (1 << 19))
        flags.push_back("clfsh");
    if (cpuInfo[3] & (1 << 21))
        flags.push_back("ds");
    if (cpuInfo[3] & (1 << 22))
        flags.push_back("acpi");
    if (cpuInfo[3] & (1 << 23))
        flags.push_back("mmx");
    if (cpuInfo[3] & (1 << 24))
        flags.push_back("fxsr");
    if (cpuInfo[3] & (1 << 25))
        flags.push_back("sse");
    if (cpuInfo[3] & (1 << 26))
        flags.push_back("sse2");
    if (cpuInfo[3] & (1 << 27))
        flags.push_back("ss");
    if (cpuInfo[3] & (1 << 28))
        flags.push_back("htt");
    if (cpuInfo[3] & (1 << 29))
        flags.push_back("tm");
    if (cpuInfo[3] & (1 << 31))
        flags.push_back("pbe");

    // ECX register flags
    if (cpuInfo[2] & (1 << 0))
        flags.push_back("sse3");
    if (cpuInfo[2] & (1 << 1))
        flags.push_back("pclmulqdq");
    if (cpuInfo[2] & (1 << 3))
        flags.push_back("monitor");
    if (cpuInfo[2] & (1 << 4))
        flags.push_back("ds_cpl");
    if (cpuInfo[2] & (1 << 5))
        flags.push_back("vmx");
    if (cpuInfo[2] & (1 << 6))
        flags.push_back("smx");
    if (cpuInfo[2] & (1 << 7))
        flags.push_back("est");
    if (cpuInfo[2] & (1 << 8))
        flags.push_back("tm2");
    if (cpuInfo[2] & (1 << 9))
        flags.push_back("ssse3");
    if (cpuInfo[2] & (1 << 13))
        flags.push_back("cx16");
    if (cpuInfo[2] & (1 << 19))
        flags.push_back("sse4_1");
    if (cpuInfo[2] & (1 << 20))
        flags.push_back("sse4_2");
    if (cpuInfo[2] & (1 << 21))
        flags.push_back("x2apic");
    if (cpuInfo[2] & (1 << 22))
        flags.push_back("movbe");
    if (cpuInfo[2] & (1 << 23))
        flags.push_back("popcnt");
    if (cpuInfo[2] & (1 << 25))
        flags.push_back("aes");
    if (cpuInfo[2] & (1 << 26))
        flags.push_back("xsave");
    if (cpuInfo[2] & (1 << 28))
        flags.push_back("avx");
    if (cpuInfo[2] & (1 << 29))
        flags.push_back("f16c");
    if (cpuInfo[2] & (1 << 30))
        flags.push_back("rdrnd");

    // Check for extended features
    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000001) {
        __cpuid(cpuInfo, 0x80000001);

        // EDX
        if (cpuInfo[3] & (1 << 11))
            flags.push_back("syscall");
        if (cpuInfo[3] & (1 << 20))
            flags.push_back("nx");
        if (cpuInfo[3] & (1 << 29))
            flags.push_back("lm");  // Long Mode (64-bit)

        // ECX
        if (cpuInfo[2] & (1 << 0))
            flags.push_back("lahf_lm");
        if (cpuInfo[2] & (1 << 5))
            flags.push_back("abm");
        if (cpuInfo[2] & (1 << 6))
            flags.push_back("sse4a");
        if (cpuInfo[2] & (1 << 8))
            flags.push_back("3dnowprefetch");
        if (cpuInfo[2] & (1 << 11))
            flags.push_back("xop");
        if (cpuInfo[2] & (1 << 12))
            flags.push_back("fma4");
    }

    // Check for AVX2 and other newer features (CPUID 7)
    __cpuidex(cpuInfo, 7, 0);

    // EBX
    if (cpuInfo[1] & (1 << 5))
        flags.push_back("avx2");
    if (cpuInfo[1] & (1 << 3))
        flags.push_back("bmi1");
    if (cpuInfo[1] & (1 << 8))
        flags.push_back("bmi2");

    // Check for AVX-512 features
    if (cpuInfo[1] & (1 << 16))
        flags.push_back("avx512f");
    if (cpuInfo[1] & (1 << 17))
        flags.push_back("avx512dq");
    if (cpuInfo[1] & (1 << 21))
        flags.push_back("avx512ifma");
    if (cpuInfo[1] & (1 << 26))
        flags.push_back("avx512pf");
    if (cpuInfo[1] & (1 << 27))
        flags.push_back("avx512er");
    if (cpuInfo[1] & (1 << 28))
        flags.push_back("avx512cd");
    if (cpuInfo[1] & (1 << 30))
        flags.push_back("avx512bw");
    if (cpuInfo[1] & (1 << 31))
        flags.push_back("avx512vl");

    // ECX
    if (cpuInfo[2] & (1 << 1))
        flags.push_back("avx512vbmi");
    if (cpuInfo[2] & (1 << 6))
        flags.push_back("avx512vbmi2");

    LOG_F(INFO, "Windows CPU Flags: {} features collected", flags.size());

    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    LOG_F(INFO, "Starting getCpuArchitecture function on Windows");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized &&
            g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            arch = CpuArchitecture::X86_64;
            break;
        case PROCESSOR_ARCHITECTURE_INTEL:
            arch = CpuArchitecture::X86;
            break;
        case PROCESSOR_ARCHITECTURE_ARM:
            arch = CpuArchitecture::ARM;
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            arch = CpuArchitecture::ARM64;
            break;
        default:
            arch = CpuArchitecture::UNKNOWN;
            break;
    }

    LOG_F(INFO, "Windows CPU Architecture: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    LOG_F(INFO, "Starting getCpuVendor function on Windows");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.vendor != CpuVendor::UNKNOWN) {
            return g_cpuInfoCache.vendor;
        }
    }

    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString;

    int cpuInfo[4] = {0};
    char vendorID[13] = {0};

    __cpuid(cpuInfo, 0);
    memcpy(vendorID, &cpuInfo[1], sizeof(int));
    memcpy(vendorID + 4, &cpuInfo[3], sizeof(int));
    memcpy(vendorID + 8, &cpuInfo[2], sizeof(int));
    vendorID[12] = '\0';

    vendorString = vendorID;
    vendor = getVendorFromString(vendorString);

    LOG_F(INFO, "Windows CPU Vendor: {} ({})", vendorString,
          cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    LOG_F(INFO, "Starting getCpuSocketType function on Windows");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    // Windows doesn't provide a direct API for socket type
    // This would require WMI or similar advanced techniques
    // This is a placeholder implementation

    LOG_F(INFO, "Windows CPU Socket Type: {} (placeholder)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    LOG_F(INFO, "Starting getCpuScalingGovernor function on Windows");

    std::string governor = "Unknown";

    GUID* activePlanGuid = NULL;
    if (PowerGetActiveScheme(NULL, &activePlanGuid) == ERROR_SUCCESS) {
        // First, get the required buffer size
        DWORD bufferSize = 0;
        PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL, NULL,
                              &bufferSize);

        if (bufferSize > 0) {
            // Allocate buffer of the correct type
            std::vector<BYTE> buffer(bufferSize);

            // Get the friendly name
            if (PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL,
                                      buffer.data(),
                                      &bufferSize) == ERROR_SUCCESS) {
                // The result is a wide string (UTF-16)
                LPWSTR friendlyName = reinterpret_cast<LPWSTR>(buffer.data());

                // Convert wide string to UTF-8
                int narrowBufferSize = WideCharToMultiByte(
                    CP_UTF8, 0, friendlyName, -1, NULL, 0, NULL, NULL);
                if (narrowBufferSize > 0) {
                    std::vector<char> narrowBuffer(narrowBufferSize);
                    if (WideCharToMultiByte(CP_UTF8, 0, friendlyName, -1,
                                            narrowBuffer.data(),
                                            narrowBufferSize, NULL, NULL) > 0) {
                        governor = narrowBuffer.data();
                    }
                }
            }
        }

        LocalFree(activePlanGuid);
    }

    LOG_F(INFO, "Windows Power Plan: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getPerCoreScalingGovernors function on Windows");

    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores);

    // Windows doesn't have per-core power modes, use system-wide setting for
    // all
    std::string governor = getCpuScalingGovernor();

    for (int i = 0; i < numCores; ++i) {
        governors[i] = governor;
    }

    LOG_F(INFO, "Windows Per-Core Power Plans: {} (same for all cores)",
          governor);
    return governors;
}

}  // namespace atom::system

#endif /* _WIN32 */
