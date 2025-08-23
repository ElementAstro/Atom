/*
 * freebsd.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: System Information Module - CPU FreeBSD Implementation

**************************************************/

#ifdef __FreeBSD__

#include "common.hpp"

namespace atom::system {

// 添加FreeBSD特定函数前向声明
auto getCurrentCpuUsage_FreeBSD() -> float;
auto getPerCoreCpuUsage_FreeBSD() -> std::vector<float>;
auto getCurrentCpuTemperature_FreeBSD() -> float;
auto getPerCoreCpuTemperature_FreeBSD() -> std::vector<float>;
auto getCPUModel_FreeBSD() -> std::string;
// 这里应该添加所有函数的前向声明

auto getCurrentCpuUsage_FreeBSD() -> float {
    LOG_F(INFO, "Starting getCurrentCpuUsage function on FreeBSD");

    static std::mutex mutex;
    static long lastTotal = 0, lastIdle = 0;

    std::unique_lock<std::mutex> lock(mutex);

    float cpuUsage = 0.0f;

    long cp_time[CPUSTATES];
    size_t len = sizeof(cp_time);

    if (sysctlbyname("kern.cp_time", &cp_time, &len, NULL, 0) != -1) {
        long total = cp_time[CP_USER] + cp_time[CP_NICE] + cp_time[CP_SYS] + cp_time[CP_IDLE] + cp_time[CP_INTR];
        long idle = cp_time[CP_IDLE];

        if (lastTotal > 0 && lastIdle > 0) {
            long totalDiff = total - lastTotal;
            long idleDiff = idle - lastIdle;

            if (totalDiff > 0) {
                cpuUsage = 100.0f * (1.0f - (static_cast<float>(idleDiff) / totalDiff));
            }
        }

        lastTotal = total;
        lastIdle = idle;
    }

    // Clamp to 0-100 range
    cpuUsage = std::max(0.0f, std::min(100.0f, cpuUsage));

    LOG_F(INFO, "FreeBSD CPU Usage: {}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuUsage function on FreeBSD");

    static std::mutex mutex;
    static std::vector<long> lastTotals;
    static std::vector<long> lastIdles;

    std::unique_lock<std::mutex> lock(mutex);

    int numCpus = getNumberOfLogicalCores();
    std::vector<float> coreUsages(numCpus, 0.0f);

    // Resize previous vectors if needed
    if (lastTotals.size() < static_cast<size_t>(numCpus)) {
        lastTotals.resize(numCpus, 0);
        lastIdles.resize(numCpus, 0);
    }

    // Get per-CPU statistics
    for (int i = 0; i < numCpus; i++) {
        long cp_time[CPUSTATES];
        size_t len = sizeof(cp_time);

        std::string sysctlName = "kern.cp_times";
        if (sysctlbyname(sysctlName.c_str(), NULL, &len, NULL, 0) != -1) {
            std::vector<long> times(len / sizeof(long));
            if (sysctlbyname(sysctlName.c_str(), times.data(), &len, NULL, 0) != -1) {
                // Each CPU has CPUSTATES values
                int j = i * CPUSTATES;
                long total = times[j + CP_USER] + times[j + CP_NICE] + times[j + CP_SYS] +
                            times[j + CP_IDLE] + times[j + CP_INTR];
                long idle = times[j + CP_IDLE];

                if (lastTotals[i] > 0 && lastIdles[i] > 0) {
                    long totalDiff = total - lastTotals[i];
                    long idleDiff = idle - lastIdles[i];

                    if (totalDiff > 0) {
                        coreUsages[i] = 100.0f * (1.0f - (static_cast<float>(idleDiff) / totalDiff));
                        coreUsages[i] = std::max(0.0f, std::min(100.0f, coreUsages[i]));
                    }
                }

                lastTotals[i] = total;
                lastIdles[i] = idle;
            }
        }
    }

    LOG_F(INFO, "FreeBSD Per-Core CPU Usage collected for {} cores", numCpus);
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    LOG_F(INFO, "Starting getCurrentCpuTemperature function on FreeBSD");

    float temperature = 0.0f;

    // FreeBSD typically uses ACPI or hardware-specific drivers for temperature
    // This would require access to /dev/acpi or similar
    // This is a placeholder implementation

    LOG_F(INFO, "FreeBSD CPU Temperature: {}°C (placeholder)", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    LOG_F(INFO, "Starting getPerCoreCpuTemperature function on FreeBSD");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0f);

    // FreeBSD doesn't have a standard way to get per-core temperatures
    // This is a placeholder implementation

    LOG_F(INFO, "FreeBSD Per-Core CPU Temperature: placeholder values for {} cores", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    LOG_F(INFO, "Starting getCPUModel function on FreeBSD");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    // Try to get model from sysctl
    char buffer[1024];
    size_t len = sizeof(buffer);

    if (sysctlbyname("hw.model", buffer, &len, NULL, 0) != -1) {
        cpuModel = buffer;
    }

    LOG_F(INFO, "FreeBSD CPU Model: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    LOG_F(INFO, "Starting getProcessorIdentifier function on FreeBSD");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier;

    // Combine hw.model with some additional CPU information
    char model[256];
    size_t len = sizeof(model);

    if (sysctlbyname("hw.model", model, &len, NULL, 0) != -1) {
        identifier = model;

        // Try to get additional CPU information (family, level, etc.)
        int family = 0;
        len = sizeof(family);

        if (sysctlbyname("hw.cpu.family", &family, &len, NULL, 0) != -1) {
            identifier += " Family " + std::to_string(family);
        }

        int model_id = 0;
        len = sizeof(model_id);

        if (sysctlbyname("hw.cpu.model", &model_id, &len, NULL, 0) != -1) {
            identifier += " Model " + std::to_string(model_id);
        }

        int stepping = 0;
        len = sizeof(stepping);

        if (sysctlbyname("hw.cpu.stepping", &stepping, &len, NULL, 0) != -1) {
            identifier += " Stepping " + std::to_string(stepping);
        }
    }

    if (identifier.empty()) {
        identifier = "FreeBSD CPU";
    }

    LOG_F(INFO, "FreeBSD CPU Identifier: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getProcessorFrequency function on FreeBSD");

    double frequency = 0.0;

    // Try to get CPU frequency
    int freq = 0;
    size_t len = sizeof(freq);

    if (sysctlbyname("dev.cpu.0.freq", &freq, &len, NULL, 0) != -1) {
        // dev.cpu.0.freq returns frequency in MHz
        frequency = static_cast<double>(freq) / 1000.0; // Convert MHz to GHz
    } else {
        // Alternative: try hw.clockrate
        if (sysctlbyname("hw.clockrate", &freq, &len, NULL, 0) != -1) {
            frequency = static_cast<double>(freq) / 1000.0; // Convert MHz to GHz
        }
    }

    LOG_F(INFO, "FreeBSD CPU Frequency: {} GHz", frequency);
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMinProcessorFrequency function on FreeBSD");

    double minFreq = 0.0;

    // Check if CPU frequency scaling is available
    int freq = 0;
    size_t len = sizeof(freq);

    // Some FreeBSD systems expose this information
    if (sysctlbyname("dev.cpu.0.freq_levels", NULL, &len, NULL, 0) != -1) {
        std::vector<char> freqLevels(len);
        if (sysctlbyname("dev.cpu.0.freq_levels", freqLevels.data(), &len, NULL, 0) != -1) {
            std::string levels(freqLevels.begin(), freqLevels.end());

            // Format is typically "frequency/power frequency/power ..."
            // We want the lowest frequency
            size_t pos = levels.find_last_of(" \t");
            if (pos != std::string::npos && pos + 1 < levels.size()) {
                std::string lastLevel = levels.substr(pos + 1);
                pos = lastLevel.find('/');
                if (pos != std::string::npos) {
                    try {
                        minFreq = std::stod(lastLevel.substr(0, pos)) / 1000.0; // Convert MHz to GHz
                    } catch (const std::exception& e) {
                        LOG_F(WARNING, "Error parsing min frequency: {}", e.what());
                    }
                }
            }
        }
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

    LOG_F(INFO, "FreeBSD CPU Min Frequency: {} GHz", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    LOG_F(INFO, "Starting getMaxProcessorFrequency function on FreeBSD");

    double maxFreq = 0.0;

    // Check if CPU frequency scaling is available
    int freq = 0;
    size_t len = sizeof(freq);

    // Some FreeBSD systems expose this information
    if (sysctlbyname("dev.cpu.0.freq_levels", NULL, &len, NULL, 0) != -1) {
        std::vector<char> freqLevels(len);
        if (sysctlbyname("dev.cpu.0.freq_levels", freqLevels.data(), &len, NULL, 0) != -1) {
            std::string levels(freqLevels.begin(), freqLevels.end());

            // Format is typically "frequency/power frequency/power ..."
            // We want the highest frequency (first one)
            size_t pos = levels.find('/');
            if (pos != std::string::npos) {
                try {
                    maxFreq = std::stod(levels.substr(0, pos)) / 1000.0; // Convert MHz to GHz
                } catch (const std::exception& e) {
                    LOG_F(WARNING, "Error parsing max frequency: {}", e.what());
                }
            }
        }
    }

    // If we couldn't find a max frequency, use current as fallback
    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        LOG_F(INFO, "Using current CPU frequency as max: {} GHz", maxFreq);
    }

    LOG_F(INFO, "FreeBSD CPU Max Frequency: {} GHz", maxFreq);
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    LOG_F(INFO, "Starting getPerCoreFrequencies function on FreeBSD");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    // Try to get per-core frequencies
    for (int i = 0; i < numCores; i++) {
        std::string sysctlName = "dev.cpu." + std::to_string(i) + ".freq";

        int freq = 0;
        size_t len = sizeof(freq);

        if (sysctlbyname(sysctlName.c_str(), &freq, &len, NULL, 0) != -1) {
            // dev.cpu.N.freq returns frequency in MHz
            frequencies[i] = static_cast<double>(freq) / 1000.0; // Convert MHz to GHz
        } else {
            // Fall back to overall CPU frequency
            if (i == 0) {
                frequencies[i] = getProcessorFrequency();
            } else {
                frequencies[i] = frequencies[0];
            }
        }
    }

    LOG_F(INFO, "FreeBSD Per-Core CPU Frequencies collected for {} cores", numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalPackages function on FreeBSD");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    // FreeBSD doesn't provide a direct way to get physical packages
    // Most systems have a single physical package
    int numberOfPackages = 1;

    // Check hw.packages if available
    int packages = 0;
    size_t len = sizeof(packages);

    if (sysctlbyname("hw.packages", &packages, &len, NULL, 0) != -1 && packages > 0) {
        numberOfPackages = packages;
    }

    LOG_F(INFO, "FreeBSD Physical CPU Packages: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfPhysicalCores function on FreeBSD");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    int numberOfCores = 0;

    // Try to get physical cores
    int physCores = 0;
    size_t len = sizeof(physCores);

    // Check hw.ncpu for physical cores
    if (sysctlbyname("hw.ncpu", &physCores, &len, NULL, 0) != -1) {
        numberOfCores = physCores;

        // Check if hyperthreading is enabled
        int hyperThreading = 0;
        len = sizeof(hyperThreading);

        if (sysctlbyname("hw.cpu_hyperthreading", &hyperThreading, &len, NULL, 0) != -1 && hyperThreading) {
            numberOfCores /= 2; // If hyperthreading is enabled, logical cores = 2 * physical cores
        }
    }

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    LOG_F(INFO, "FreeBSD Physical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    LOG_F(INFO, "Starting getNumberOfLogicalCores function on FreeBSD");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    int numberOfCores = 0;

    // Get logical cores using hw.ncpu
    int ncpu = 0;
    size_t len = sizeof(ncpu);

    if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) != -1) {
        numberOfCores = ncpu;
    } else {
        // Fall back to sysconf
        numberOfCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }

    // Ensure at least one core
    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    LOG_F(INFO, "FreeBSD Logical CPU Cores: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    LOG_F(INFO, "Starting getCacheSizes function on FreeBSD");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    // Try to read cache sizes
    int cachesize = 0;
    size_t len = sizeof(cachesize);

    // L1 Data Cache
    if (sysctlbyname("hw.l1dcachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l1d = static_cast<size_t>(cachesize);
    }

    // L1 Instruction Cache
    if (sysctlbyname("hw.l1icachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l1i = static_cast<size_t>(cachesize);
    }

    // L2 Cache
    if (sysctlbyname("hw.l2cachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l2 = static_cast<size_t>(cachesize);
    }

    // L3 Cache
    if (sysctlbyname("hw.l3cachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l3 = static_cast<size_t>(cachesize);
    }

    // Cache line sizes
    int lineSize = 0;

    if (sysctlbyname("hw.cacheline", &lineSize, &len, NULL, 0) != -1) {
        cacheSizes.l1d_line_size = lineSize;
        cacheSizes.l1i_line_size = lineSize;
        cacheSizes.l2_line_size = lineSize;
        cacheSizes.l3_line_size = lineSize;
    }

    LOG_F(INFO, "FreeBSD Cache Sizes: L1d={}KB, L1i={}KB, L2={}KB, L3={}KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024, cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    LOG_F(INFO, "Starting getCpuLoadAverage function on FreeBSD");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    double avg[3];
    if (getloadavg(avg, 3) == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
    }

    LOG_F(INFO, "FreeBSD Load Average: {}, {}, {}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    LOG_F(INFO, "Starting getCpuPowerInfo function on FreeBSD");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    // FreeBSD doesn't provide CPU power information through a simple API

    LOG_F(INFO, "FreeBSD CPU Power Info: Not implemented");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getCpuFeatureFlags function on FreeBSD");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

    // Get CPU feature flags
    char buffer[1024];
    size_t len = sizeof(buffer);

    if (sysctlbyname("hw.cpu.features", buffer, &len, NULL, 0) != -1) {
        std::string flagsStr(buffer);
        std::istringstream ss(flagsStr);
        std::string flag;

        while (ss >> flag) {
            flags.push_back(flag);
        }
    }

    // Additional features for newer CPUs
    if (sysctlbyname("hw.cpu.features.ext", buffer, &len, NULL, 0) != -1) {
        std::string flagsStr(buffer);
        std::istringstream ss(flagsStr);
        std::string flag;

        while (ss >> flag) {
            flags.push_back(flag);
        }
    }

    // Even more features
    if (sysctlbyname("hw.cpu.features.amd", buffer, &len, NULL, 0) != -1) {
        std::string flagsStr(buffer);
        std::istringstream ss(flagsStr);
        std::string flag;

        while (ss >> flag) {
            flags.push_back(flag);
        }
    }

    // Remove duplicates
    std::sort(flags.begin(), flags.end());
    flags.erase(std::unique(flags.begin(), flags.end()), flags.end());

    LOG_F(INFO, "FreeBSD CPU Flags: {} features collected", flags.size());
    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    LOG_F(INFO, "Starting getCpuArchitecture function on FreeBSD");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

    // Get architecture using uname
    struct utsname sysInfo;
    if (uname(&sysInfo) == 0) {
        std::string machine = sysInfo.machine;

        if (machine == "amd64") {
            arch = CpuArchitecture::X86_64;
        } else if (machine == "i386") {
            arch = CpuArchitecture::X86;
        } else if (machine == "arm64") {
            arch = CpuArchitecture::ARM64;
        } else if (machine.find("arm") != std::string::npos) {
            arch = CpuArchitecture::ARM;
        } else if (machine.find("powerpc") != std::string::npos) {
            arch = CpuArchitecture::POWERPC;
        } else if (machine.find("mips") != std::string::npos) {
            arch = CpuArchitecture::MIPS;
        } else if (machine.find("riscv") != std::string::npos) {
            arch = CpuArchitecture::RISC_V;
        }
    }

    LOG_F(INFO, "FreeBSD CPU Architecture: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    LOG_F(INFO, "Starting getCpuVendor function on FreeBSD");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.vendor != CpuVendor::UNKNOWN) {
            return g_cpuInfoCache.vendor;
        }
    }

    CpuVendor vendor = CpuVendor::UNKNOWN;
    std::string vendorString;

    char buffer[64];
    size_t len = sizeof(buffer);

    if (sysctlbyname("hw.cpu.vendor", buffer, &len, NULL, 0) != -1) {
        vendorString = buffer;
    }

    vendor = getVendorFromString(vendorString);

    LOG_F(INFO, "FreeBSD CPU Vendor: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    LOG_F(INFO, "Starting getCpuSocketType function on FreeBSD");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    // FreeBSD doesn't provide socket type directly

    LOG_F(INFO, "FreeBSD CPU Socket Type: {} (placeholder)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    LOG_F(INFO, "Starting getCpuScalingGovernor function on FreeBSD");

    std::string governor = "Unknown";

    // Check if powerd is running
    FILE* pipe = popen("service powerd status", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            if (strstr(buffer, "running") != NULL) {
                governor = "powerd";
            }
        }
        pclose(pipe);
    }

    // Check the current governor setting
    if (governor == "powerd") {
        int economy = 0, performance = 0;
        size_t len = sizeof(economy);

        if (sysctlbyname("hw.acpi.cpu.px_dom0.select", &economy, &len, NULL, 0) != -1) {
            if (economy == 0) {
                governor = "performance";
            } else {
                governor = "economy";
            }
        }
    }

    LOG_F(INFO, "FreeBSD CPU Scaling Governor: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    LOG_F(INFO, "Starting getPerCoreScalingGovernors function on FreeBSD");

    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores);

    // FreeBSD typically uses the same governor for all cores
    std::string governor = getCpuScalingGovernor();

    for (int i = 0; i < numCores; ++i) {
        governors[i] = governor;
    }

    LOG_F(INFO, "FreeBSD Per-Core Scaling Governors: {} (same for all cores)", governor);
    return governors;
}

} // namespace atom::system

#endif /* __FreeBSD__ */
