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
    spdlog::info("Invoking getCurrentCpuUsage_Windows to retrieve overall CPU usage on Windows.");

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

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, nullptr, &counterVal);
    cpuUsage = static_cast<float>(counterVal.doubleValue);

    cpuUsage = std::max(0.0F, std::min(100.0F, cpuUsage));

    spdlog::info("Overall CPU usage on Windows: {:.2f}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuUsage to retrieve per-core CPU usage statistics on Windows.");

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

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        PdhCollectQueryData(cpuQuery);
    }

    PdhCollectQueryData(cpuQuery);

    for (int i = 0; i < numCores; i++) {
        PDH_FMT_COUNTERVALUE counterVal;
        PdhGetFormattedCounterValue(cpuCounters[i], PDH_FMT_DOUBLE, nullptr,
                                    &counterVal);
        coreUsages[i] = static_cast<float>(counterVal.doubleValue);
        coreUsages[i] = std::max(0.0F, std::min(100.0F, coreUsages[i]));
    }

    spdlog::info("Collected per-core CPU usage for {} logical cores on Windows.", numCores);
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    spdlog::info("Invoking getCurrentCpuTemperature to retrieve CPU temperature on Windows (placeholder implementation).");

    float temperature = 0.0F;

    spdlog::info("CPU temperature on Windows: {:.2f}°C (placeholder value)", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuTemperature to retrieve per-core CPU temperatures on Windows (placeholder implementation).");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0F);

    spdlog::info("Per-core CPU temperatures on Windows: placeholder values for {} logical cores.", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    spdlog::info("Invoking getCPUModel to retrieve the CPU model string on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    int cpuInfo[4] = {-1};
    char cpuBrandString[64] = {0};

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000004) {
        for (unsigned int i = 0x80000002; i <= 0x80000004; i++) {
            __cpuid(cpuInfo, i);
            memcpy(cpuBrandString + (i - 0x80000002) * 16, cpuInfo,
                   sizeof(cpuInfo));
        }
        cpuModel = cpuBrandString;
    }

    cpuModel.erase(0, cpuModel.find_first_not_of(" \t\n\r\f\v"));
    cpuModel.erase(cpuModel.find_last_not_of(" \t\n\r\f\v") + 1);

    spdlog::info("Detected CPU model on Windows: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    spdlog::info("Invoking getProcessorIdentifier to retrieve the processor identifier on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier = "Unknown";

    int cpuInfo[4] = {0};
    char vendorID[13] = {0};

    __cpuid(cpuInfo, 0);
    memcpy(vendorID, &cpuInfo[1], sizeof(int));
    memcpy(vendorID + 4, &cpuInfo[3], sizeof(int));
    memcpy(vendorID + 8, &cpuInfo[2], sizeof(int));
    vendorID[12] = '\0';

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

    spdlog::info("Constructed processor identifier on Windows: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    spdlog::info("Invoking getProcessorFrequency to retrieve the current CPU frequency on Windows.");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        spdlog::info("Current CPU frequency on Windows: {:.3f} GHz", frequency);
        return frequency;
    }

    spdlog::warn("Failed to retrieve current CPU frequency on Windows.");
    return 0.0;
}

auto getMinProcessorFrequency() -> double {
    spdlog::info("Invoking getMinProcessorFrequency to retrieve the minimum CPU frequency on Windows.");

    double minFreq = 0.0;

    double currentFreq = getProcessorFrequency();
    if (currentFreq > 0) {
        minFreq = currentFreq * 0.5;
        spdlog::info("Estimated minimum CPU frequency as half of current: {:.3f} GHz", minFreq);
    }

    spdlog::info("Minimum CPU frequency on Windows: {:.3f} GHz (estimated)", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    spdlog::info("Invoking getMaxProcessorFrequency to retrieve the maximum CPU frequency on Windows.");

    DWORD bufSize = sizeof(DWORD);
    DWORD mhz = 0;

    if (RegGetValue(HKEY_LOCAL_MACHINE,
                    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                    "~MHz", RRF_RT_REG_DWORD, nullptr, &mhz,
                    &bufSize) == ERROR_SUCCESS) {
        double frequency = static_cast<double>(mhz) / 1000.0;
        spdlog::info("Maximum CPU frequency on Windows: {:.3f} GHz", frequency);
        return frequency;
    }

    spdlog::warn("Failed to retrieve maximum CPU frequency on Windows. Using current frequency as fallback.");
    return getProcessorFrequency();
}

auto getPerCoreFrequencies() -> std::vector<double> {
    spdlog::info("Invoking getPerCoreFrequencies to retrieve per-core CPU frequencies on Windows.");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    double frequency = getProcessorFrequency();

    for (int i = 0; i < numCores; i++) {
        frequencies[i] = frequency;
    }

    spdlog::info("Assigned CPU frequency {:.3f} GHz to all {} logical cores on Windows.", frequency, numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    spdlog::info("Invoking getNumberOfPhysicalPackages to determine the number of physical CPU packages on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 1;

    spdlog::info("Number of physical CPU packages detected on Windows: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    spdlog::info("Invoking getNumberOfPhysicalCores to determine the number of physical CPU cores on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numberOfCores = sysInfo.dwNumberOfProcessors;

    bool hasHyperthreading = false;

    int cpuInfo[4] = {0};
    __cpuid(cpuInfo, 1);
    hasHyperthreading = (cpuInfo[3] & (1 << 28)) != 0;

    if (hasHyperthreading && numberOfCores > 1) {
        numberOfCores = numberOfCores / 2;
    }

    numberOfCores = std::max(1, numberOfCores);

    spdlog::info("Number of physical CPU cores detected on Windows: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    spdlog::info("Invoking getNumberOfLogicalCores to determine the number of logical CPU cores on Windows.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    int numberOfCores = sysInfo.dwNumberOfProcessors;

    spdlog::info("Number of logical CPU cores detected on Windows: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    spdlog::info("Invoking getCacheSizes to retrieve CPU cache sizes on Windows.");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 0);
    int maxFunc = cpuInfo[0];

    if (maxFunc >= 4) {
        for (int i = 0;; i++) {
            __cpuidex(cpuInfo, 4, i);

            if ((cpuInfo[0] & 0x1F) == 0)
                break;

            int level = (cpuInfo[0] >> 5) & 0x7;
            int type = cpuInfo[0] & 0x1F;
            int lineSize = (cpuInfo[1] & 0xFFF) + 1;
            int associativity = ((cpuInfo[1] >> 22) & 0x3FF) + 1;
            int sets = cpuInfo[2] + 1;
            int totalSize = (associativity * lineSize * sets);

            switch (level) {
                case 1:
                    if (type == 1) {
                        cacheSizes.l1d = totalSize;
                        cacheSizes.l1d_line_size = lineSize;
                        cacheSizes.l1d_associativity = associativity;
                    } else if (type == 2) {
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

    spdlog::info("Cache sizes on Windows: L1d={} KB, L1i={} KB, L2={} KB, L3={} KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024,
          cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    spdlog::info("Invoking getCpuLoadAverage to retrieve system load averages on Windows (approximated from CPU usage).");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    float cpuUsage = getCurrentCpuUsage();

    int numCores = getNumberOfLogicalCores();
    double load = (cpuUsage / 100.0) * numCores;

    loadAvg.oneMinute = load;
    loadAvg.fiveMinutes = load;
    loadAvg.fifteenMinutes = load;

    spdlog::info("System load averages on Windows (approximated): 1min={:.2f}, 5min={:.2f}, 15min={:.2f}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    spdlog::info("Invoking getCpuPowerInfo to retrieve CPU power information on Windows (not implemented).");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    spdlog::info("CPU power information retrieval is not implemented for Windows.");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    spdlog::info("Invoking getCpuFeatureFlags to retrieve CPU feature flags on Windows.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    int cpuInfo[4] = {0};

    __cpuid(cpuInfo, 1);

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

    __cpuid(cpuInfo, 0x80000000);
    unsigned int nExIds = cpuInfo[0];

    if (nExIds >= 0x80000001) {
        __cpuid(cpuInfo, 0x80000001);

        if (cpuInfo[3] & (1 << 11))
            flags.push_back("syscall");
        if (cpuInfo[3] & (1 << 20))
            flags.push_back("nx");
        if (cpuInfo[3] & (1 << 29))
            flags.push_back("lm");

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

    __cpuidex(cpuInfo, 7, 0);

    if (cpuInfo[1] & (1 << 5))
        flags.push_back("avx2");
    if (cpuInfo[1] & (1 << 3))
        flags.push_back("bmi1");
    if (cpuInfo[1] & (1 << 8))
        flags.push_back("bmi2");

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

    if (cpuInfo[2] & (1 << 1))
        flags.push_back("avx512vbmi");
    if (cpuInfo[2] & (1 << 6))
        flags.push_back("avx512vbmi2");

    spdlog::info("Collected {} CPU feature flags on Windows.", flags.size());

    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    spdlog::info("Invoking getCpuArchitecture to determine the CPU architecture on Windows.");

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

    spdlog::info("Detected CPU architecture on Windows: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    spdlog::info("Invoking getCpuVendor to determine the CPU vendor on Windows.");

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

    spdlog::info("Detected CPU vendor on Windows: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    spdlog::info("Invoking getCpuSocketType to retrieve the CPU socket type on Windows (placeholder implementation).");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    spdlog::info("CPU socket type on Windows: {} (no direct method available, placeholder value)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    spdlog::info("Invoking getCpuScalingGovernor to retrieve the CPU scaling governor (power plan) on Windows.");

    std::string governor = "Unknown";

    GUID* activePlanGuid = NULL;
    if (PowerGetActiveScheme(NULL, &activePlanGuid) == ERROR_SUCCESS) {
        DWORD bufferSize = 0;
        PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL, NULL,
                              &bufferSize);

        if (bufferSize > 0) {
            std::vector<BYTE> buffer(bufferSize);

            if (PowerReadFriendlyName(NULL, activePlanGuid, NULL, NULL,
                                      buffer.data(),
                                      &bufferSize) == ERROR_SUCCESS) {
                LPWSTR friendlyName = reinterpret_cast<LPWSTR>(buffer.data());

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

    spdlog::info("CPU scaling governor (power plan) on Windows: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    spdlog::info("Invoking getPerCoreScalingGovernors to retrieve per-core CPU scaling governors on Windows.");

    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores);

    std::string governor = getCpuScalingGovernor();

    for (int i = 0; i < numCores; ++i) {
        governors[i] = governor;
    }

    spdlog::info("Assigned CPU scaling governor '{}' to all {} logical cores on Windows.", governor, numCores);
    return governors;
}

}  // namespace atom::system

#endif /* _WIN32 */
