/*
 * macos.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU macOS Implementation

**************************************************/

#ifdef __APPLE__

#include "common.hpp"

namespace atom::system {

// 添加MacOS特定函数前向声明
auto getCurrentCpuUsage_MacOS() -> float;
auto getPerCoreCpuUsage_MacOS() -> std::vector<float>;
auto getCurrentCpuTemperature_MacOS() -> float;
auto getPerCoreCpuTemperature_MacOS() -> std::vector<float>;
auto getCPUModel_MacOS() -> std::string;
// 这里应该添加所有函数的前向声明

auto getCurrentCpuUsage_MacOS() -> float {
    spdlog::info("Invoking getCurrentCpuUsage_MacOS to retrieve overall CPU usage on macOS.");

    processor_cpu_load_info_t cpuInfo;
    mach_msg_type_number_t count;

    float cpuUsage = 0.0F;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_array_t*>(&cpuInfo),
                           &count) == KERN_SUCCESS) {

        static unsigned long long previousUser = 0, previousSystem = 0, previousIdle = 0;

        unsigned long long user = 0, system = 0, idle = 0;

        for (unsigned i = 0; i < count / CPU_STATE_MAX; i++) {
            user += cpuInfo[i].cpu_ticks[CPU_STATE_USER] + cpuInfo[i].cpu_ticks[CPU_STATE_NICE];
            system += cpuInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
            idle += cpuInfo[i].cpu_ticks[CPU_STATE_IDLE];
        }

        if (previousUser > 0 || previousSystem > 0 || previousIdle > 0) {
            unsigned long long userDiff = user - previousUser;
            unsigned long long systemDiff = system - previousSystem;
            unsigned long long idleDiff = idle - previousIdle;

            unsigned long long totalTicks = userDiff + systemDiff + idleDiff;

            if (totalTicks > 0) {
                cpuUsage = 100.0F * (static_cast<float>(userDiff + systemDiff) / totalTicks);
            }
        }

        previousUser = user;
        previousSystem = system;
        previousIdle = idle;

        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuInfo), count);
    }

    cpuUsage = std::max(0.0F, std::min(100.0F, cpuUsage));

    spdlog::info("Overall CPU usage on macOS: {:.2f}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuUsage to retrieve per-core CPU usage statistics on macOS.");

    processor_cpu_load_info_t cpuInfo;
    mach_msg_type_number_t count;

    std::vector<float> coreUsages;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_array_t*>(&cpuInfo),
                           &count) == KERN_SUCCESS) {

        static std::vector<unsigned long long> previousUser, previousSystem, previousIdle;

        int numCores = count / CPU_STATE_MAX;
        coreUsages.resize(numCores, 0.0F);

        if (previousUser.size() < static_cast<size_t>(numCores)) {
            previousUser.resize(numCores, 0);
            previousSystem.resize(numCores, 0);
            previousIdle.resize(numCores, 0);
        }

        for (int i = 0; i < numCores; i++) {
            unsigned long long user = cpuInfo[i].cpu_ticks[CPU_STATE_USER] + cpuInfo[i].cpu_ticks[CPU_STATE_NICE];
            unsigned long long system = cpuInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
            unsigned long long idle = cpuInfo[i].cpu_ticks[CPU_STATE_IDLE];

            if (previousUser[i] > 0 || previousSystem[i] > 0 || previousIdle[i] > 0) {
                unsigned long long userDiff = user - previousUser[i];
                unsigned long long systemDiff = system - previousSystem[i];
                unsigned long long idleDiff = idle - previousIdle[i];

                unsigned long long totalTicks = userDiff + systemDiff + idleDiff;

                if (totalTicks > 0) {
                    coreUsages[i] = 100.0F * (static_cast<float>(userDiff + systemDiff) / totalTicks);
                    coreUsages[i] = std::max(0.0F, std::min(100.0F, coreUsages[i]));
                }
            }

            previousUser[i] = user;
            previousSystem[i] = system;
            previousIdle[i] = idle;
        }

        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuInfo), count);
    }

    spdlog::info("Collected per-core CPU usage for {} logical cores on macOS.", coreUsages.size());
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    spdlog::info("Invoking getCurrentCpuTemperature to retrieve CPU temperature on macOS (not implemented).");

    float temperature = 0.0F;

    spdlog::info("CPU temperature on macOS: {:.2f}°C (not implemented, placeholder value)", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuTemperature to retrieve per-core CPU temperatures on macOS (not implemented).");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0F);

    spdlog::info("Per-core CPU temperatures on macOS: not implemented, returning zeros for {} logical cores.", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    spdlog::info("Invoking getCPUModel to retrieve the CPU model string on macOS.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    char buffer[1024];
    size_t bufferSize = sizeof(buffer);

    if (sysctlbyname("machdep.cpu.brand_string", buffer, &bufferSize, NULL, 0) == 0) {
        cpuModel = buffer;
    } else {
        if (sysctlbyname("machdep.cpu.brand", buffer, &bufferSize, NULL, 0) == 0) {
            cpuModel = buffer;

            char modelBuffer[256];
            size_t modelBufferSize = sizeof(modelBuffer);

            if (sysctlbyname("hw.model", modelBuffer, &modelBufferSize, NULL, 0) == 0) {
                if (std::string(modelBuffer).find("Mac") != std::string::npos) {
                    cpuModel += " " + std::string(modelBuffer);
                }
            }
        }
    }

    spdlog::info("Detected CPU model on macOS: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    spdlog::info("Invoking getProcessorIdentifier to retrieve the processor identifier on macOS.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier = "Unknown";

    char vendor[64];
    int family = 0, model = 0, stepping = 0;
    size_t size = sizeof(vendor);

    if (sysctlbyname("machdep.cpu.vendor", vendor, &size, NULL, 0) == 0) {
        size = sizeof(family);
        sysctlbyname("machdep.cpu.family", &family, &size, NULL, 0);

        size = sizeof(model);
        sysctlbyname("machdep.cpu.model", &model, &size, NULL, 0);

        size = sizeof(stepping);
        sysctlbyname("machdep.cpu.stepping", &stepping, &size, NULL, 0);

        identifier = std::string(vendor) + " Family " + std::to_string(family) +
                     " Model " + std::to_string(model) +
                     " Stepping " + std::to_string(stepping);
    } else {
        char buffer[256];
        size = sizeof(buffer);

        if (sysctlbyname("machdep.cpu.brand", buffer, &size, NULL, 0) == 0) {
            identifier = buffer;
        }
    }

    spdlog::info("Constructed processor identifier on macOS: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    spdlog::info("Invoking getProcessorFrequency to retrieve the current CPU frequency on macOS.");

    double frequency = 0.0;

    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
        frequency = static_cast<double>(freq) / 1000000000.0;
    } else {
        unsigned int freqMHz = 0;
        size = sizeof(freqMHz);

        if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
            frequency = static_cast<double>(freq) / 1000000000.0;
        } else if (sysctlbyname("hw.cpufrequency_max", &freqMHz, &size, NULL, 0) == 0) {
            frequency = static_cast<double>(freqMHz) / 1000.0;
        }
    }

    spdlog::info("Current CPU frequency on macOS: {:.3f} GHz", frequency);
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    spdlog::info("Invoking getMinProcessorFrequency to retrieve the minimum CPU frequency on macOS.");

    double minFreq = 0.0;

    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency_min", &freq, &size, NULL, 0) == 0) {
        minFreq = static_cast<double>(freq) / 1000000000.0;
    }

    if (minFreq <= 0.0) {
        double currentFreq = getProcessorFrequency();
        if (currentFreq > 0.0) {
            minFreq = currentFreq * 0.5;
            spdlog::info("Estimated minimum CPU frequency as half of current: {:.3f} GHz", minFreq);
        } else {
            minFreq = 1.0;
        }
    }

    spdlog::info("Minimum CPU frequency on macOS: {:.3f} GHz", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    spdlog::info("Invoking getMaxProcessorFrequency to retrieve the maximum CPU frequency on macOS.");

    double maxFreq = 0.0;

    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
        maxFreq = static_cast<double>(freq) / 1000000000.0;
    } else {
        if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
            maxFreq = static_cast<double>(freq) / 1000000000.0;
        }
    }

    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        spdlog::info("Using current CPU frequency as maximum: {:.3f} GHz", maxFreq);
    }

    spdlog::info("Maximum CPU frequency on macOS: {:.3f} GHz", maxFreq);
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    spdlog::info("Invoking getPerCoreFrequencies to retrieve per-core CPU frequencies on macOS.");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    double frequency = getProcessorFrequency();

    for (int i = 0; i < numCores; i++) {
        frequencies[i] = frequency;
    }

    spdlog::info("Assigned CPU frequency {:.3f} GHz to all {} logical cores on macOS.", frequency, numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    spdlog::info("Invoking getNumberOfPhysicalPackages to determine the number of physical CPU packages on macOS.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 1;

    spdlog::info("Number of physical CPU packages detected on macOS: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    spdlog::info("Invoking getNumberOfPhysicalCores to determine the number of physical CPU cores on macOS.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    int numberOfCores = 0;

    int physCores = 0;
    size_t size = sizeof(physCores);

    if (sysctlbyname("hw.physicalcpu", &physCores, &size, NULL, 0) == 0) {
        numberOfCores = physCores;
    } else {
        numberOfCores = getNumberOfLogicalCores() / 2;
    }

    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    spdlog::info("Number of physical CPU cores detected on macOS: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    spdlog::info("Invoking getNumberOfLogicalCores to determine the number of logical CPU cores on macOS.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    int numberOfCores = 0;

    int logicalCores = 0;
    size_t size = sizeof(logicalCores);

    if (sysctlbyname("hw.logicalcpu", &logicalCores, &size, NULL, 0) == 0) {
        numberOfCores = logicalCores;
    } else {
        if (sysctlbyname("hw.ncpu", &logicalCores, &size, NULL, 0) == 0) {
            numberOfCores = logicalCores;
        } else {
            numberOfCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
        }
    }

    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    spdlog::info("Number of logical CPU cores detected on macOS: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    spdlog::info("Invoking getCacheSizes to retrieve CPU cache sizes on macOS.");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    uint64_t cacheSize = 0;
    size_t size = sizeof(cacheSize);

    if (sysctlbyname("hw.l1dcachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1d = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l1icachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1i = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l2cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l2 = static_cast<size_t>(cacheSize);
    }

    if (sysctlbyname("hw.l3cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l3 = static_cast<size_t>(cacheSize);
    }

    int lineSize = 0;
    size = sizeof(lineSize);

    if (sysctlbyname("hw.cachelinesize", &lineSize, &size, NULL, 0) == 0) {
        cacheSizes.l1d_line_size = lineSize;
        cacheSizes.l1i_line_size = lineSize;
        cacheSizes.l2_line_size = lineSize;
        cacheSizes.l3_line_size = lineSize;
    }

    int l2associativity = 0;
    size = sizeof(l2associativity);
    if (sysctlbyname("machdep.cpu.cache.L2_associativity", &l2associativity, &size, NULL, 0) == 0) {
        cacheSizes.l2_associativity = l2associativity;
    }

    spdlog::info("Cache sizes on macOS: L1d={} KB, L1i={} KB, L2={} KB, L3={} KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024, cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    spdlog::info("Invoking getCpuLoadAverage to retrieve system load averages on macOS.");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    double avg[3];
    if (getloadavg(avg, 3) == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
    }

    spdlog::info("System load averages on macOS: 1min={:.2f}, 5min={:.2f}, 15min={:.2f}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    spdlog::info("Invoking getCpuPowerInfo to retrieve CPU power information on macOS (not implemented).");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    spdlog::info("CPU power information retrieval is not implemented for macOS.");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    spdlog::info("Invoking getCpuFeatureFlags to retrieve CPU feature flags on macOS.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    auto checkFeature = [&flags](const char* name) {
        int supported = 0;
        size_t size = sizeof(supported);

        if (sysctlbyname(name, &supported, &size, NULL, 0) == 0 && supported) {
            std::string featureName = name;
            size_t pos = featureName.rfind('.');
            if (pos != std::string::npos && pos + 1 < featureName.length()) {
                flags.push_back(featureName.substr(pos + 1));
            }
        }
    };

    checkFeature("hw.optional.floatingpoint");
    checkFeature("hw.optional.mmx");
    checkFeature("hw.optional.sse");
    checkFeature("hw.optional.sse2");
    checkFeature("hw.optional.sse3");
    checkFeature("hw.optional.supplementalsse3");
    checkFeature("hw.optional.sse4_1");
    checkFeature("hw.optional.sse4_2");
    checkFeature("hw.optional.aes");
    checkFeature("hw.optional.avx1_0");
    checkFeature("hw.optional.avx2_0");
    checkFeature("hw.optional.x86_64");
    checkFeature("hw.optional.rdrand");
    checkFeature("hw.optional.f16c");
    checkFeature("hw.optional.enfstrg");
    checkFeature("hw.optional.fma");
    checkFeature("hw.optional.avx512f");
    checkFeature("hw.optional.avx512cd");
    checkFeature("hw.optional.avx512dq");
    checkFeature("hw.optional.avx512bw");
    checkFeature("hw.optional.avx512vl");
    checkFeature("hw.optional.avx512ifma");
    checkFeature("hw.optional.avx512vbmi");

    checkFeature("hw.optional.neon");
    checkFeature("hw.optional.armv8_1_atomics");
    checkFeature("hw.optional.armv8_2_fhm");
    checkFeature("hw.optional.armv8_2_sha512");
    checkFeature("hw.optional.armv8_2_sha3");
    checkFeature("hw.optional.amx_version");
    checkFeature("hw.optional.ucnormal_mem");

    spdlog::info("Collected {} CPU feature flags on macOS.", flags.size());
    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    spdlog::info("Invoking getCpuArchitecture to determine the CPU architecture on macOS.");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

#ifdef __x86_64__
    arch = CpuArchitecture::X86_64;
#elif defined(__i386__)
    arch = CpuArchitecture::X86;
#elif defined(__arm64__) || defined(__aarch64__)
    arch = CpuArchitecture::ARM64;
#elif defined(__arm__)
    arch = CpuArchitecture::ARM;
#else
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0) {
        std::string machine = sysInfo.machine;

        if (machine == "x86_64") {
            arch = CpuArchitecture::X86_64;
        } else if (machine == "i386") {
            arch = CpuArchitecture::X86;
        } else if (machine == "arm64") {
            arch = CpuArchitecture::ARM64;
        } else if (machine.find("arm") != std::string::npos) {
            arch = CpuArchitecture::ARM;
        }
    }
#endif

    spdlog::info("Detected CPU architecture on macOS: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    spdlog::info("Invoking getCpuVendor to determine the CPU vendor on macOS.");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.vendor != CpuVendor::UNKNOWN) {
            return g_cpuInfoCache.vendor;
        }
    }

    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString = "Unknown";

    char buffer[64];
    size_t size = sizeof(buffer);

    if (sysctlbyname("machdep.cpu.vendor", buffer, &size, NULL, 0) == 0) {
        vendorString = buffer;
    } else {
        CpuArchitecture arch = getCpuArchitecture();
        if (arch == CpuArchitecture::ARM64 || arch == CpuArchitecture::ARM) {
            vendorString = "Apple";
        }
    }

    vendor = getVendorFromString(vendorString);

    spdlog::info("Detected CPU vendor on macOS: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    spdlog::info("Invoking getCpuSocketType to retrieve the CPU socket type on macOS.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    CpuArchitecture arch = getCpuArchitecture();

    if (arch == CpuArchitecture::ARM64 || arch == CpuArchitecture::ARM) {
        socketType = "Apple SoC";
    } else {
        socketType = "Intel Mac";
    }

    spdlog::info("CPU socket type on macOS: {}", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    spdlog::info("Invoking getCpuScalingGovernor to retrieve the CPU scaling governor on macOS.");

    std::string governor = "Unknown";

    int perfMode = 0;
    size_t size = sizeof(perfMode);

    if (sysctlbyname("hw.perflevel0.frequency", &perfMode, &size, NULL, 0) == 0) {
        governor = "perflevel";
    } else {
        CFTypeRef powerSourceInfo = IOPSCopyPowerSourcesInfo();
        if (powerSourceInfo) {
            CFArrayRef powerSources = IOPSCopyPowerSourcesList(powerSourceInfo);

            if (powerSources && CFArrayGetCount(powerSources) > 0) {
                CFDictionaryRef powerSource = (CFDictionaryRef)CFArrayGetValueAtIndex(powerSources, 0);
                CFStringRef powerSourceState = (CFStringRef)CFDictionaryGetValue(powerSource, CFSTR(kIOPSPowerSourceStateKey));

                if (powerSourceState) {
                    bool onBattery = CFStringCompare(powerSourceState, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo;
                    governor = onBattery ? "Battery Power" : "AC Power";
                }
            }

            if (powerSources) {
                CFRelease(powerSources);
            }
            CFRelease(powerSourceInfo);
        }
    }

    spdlog::info("CPU scaling governor (power mode) on macOS: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    spdlog::info("Invoking getPerCoreScalingGovernors to retrieve per-core CPU scaling governors on macOS.");

    int numCores = getNumberOfLogicalCores();
    std::string governor = getCpuScalingGovernor();

    std::vector<std::string> governors(numCores, governor);

    spdlog::info("Assigned CPU scaling governor '{}' to all {} logical cores on macOS.", governor, numCores);
    return governors;
}
