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
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <sys/resource.h>
#include <sys/cpuset.h>
#include <kvm.h>

namespace atom::system {

// Enhanced FreeBSD-specific caching and utilities
namespace {
    // Thread-safe caching for FreeBSD-specific data
    std::shared_mutex g_freebsd_mutex;
    std::atomic<bool> g_sysctl_initialized{false};

    // Thermal monitoring cache
    struct ThermalCache {
        std::atomic<std::chrono::steady_clock::time_point> last_update{};
        std::atomic<bool> valid{false};
        std::atomic<float> temperature{0.0f};
        std::mutex mutex;
    };

    inline ThermalCache g_thermal_cache;
    constexpr auto THERMAL_CACHE_DURATION = std::chrono::seconds(2);

    // Performance counter cache
    struct PerformanceCache {
        std::atomic<std::chrono::steady_clock::time_point> last_update{};
        std::atomic<bool> valid{false};
        std::vector<long> last_cp_times;
        std::mutex mutex;
    };

    inline PerformanceCache g_perf_cache;
    constexpr auto PERF_CACHE_DURATION = std::chrono::milliseconds(100);
}

/**
 * @brief Enhanced sysctl helper for FreeBSD CPU information
 */
class SysctlHelper {
public:
    template<typename T>
    static bool getValue(const std::string& name, T& value) {
        size_t size = sizeof(T);
        return sysctlbyname(name.c_str(), &value, &size, nullptr, 0) == 0;
    }

    static bool getString(const std::string& name, std::string& value, size_t max_size = 1024) {
        std::vector<char> buffer(max_size);
        size_t size = buffer.size();

        if (sysctlbyname(name.c_str(), buffer.data(), &size, nullptr, 0) == 0) {
            value = std::string(buffer.data(), size > 0 ? size - 1 : 0); // Remove null terminator
            return true;
        }
        return false;
    }

    static bool getStringArray(const std::string& name, std::vector<std::string>& values) {
        std::string str;
        if (getString(name, str)) {
            std::istringstream ss(str);
            std::string item;
            values.clear();

            while (ss >> item) {
                values.push_back(item);
            }
            return true;
        }
        return false;
    }

    template<typename T>
    static std::vector<T> getArray(const std::string& name) {
        std::vector<T> result;
        size_t size = 0;

        // First call to get size
        if (sysctlbyname(name.c_str(), nullptr, &size, nullptr, 0) == 0 && size > 0) {
            size_t count = size / sizeof(T);
            result.resize(count);

            // Second call to get data
            if (sysctlbyname(name.c_str(), result.data(), &size, nullptr, 0) != 0) {
                result.clear();
            }
        }
        return result;
    }
};

// Forward declarations for enhanced FreeBSD functions
auto getCurrentCpuUsage_FreeBSD() -> float;
auto getPerCoreCpuUsage_FreeBSD() -> std::vector<float>;
auto getCurrentCpuTemperature_FreeBSD() -> float;
auto getPerCoreCpuTemperature_FreeBSD() -> std::vector<float>;
auto getCPUModel_FreeBSD() -> std::string;

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
    spdlog::info("Invoking getCurrentCpuTemperature to retrieve CPU temperature on FreeBSD using enhanced thermal monitoring.");

    // Check cache first
    auto now = std::chrono::steady_clock::now();
    auto last_update = g_thermal_cache.last_update.load(std::memory_order_acquire);

    if (g_thermal_cache.valid.load(std::memory_order_acquire) &&
        (now - last_update) < THERMAL_CACHE_DURATION) {
        float cached_temp = g_thermal_cache.temperature.load(std::memory_order_acquire);
        spdlog::debug("Using cached CPU temperature: {:.2f}°C", cached_temp);
        return cached_temp;
    }

    float temperature = 0.0f;

    try {
        // Try multiple thermal sensor sources
        std::vector<std::string> temp_sysctls = {
            "hw.acpi.thermal.tz0.temperature",
            "hw.acpi.thermal.tz1.temperature",
            "dev.cpu.0.temperature",
            "dev.coretemp.0.%desc",
            "dev.amdtemp.0.core0.sensor0",
            "hw.sensors.cpu0.temp0",
            "hw.sensors.acpi_tz0.temp0"
        };

        for (const auto& sysctl_name : temp_sysctls) {
            std::string temp_str;
            if (SysctlHelper::getString(sysctl_name, temp_str)) {
                try {
                    // FreeBSD thermal values are often in Kelvin * 10 or Celsius
                    if (temp_str.find("C") != std::string::npos) {
                        // Extract numeric part before 'C'
                        size_t c_pos = temp_str.find("C");
                        std::string num_part = temp_str.substr(0, c_pos);
                        temperature = std::stof(num_part);
                    } else {
                        double temp_val = std::stod(temp_str);

                        // Check if it's in Kelvin (> 200) and convert to Celsius
                        if (temp_val > 200.0) {
                            if (temp_val > 2000.0) {
                                // Likely Kelvin * 10
                                temperature = static_cast<float>((temp_val / 10.0) - 273.15);
                            } else {
                                // Likely Kelvin
                                temperature = static_cast<float>(temp_val - 273.15);
                            }
                        } else {
                            // Already in Celsius
                            temperature = static_cast<float>(temp_val);
                        }
                    }

                    // Validate temperature range
                    if (temperature > 0.0f && temperature < 150.0f) {
                        break; // Found valid temperature
                    } else {
                        temperature = 0.0f; // Invalid, try next sensor
                    }
                } catch (const std::exception& e) {
                    spdlog::debug("Failed to parse temperature from {}: {}", sysctl_name, e.what());
                    continue;
                }
            }
        }

        // Fallback: Try integer-based thermal sysctls
        if (temperature <= 0.0f) {
            std::vector<std::string> int_temp_sysctls = {
                "hw.acpi.thermal.tz0.temperature",
                "dev.cpu.0.temperature"
            };

            for (const auto& sysctl_name : int_temp_sysctls) {
                int temp_int;
                if (SysctlHelper::getValue(sysctl_name, temp_int)) {
                    if (temp_int > 200) {
                        // Likely Kelvin * 10
                        temperature = static_cast<float>((temp_int / 10.0) - 273.15);
                    } else if (temp_int > 0) {
                        // Likely Celsius
                        temperature = static_cast<float>(temp_int);
                    }

                    if (temperature > 0.0f && temperature < 150.0f) {
                        break;
                    }
                }
            }
        }

        // Cache valid temperature
        if (temperature > 0.0f && temperature < 150.0f) {
            g_thermal_cache.temperature.store(temperature, std::memory_order_release);
            g_thermal_cache.last_update.store(now, std::memory_order_release);
            g_thermal_cache.valid.store(true, std::memory_order_release);
        } else {
            temperature = 0.0f; // Invalid reading
        }

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCurrentCpuTemperature: {}", e.what());
        temperature = 0.0f;
    }

    spdlog::info("CPU temperature on FreeBSD: {:.2f}°C", temperature);
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

    CacheSizes cacheSizes{};
    cacheSizes.l1d = cacheSizes.l1i = cacheSizes.l2 = cacheSizes.l3 = 0;
    cacheSizes.l1d_line_size = cacheSizes.l1i_line_size = cacheSizes.l2_line_size = cacheSizes.l3_line_size = 0;
    cacheSizes.l1d_associativity = cacheSizes.l1i_associativity = cacheSizes.l2_associativity = cacheSizes.l3_associativity = 0;

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

// Enhanced FreeBSD-specific CPU functions for new features

/**
 * @brief Get CPU topology information using FreeBSD APIs
 * @return CpuTopology structure with detailed topology information
 */
auto getCpuTopology() -> CpuTopology {
    spdlog::debug("getCpuTopology_FreeBSD: Reading CPU topology information");

    CpuTopology topology{};

    try {
        // Get basic topology information
        topology.packages = 1; // FreeBSD typically reports single package
        topology.cores_per_package = getNumberOfPhysicalCores();
        topology.threads_per_core = getNumberOfLogicalCores() / getNumberOfPhysicalCores();
        topology.hyperthreading_enabled = (topology.threads_per_core > 1);

        // FreeBSD typically doesn't have NUMA on most systems
        topology.numa_nodes = 1;
        topology.numa_enabled = false;

        // Check for NUMA support
        int numa_domains = 0;
        if (SysctlHelper::getValue("vm.ndomains", numa_domains) && numa_domains > 1) {
            topology.numa_nodes = numa_domains;
            topology.numa_enabled = true;
        }

        // Build CPU mapping
        topology.numa_node_cpus.resize(topology.numa_nodes);
        for (int cpu = 0; cpu < getNumberOfLogicalCores(); ++cpu) {
            topology.numa_node_cpus[0].push_back(cpu);
        }

        // Try to get more detailed topology from cpuset
        cpuset_t cpuset;
        if (cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1, sizeof(cpuset), &cpuset) == 0) {
            // Successfully got CPU affinity information
            spdlog::debug("FreeBSD: Successfully retrieved CPU affinity information");
        }

        spdlog::info("FreeBSD CPU Topology: {} packages, {} NUMA nodes, {} cores/package, {} threads/core",
                     topology.packages, topology.numa_nodes, topology.cores_per_package, topology.threads_per_core);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuTopology: {}", e.what());
    }

    return topology;
}

/**
 * @brief Get CPU security and vulnerability information for FreeBSD
 * @return CpuSecurityInfo structure with security details
 */
auto getCpuSecurityInfo() -> CpuSecurityInfo {
    spdlog::debug("getCpuSecurityInfo_FreeBSD: Reading CPU security information");

    CpuSecurityInfo security{};

    try {
        // Get CPU features that relate to security
        std::vector<std::string> features = getCpuFeatureFlags();

        // Check for common security features
        for (const auto& feature : features) {
            CpuVulnerabilityInfo vuln_info;

            if (feature == "SMEP" || feature == "SMAP") {
                vuln_info.name = feature;
                vuln_info.status = "Mitigation: Hardware feature enabled";
                vuln_info.hardware_mitigation = true;
                vuln_info.software_mitigation = false;
                security.vulnerabilities.push_back(vuln_info);
            } else if (feature == "IBRS" || feature == "STIBP") {
                vuln_info.name = "spectre_v2";
                vuln_info.status = "Mitigation: Hardware feature enabled";
                vuln_info.hardware_mitigation = true;
                vuln_info.software_mitigation = false;
                security.vulnerabilities.push_back(vuln_info);
            }
        }

        // Check FreeBSD-specific security features
        int securelevel = 0;
        if (SysctlHelper::getValue("kern.securelevel", securelevel)) {
            security.secure_boot_enabled = (securelevel > 0);
        }

        // Check for hardware security features
        std::string cpu_vendor;
        if (SysctlHelper::getString("hw.cpu.vendor", cpu_vendor)) {
            if (cpu_vendor.find("Intel") != std::string::npos) {
                // Intel-specific vulnerability checks
                CpuVulnerabilityInfo vuln_info;
                vuln_info.name = "meltdown";
                vuln_info.status = "Vulnerable: Intel CPU";
                vuln_info.hardware_mitigation = false;
                vuln_info.software_mitigation = true;
                vuln_info.mitigation = "FreeBSD kernel mitigations";
                security.vulnerabilities.push_back(vuln_info);
            }
        }

        spdlog::info("FreeBSD CPU Security: Found {} vulnerabilities, Secure level: {}",
                     security.vulnerabilities.size(), securelevel);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuSecurityInfo: {}", e.what());
    }

    return security;
}

/**
 * @brief Get enhanced thermal information for FreeBSD
 * @return ThermalInfo structure with detailed thermal data
 */
auto getCpuThermalInfo() -> ThermalInfo {
    spdlog::debug("getCpuThermalInfo_FreeBSD: Reading enhanced thermal information");

    ThermalInfo thermal{};
    thermal.current_temp = getCurrentCpuTemperature();
    thermal.thermal_throttling_active = false;
    thermal.throttle_state = ThermalThrottleState::NORMAL;
    thermal.thermal_zone = "FreeBSD Thermal Zone";

    try {
        // Try to get thermal zone information
        std::string tz_temp;
        if (SysctlHelper::getString("hw.acpi.thermal.tz0.temperature", tz_temp)) {
            // Parse thermal zone temperature
            if (tz_temp.find("C") != std::string::npos) {
                size_t c_pos = tz_temp.find("C");
                std::string temp_part = tz_temp.substr(0, c_pos);
                thermal.current_temp = std::stof(temp_part);
            }
        }

        // Try to get critical temperature
        std::string crit_temp;
        if (SysctlHelper::getString("hw.acpi.thermal.tz0._CRT", crit_temp)) {
            if (crit_temp.find("C") != std::string::npos) {
                size_t c_pos = crit_temp.find("C");
                std::string temp_part = crit_temp.substr(0, c_pos);
                thermal.critical_temp = std::stof(temp_part);
            }
        } else {
            // Default critical temperature for most CPUs
            thermal.critical_temp = 100.0f;
        }

        // Try to get passive temperature (throttling point)
        std::string passive_temp;
        if (SysctlHelper::getString("hw.acpi.thermal.tz0._PSV", passive_temp)) {
            if (passive_temp.find("C") != std::string::npos) {
                size_t c_pos = passive_temp.find("C");
                std::string temp_part = passive_temp.substr(0, c_pos);
                thermal.throttle_temp = std::stof(temp_part);
            }
        } else {
            // Default throttling temperature
            thermal.throttle_temp = 85.0f;
        }

        // Calculate thermal margin
        if (thermal.critical_temp > 0 && thermal.current_temp > 0) {
            thermal.thermal_margin = thermal.critical_temp - thermal.current_temp;
        }

        // Determine throttle state based on temperature
        if (thermal.current_temp > thermal.critical_temp * 0.95f) {
            thermal.throttle_state = ThermalThrottleState::CRITICAL_THROTTLE;
            thermal.thermal_throttling_active = true;
        } else if (thermal.current_temp > thermal.throttle_temp) {
            thermal.throttle_state = ThermalThrottleState::HEAVY_THROTTLE;
            thermal.thermal_throttling_active = true;
        } else if (thermal.current_temp > thermal.throttle_temp * 0.9f) {
            thermal.throttle_state = ThermalThrottleState::MODERATE_THROTTLE;
        } else if (thermal.current_temp > thermal.throttle_temp * 0.8f) {
            thermal.throttle_state = ThermalThrottleState::LIGHT_THROTTLE;
        }

        spdlog::info("FreeBSD CPU Thermal: Current: {:.1f}°C, Critical: {:.1f}°C, Margin: {:.1f}°C, Throttling: {}",
                     thermal.current_temp, thermal.critical_temp, thermal.thermal_margin, thermal.thermal_throttling_active);

    } catch (const std::exception& e) {
        spdlog::error("Exception in getCpuThermalInfo: {}", e.what());
    }

    return thermal;
}

/**
 * @brief Check if CPU is currently throttling on FreeBSD
 * @return True if CPU is throttling due to thermal or power limits
 */
auto isCpuThrottling() -> bool {
    try {
        auto thermal = getCpuThermalInfo();
        return thermal.thermal_throttling_active;
    } catch (const std::exception& e) {
        spdlog::error("Exception in isCpuThrottling: {}", e.what());
        return false;
    }
}

} // namespace atom::system

#endif /* __FreeBSD__ */
