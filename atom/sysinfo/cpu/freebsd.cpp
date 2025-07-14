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
    spdlog::info("Invoking getCurrentCpuUsage_FreeBSD to retrieve overall CPU usage on FreeBSD.");

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

    cpuUsage = std::max(0.0f, std::min(100.0f, cpuUsage));

    spdlog::info("Overall CPU usage on FreeBSD: {:.2f}%", cpuUsage);
    return cpuUsage;
}

auto getPerCoreCpuUsage() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuUsage to retrieve per-core CPU usage statistics on FreeBSD.");

    static std::mutex mutex;
    static std::vector<long> lastTotals;
    static std::vector<long> lastIdles;

    std::unique_lock<std::mutex> lock(mutex);

    int numCpus = getNumberOfLogicalCores();
    std::vector<float> coreUsages(numCpus, 0.0f);

    if (lastTotals.size() < static_cast<size_t>(numCpus)) {
        lastTotals.resize(numCpus, 0);
        lastIdles.resize(numCpus, 0);
    }

    for (int i = 0; i < numCpus; i++) {
        long cp_time[CPUSTATES];
        size_t len = sizeof(cp_time);

        std::string sysctlName = "kern.cp_times";
        if (sysctlbyname(sysctlName.c_str(), NULL, &len, NULL, 0) != -1) {
            std::vector<long> times(len / sizeof(long));
            if (sysctlbyname(sysctlName.c_str(), times.data(), &len, NULL, 0) != -1) {
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

    spdlog::info("Collected per-core CPU usage for {} logical cores on FreeBSD.", numCpus);
    return coreUsages;
}

auto getCurrentCpuTemperature() -> float {
    spdlog::info("Invoking getCurrentCpuTemperature to retrieve CPU temperature on FreeBSD (placeholder implementation).");

    float temperature = 0.0f;

    spdlog::info("CPU temperature on FreeBSD: {:.2f}°C (placeholder value)", temperature);
    return temperature;
}

auto getPerCoreCpuTemperature() -> std::vector<float> {
    spdlog::info("Invoking getPerCoreCpuTemperature to retrieve per-core CPU temperatures on FreeBSD (placeholder implementation).");

    int numCores = getNumberOfLogicalCores();
    std::vector<float> temperatures(numCores, 0.0f);

    spdlog::info("Per-core CPU temperatures on FreeBSD: placeholder values for {} logical cores.", numCores);
    return temperatures;
}

auto getCPUModel() -> std::string {
    spdlog::info("Invoking getCPUModel to retrieve the CPU model string on FreeBSD.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.model.empty()) {
        return g_cpuInfoCache.model;
    }

    std::string cpuModel = "Unknown";

    char buffer[1024];
    size_t len = sizeof(buffer);

    if (sysctlbyname("hw.model", buffer, &len, NULL, 0) != -1) {
        cpuModel = buffer;
    }

    spdlog::info("Detected CPU model on FreeBSD: {}", cpuModel);
    return cpuModel;
}

auto getProcessorIdentifier() -> std::string {
    spdlog::info("Invoking getProcessorIdentifier to retrieve the processor identifier on FreeBSD.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.identifier.empty()) {
        return g_cpuInfoCache.identifier;
    }

    std::string identifier;

    char model[256];
    size_t len = sizeof(model);

    if (sysctlbyname("hw.model", model, &len, NULL, 0) != -1) {
        identifier = model;

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

    spdlog::info("Constructed processor identifier on FreeBSD: {}", identifier);
    return identifier;
}

auto getProcessorFrequency() -> double {
    spdlog::info("Invoking getProcessorFrequency to retrieve the current CPU frequency on FreeBSD.");

    double frequency = 0.0;

    int freq = 0;
    size_t len = sizeof(freq);

    if (sysctlbyname("dev.cpu.0.freq", &freq, &len, NULL, 0) != -1) {
        frequency = static_cast<double>(freq) / 1000.0;
    } else {
        if (sysctlbyname("hw.clockrate", &freq, &len, NULL, 0) != -1) {
            frequency = static_cast<double>(freq) / 1000.0;
        }
    }

    spdlog::info("Current CPU frequency on FreeBSD: {:.3f} GHz", frequency);
    return frequency;
}

auto getMinProcessorFrequency() -> double {
    spdlog::info("Invoking getMinProcessorFrequency to retrieve the minimum CPU frequency on FreeBSD.");

    double minFreq = 0.0;

    int freq = 0;
    size_t len = sizeof(freq);

    if (sysctlbyname("dev.cpu.0.freq_levels", NULL, &len, NULL, 0) != -1) {
        std::vector<char> freqLevels(len);
        if (sysctlbyname("dev.cpu.0.freq_levels", freqLevels.data(), &len, NULL, 0) != -1) {
            std::string levels(freqLevels.begin(), freqLevels.end());

            size_t pos = levels.find_last_of(" \t");
            if (pos != std::string::npos && pos + 1 < levels.size()) {
                std::string lastLevel = levels.substr(pos + 1);
                pos = lastLevel.find('/');
                if (pos != std::string::npos) {
                    try {
                        minFreq = std::stod(lastLevel.substr(0, pos)) / 1000.0;
                    } catch (const std::exception& e) {
                        spdlog::warn("Failed to parse minimum CPU frequency from sysctl output: {}", e.what());
                    }
                }
            }
        }
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

    spdlog::info("Minimum CPU frequency on FreeBSD: {:.3f} GHz", minFreq);
    return minFreq;
}

auto getMaxProcessorFrequency() -> double {
    spdlog::info("Invoking getMaxProcessorFrequency to retrieve the maximum CPU frequency on FreeBSD.");

    double maxFreq = 0.0;

    int freq = 0;
    size_t len = sizeof(freq);

    if (sysctlbyname("dev.cpu.0.freq_levels", NULL, &len, NULL, 0) != -1) {
        std::vector<char> freqLevels(len);
        if (sysctlbyname("dev.cpu.0.freq_levels", freqLevels.data(), &len, NULL, 0) != -1) {
            std::string levels(freqLevels.begin(), freqLevels.end());

            size_t pos = levels.find('/');
            if (pos != std::string::npos) {
                try {
                    maxFreq = std::stod(levels.substr(0, pos)) / 1000.0;
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to parse maximum CPU frequency from sysctl output: {}", e.what());
                }
            }
        }
    }

    if (maxFreq <= 0.0) {
        maxFreq = getProcessorFrequency();
        spdlog::info("Using current CPU frequency as maximum: {:.3f} GHz", maxFreq);
    }

    spdlog::info("Maximum CPU frequency on FreeBSD: {:.3f} GHz", maxFreq);
    return maxFreq;
}

auto getPerCoreFrequencies() -> std::vector<double> {
    spdlog::info("Invoking getPerCoreFrequencies to retrieve per-core CPU frequencies on FreeBSD.");

    int numCores = getNumberOfLogicalCores();
    std::vector<double> frequencies(numCores, 0.0);

    for (int i = 0; i < numCores; i++) {
        std::string sysctlName = "dev.cpu." + std::to_string(i) + ".freq";

        int freq = 0;
        size_t len = sizeof(freq);

        if (sysctlbyname(sysctlName.c_str(), &freq, &len, NULL, 0) != -1) {
            frequencies[i] = static_cast<double>(freq) / 1000.0;
        } else {
            if (i == 0) {
                frequencies[i] = getProcessorFrequency();
            } else {
                frequencies[i] = frequencies[0];
            }
        }
    }

    spdlog::info("Collected per-core CPU frequencies for {} logical cores on FreeBSD.", numCores);
    return frequencies;
}

auto getNumberOfPhysicalPackages() -> int {
    spdlog::info("Invoking getNumberOfPhysicalPackages to determine the number of physical CPU packages on FreeBSD.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalPackages > 0) {
        return g_cpuInfoCache.numPhysicalPackages;
    }

    int numberOfPackages = 1;

    int packages = 0;
    size_t len = sizeof(packages);

    if (sysctlbyname("hw.packages", &packages, &len, NULL, 0) != -1 && packages > 0) {
        numberOfPackages = packages;
    }

    spdlog::info("Number of physical CPU packages detected on FreeBSD: {}", numberOfPackages);
    return numberOfPackages;
}

auto getNumberOfPhysicalCores() -> int {
    spdlog::info("Invoking getNumberOfPhysicalCores to determine the number of physical CPU cores on FreeBSD.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numPhysicalCores > 0) {
        return g_cpuInfoCache.numPhysicalCores;
    }

    int numberOfCores = 0;

    int physCores = 0;
    size_t len = sizeof(physCores);

    if (sysctlbyname("hw.ncpu", &physCores, &len, NULL, 0) != -1) {
        numberOfCores = physCores;

        int hyperThreading = 0;
        len = sizeof(hyperThreading);

        if (sysctlbyname("hw.cpu_hyperthreading", &hyperThreading, &len, NULL, 0) != -1 && hyperThreading) {
            numberOfCores /= 2;
        }
    }

    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    spdlog::info("Number of physical CPU cores detected on FreeBSD: {}", numberOfCores);
    return numberOfCores;
}

auto getNumberOfLogicalCores() -> int {
    spdlog::info("Invoking getNumberOfLogicalCores to determine the number of logical CPU cores on FreeBSD.");

    if (!needsCacheRefresh() && g_cpuInfoCache.numLogicalCores > 0) {
        return g_cpuInfoCache.numLogicalCores;
    }

    int numberOfCores = 0;

    int ncpu = 0;
    size_t len = sizeof(ncpu);

    if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) != -1) {
        numberOfCores = ncpu;
    } else {
        numberOfCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }

    if (numberOfCores <= 0) {
        numberOfCores = 1;
    }

    spdlog::info("Number of logical CPU cores detected on FreeBSD: {}", numberOfCores);
    return numberOfCores;
}

auto getCacheSizes() -> CacheSizes {
    spdlog::info("Invoking getCacheSizes to retrieve CPU cache sizes on FreeBSD.");

    if (!needsCacheRefresh() &&
        (g_cpuInfoCache.caches.l1d > 0 || g_cpuInfoCache.caches.l2 > 0 ||
         g_cpuInfoCache.caches.l3 > 0)) {
        return g_cpuInfoCache.caches;
    }

    CacheSizes cacheSizes{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int cachesize = 0;
    size_t len = sizeof(cachesize);

    if (sysctlbyname("hw.l1dcachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l1d = static_cast<size_t>(cachesize);
    }

    if (sysctlbyname("hw.l1icachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l1i = static_cast<size_t>(cachesize);
    }

    if (sysctlbyname("hw.l2cachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l2 = static_cast<size_t>(cachesize);
    }

    if (sysctlbyname("hw.l3cachesize", &cachesize, &len, NULL, 0) != -1) {
        cacheSizes.l3 = static_cast<size_t>(cachesize);
    }

    int lineSize = 0;

    if (sysctlbyname("hw.cacheline", &lineSize, &len, NULL, 0) != -1) {
        cacheSizes.l1d_line_size = lineSize;
        cacheSizes.l1i_line_size = lineSize;
        cacheSizes.l2_line_size = lineSize;
        cacheSizes.l3_line_size = lineSize;
    }

    spdlog::info("Cache sizes on FreeBSD: L1d={} KB, L1i={} KB, L2={} KB, L3={} KB",
          cacheSizes.l1d / 1024, cacheSizes.l1i / 1024, cacheSizes.l2 / 1024, cacheSizes.l3 / 1024);

    return cacheSizes;
}

auto getCpuLoadAverage() -> LoadAverage {
    spdlog::info("Invoking getCpuLoadAverage to retrieve system load averages on FreeBSD.");

    LoadAverage loadAvg{0.0, 0.0, 0.0};

    double avg[3];
    if (getloadavg(avg, 3) == 3) {
        loadAvg.oneMinute = avg[0];
        loadAvg.fiveMinutes = avg[1];
        loadAvg.fifteenMinutes = avg[2];
    }

    spdlog::info("System load averages on FreeBSD: 1min={:.2f}, 5min={:.2f}, 15min={:.2f}",
          loadAvg.oneMinute, loadAvg.fiveMinutes, loadAvg.fifteenMinutes);

    return loadAvg;
}

auto getCpuPowerInfo() -> CpuPowerInfo {
    spdlog::info("Invoking getCpuPowerInfo to retrieve CPU power information on FreeBSD (not implemented).");

    CpuPowerInfo powerInfo{0.0, 0.0, 0.0};

    spdlog::info("CPU power information retrieval is not implemented for FreeBSD.");
    return powerInfo;
}

auto getCpuFeatureFlags() -> std::vector<std::string> {
    spdlog::info("Invoking getCpuFeatureFlags to retrieve CPU feature flags on FreeBSD.");

    if (!needsCacheRefresh() && !g_cpuInfoCache.flags.empty()) {
        return g_cpuInfoCache.flags;
    }

    std::vector<std::string> flags;

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

    if (sysctlbyname("hw.cpu.features.ext", buffer, &len, NULL, 0) != -1) {
        std::string flagsStr(buffer);
        std::istringstream ss(flagsStr);
        std::string flag;

        while (ss >> flag) {
            flags.push_back(flag);
        }
    }

    if (sysctlbyname("hw.cpu.features.amd", buffer, &len, NULL, 0) != -1) {
        std::string flagsStr(buffer);
        std::istringstream ss(flagsStr);
        std::string flag;

        while (ss >> flag) {
            flags.push_back(flag);
        }
    }

    std::sort(flags.begin(), flags.end());
    flags.erase(std::unique(flags.begin(), flags.end()), flags.end());

    spdlog::info("Collected {} unique CPU feature flags on FreeBSD.", flags.size());
    return flags;
}

auto getCpuArchitecture() -> CpuArchitecture {
    spdlog::info("Invoking getCpuArchitecture to determine the CPU architecture on FreeBSD.");

    if (!needsCacheRefresh()) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheInitialized && g_cpuInfoCache.architecture != CpuArchitecture::UNKNOWN) {
            return g_cpuInfoCache.architecture;
        }
    }

    CpuArchitecture arch = CpuArchitecture::UNKNOWN;

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

    spdlog::info("Detected CPU architecture on FreeBSD: {}", cpuArchitectureToString(arch));
    return arch;
}

auto getCpuVendor() -> CpuVendor {
    spdlog::info("Invoking getCpuVendor to determine the CPU vendor on FreeBSD.");

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

    spdlog::info("Detected CPU vendor on FreeBSD: {} ({})", vendorString, cpuVendorToString(vendor));
    return vendor;
}

auto getCpuSocketType() -> std::string {
    spdlog::info("Invoking getCpuSocketType to retrieve the CPU socket type on FreeBSD (placeholder implementation).");

    if (!needsCacheRefresh() && !g_cpuInfoCache.socketType.empty()) {
        return g_cpuInfoCache.socketType;
    }

    std::string socketType = "Unknown";

    spdlog::info("CPU socket type on FreeBSD: {} (no direct method available, placeholder value)", socketType);
    return socketType;
}

auto getCpuScalingGovernor() -> std::string {
    spdlog::info("Invoking getCpuScalingGovernor to retrieve the CPU scaling governor on FreeBSD.");

    std::string governor = "Unknown";

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

    spdlog::info("CPU scaling governor on FreeBSD: {}", governor);
    return governor;
}

auto getPerCoreScalingGovernors() -> std::vector<std::string> {
    spdlog::info("Invoking getPerCoreScalingGovernors to retrieve per-core CPU scaling governors on FreeBSD.");

    int numCores = getNumberOfLogicalCores();
    std::vector<std::string> governors(numCores);

    std::string governor = getCpuScalingGovernor();

    for (int i = 0; i < numCores; ++i) {
        governors[i] = governor;
    }

    spdlog::info("Assigned CPU scaling governor '{}' to all {} logical cores on FreeBSD.", governor, numCores);
    return governors;
}

} // namespace atom::system

#endif /* __FreeBSD__ */
