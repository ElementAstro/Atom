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
    LOG_F(INFO, "Starting getCurrentCpuUsage function on macOS");

    processor_cpu_load_info_t cpuInfo;
    mach_msg_type_number_t count;

    float cpuUsage = 0.0F;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_array_t*>(&cpuInfo),
                           &count) == KERN_SUCCESS) {

        static unsigned long long previousUser = 0, previousSystem = 0, previousIdle = 0;

        unsigned long long user = 0, system = 0, idle = 0;

        // Sum usage across all CPUs
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

        // Free the allocated memory
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuInfo), count);
    }

    // Clamp to 0-100 range
    cpuUsage = std::max(0.0F, std::min(100.0F, cpuUsage));

    LOG_F(INFO, "macOS CPU Usage: {}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuUsage function on macOS");

    processor_cpu_load_info_t cpuInfo;
    mach_msg_type_number_t count;

    std::vector<float> coreUsages;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &count,
                           reinterpret_cast<processor_info_array_t*>(&cpuInfo),
                           &count) == KERN_SUCCESS) {

        static std::vector<unsigned long long> previousUser, previousSystem, previousIdle;

        int numCores = count / CPU_STATE_MAX;
        coreUsages.resize(numCores, 0.0F);

        // Resize previous vectors if needed
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

        // Free the allocated memory
        vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(cpuInfo), count);
    }

    LOG_F(INFO, "macOS Per-Core CPU Usage collected for {} cores", coreUsages.size());
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    LOG_F(INFO, "Starting getCurrentCpuTemperature function on macOS");

    // macOS doesn't provide a direct API for CPU temperature
    // This would require SMC (System Management Controller) access
    // through a third-party library like SMCKit

    float temperature = 0.0F;

    // This is a placeholder implementation
    LOG_F(INFO, "macOS CPU Temperature: {}°C (not implemented)", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuTemperature function on macOS");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0F);

    // macOS doesn't provide per-core temperatures through a public API
    // This is a placeholder implementation

    LOG_F(INFO, "macOS Per-Core CPU Temperature: not implemented, returning zeros for {} cores", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    LOG_F(INFO, "Starting getCPUModel function on macOS");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    // Use sysctl to get CPU model
    char buffer[1024];
    size_t bufferSize = sizeof(buffer);

    if (sysctlbyname("machdep.cpu.brand_string", buffer, &bufferSize, NULL, 0) == 0) {
        cpuModel = buffer;
    } else {
        // For Apple Silicon, get chip name
        if (sysctlbyname("machdep.cpu.brand", buffer, &bufferSize, NULL, 0) == 0) {
            cpuModel = buffer;

            // Try to get more information for Apple Silicon
            char modelBuffer[256];
            size_t modelBufferSize = sizeof(modelBuffer);

            if (sysctlbyname("hw.model", modelBuffer, &modelBufferSize, NULL, 0) == 0) {
                if (std::string(modelBuffer).find("Mac") != std::string::npos) {
                    cpuModel += " " + std::string(modelBuffer);
                }
            }
        }
    }

    LOG_F(INFO, "macOS CPU Model: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    LOG_F(INFO, "Starting getProcessorIdentifier function on macOS");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier = "Unknown";

    // Get CPU vendor, family, model, and stepping
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
        // For Apple Silicon, use what we can get
        char buffer[256];
        size = sizeof(buffer);

        if (sysctlbyname("machdep.cpu.brand", buffer, &size, NULL, 0) == 0) {
            identifier = buffer;
        }
    }

    LOG_F(INFO, "macOS CPU Identifier: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getProcessorFrequency function on macOS");

    double frequency = 0.0;

    uint64_t freq = 0;
    size_t size = sizeof(freq);

    // Try to get the CPU frequency
    if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
        frequency = static_cast<double>(freq) / 1000000000.0; // Convert Hz to GHz
    } else {
        // Try CPU frequency in MHz (some older Macs)
        unsigned int freqMHz = 0;
        size = sizeof(freqMHz);

        if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
            frequency = static_cast<double>(freq) / 1000000000.0; // Convert Hz to GHz
        } else if (sysctlbyname("hw.cpufrequency_max", &freqMHz, &size, NULL, 0) == 0) {
            frequency = static_cast<double>(freqMHz) / 1000.0; // Convert MHz to GHz
        }
    }

    LOG_F(INFO, "macOS CPU Frequency: {} GHz", frequency);
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMinProcessorFrequency function on macOS");

    double minFreq = 0.0;

    // Try to get the minimum CPU frequency
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency_min", &freq, &size, NULL, 0) == 0) {
        minFreq = static_cast<double>(freq) / 1000000000.0; // Convert Hz to GHz
    }

    // Ensure we have a reasonable minimum value
    if (minFreq <= 0.0) {
        // As a fallback, estimate min as a fraction of current
        double currentFreq = getProcessorFrequency();
        if (currentFreq > 0.0) {
            minFreq = currentFreq * 0.5; // Estimate as half the current frequency
            LOG_F(INFO, "Estimating min CPU frequency as {} GHz", minFreq);
        } else {
            minFreq = 1.0; // Default fallback
        }
    }

    LOG_F(INFO, "macOS CPU Min Frequency: {} GHz", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMaxProcessorFrequency function on macOS");

    double maxFreq = 0.0;

    // Try to get the maximum CPU frequency
    uint64_t freq = 0;
    size_t size = sizeof(freq);

    if (sysctlbyname("hw.cpufrequency_max", &freq, &size, NULL, 0) == 0) {
        maxFreq = static_cast<double>(freq) / 1000000000.0; // Convert Hz to GHz
    } else {
        // Try nominal frequency
        if (sysctlbyname("hw.cpufrequency", &freq, &size, NULL, 0) == 0) {
            maxFreq = static_cast<double>(freq) / 1000000000.0; // Convert Hz to GHz
        }
    }

    // If still no valid max frequency, use current as fallback
    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        LOG_F(INFO, "Using current CPU frequency as max: {} GHz", maxFreq);
    }

    LOG_F(INFO, "macOS CPU Max Frequency: {} GHz", maxFreq);
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    LOG_F(INFO, "Starting getPerCoreFrequencies function on macOS");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    // macOS doesn't provide per-core frequencies through a simple API
    // Use the overall CPU frequency for all cores
    double frequency = getProcessorFrequency();

    for (int i = 0; i < numCores; i++) {
        frequencies[i] = frequency;
    }

    LOG_F(INFO, "macOS Per-Core CPU Frequencies: {} GHz (all cores)", frequency);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalPackages function on macOS");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    // Most Macs have a single physical CPU package
    int numberOfPackages = 1;

    LOG_F(INFO, "macOS Physical CPU Packages: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalCores function on macOS");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    int numberOfCores = 0;

    // Get physical cores
    int physCores = 0;
    size_t size = sizeof(physCores);

    if (sysctlbyname("hw.physicalcpu", &physCores, &size, NULL, 0) == 0) {
        numberOfCores = physCores;
    } else {
        // Fall back to logical cores and account for hyperthreading
        numberOfCores = getNumberOfLogicalCores() / 2;
    }

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    LOG_F(INFO, "macOS Physical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfLogicalCores function on macOS");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    int numberOfCores = 0;

    // Get logical cores
    int logicalCores = 0;
    size_t size = sizeof(logicalCores);

    if (sysctlbyname("hw.logicalcpu", &logicalCores, &size, NULL, 0) == 0) {
        numberOfCores = logicalCores;
    } else {
        // Alternative: hw.ncpu
        if (sysctlbyname("hw.ncpu", &logicalCores, &size, NULL, 0) == 0) {
            numberOfCores = logicalCores;
        } else {
            // Last resort: get available CPUs
            numberOfCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
        }
    }

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    LOG_F(INFO, "macOS Logical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    LOG_F(INFO, "Starting getCacheSizes function on macOS");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Read cache sizes from sysctl
    uint64_t cacheSize = 0;
    size_t size = sizeof(cacheSize);

    // L1 Data Cache
    if (sysctlbyname("hw.l1dcachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1d = static_cast<size_t>(cacheSize);
    }

    // L1 Instruction Cache
    if (sysctlbyname("hw.l1icachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l1i = static_cast<size_t>(cacheSize);
    }

    // L2 Cache
    if (sysctlbyname("hw.l2cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l2 = static_cast<size_t>(cacheSize);
    }

    // L3 Cache
    if (sysctlbyname("hw.l3cachesize", &cacheSize, &size, NULL, 0) == 0) {
        cacheSizes.l3 = static_cast<size_t>(cacheSize);
    }

    // Get line sizes and associativity if available
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

    LOG_F(INFO, "macOS Cache Sizes: L1d={}KB, L1i={}KB, L2={}KB, L3={}KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024, cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    LOG_F(INFO, "Starting getCpuLoadAverage function on macOS");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    double avg[3];
    if (getloadavg(avg, 3) == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
    }

    LOG_F(INFO, "macOS Load Average: {}, {}, {}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    LOG_F(INFO, "Starting getCpuPowerInfo function on macOS");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    // macOS doesn't provide this information through a public API

    LOG_F(INFO, "macOS CPU Power Info: Not implemented");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getCpuFeatureFlags function on macOS");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    // Check for common flags using sysctlbyname
    auto checkFeature = [&flags](const char* name) {
        int supported = 0;
        size_t size = sizeof(supported);

        if (sysctlbyname(name, &supported, &size, NULL, 0) == 0 && supported) {
            // Extract feature name from sysctl name
            std::string featureName = name;
            size_t pos = featureName.rfind('.');
            if (pos != std::string::npos && pos + 1 < featureName.length()) {
                flags.push_back(featureName.substr(pos + 1));
            }
        }
    };

    // Intel CPU features
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

    // ARM features
    checkFeature("hw.optional.neon");
    checkFeature("hw.optional.armv8_1_atomics");
    checkFeature("hw.optional.armv8_2_fhm");
    checkFeature("hw.optional.armv8_2_sha512");
    checkFeature("hw.optional.armv8_2_sha3");
    checkFeature("hw.optional.amx_version");
    checkFeature("hw.optional.ucnormal_mem");

    LOG_F(INFO, "macOS CPU Flags: {} features collected", flags.size());
    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    LOG_F(INFO, "Starting getCpuArchitecture function on macOS");

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
    // Check via uname
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

    LOG_F(INFO, "macOS CPU Architecture: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    LOG_F(INFO, "Starting getCpuVendor function on macOS");

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
        // For Apple Silicon
        CpuArchitecture arch = getCpuArchitecture();
        if (arch == CpuArchitecture::ARM64 || arch == CpuArchitecture::ARM) {
            vendorString = "Apple";
        }
    }

    vendor = getVendorFromString(vendorString);

    LOG_F(INFO, "macOS CPU Vendor: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    LOG_F(INFO, "Starting getCpuSocketType function on macOS");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    // Check architecture to determine socket type
    CpuArchitecture arch = getCpuArchitecture();

    if (arch == CpuArchitecture::ARM64 || arch == CpuArchitecture::ARM) {
        socketType = "Apple SoC";
    } else {
        // For Intel Macs, socket type is generally not available through public APIs
        socketType = "Intel Mac";
    }

    LOG_F(INFO, "macOS CPU Socket Type: {}", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    LOG_F(INFO, "Starting getCpuScalingGovernor function on macOS");

    std::string governor = "Unknown";

    // Get power management mode
    // This is a simplified approach - macOS uses more sophisticated power management

    // Check if we can get power management information
    int perfMode = 0;
    size_t size = sizeof(perfMode);

    if (sysctlbyname("hw.perflevel0.frequency", &perfMode, &size, NULL, 0) == 0) {
        governor = "perflevel";
    } else {
        // Check power source (battery vs. AC)
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

    LOG_F(INFO, "macOS CPU Power Mode: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getPerCoreScalingGovernors function on macOS");

    int numCores = getNumberOfLogicalCores();
    std::string governor = getCpuScalingGovernor();

    // macOS uses a system-wide power management policy
    std::vector<std::string> governors(numCores, governor);

    LOG_F(INFO, "macOS Per-Core Power Modes: {} (same for all cores)", governor);
    return governors;
}

} // namespace atom::system

#endif /* __APPLE__ */
