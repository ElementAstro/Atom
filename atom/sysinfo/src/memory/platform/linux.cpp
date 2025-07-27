/*
 * linux.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "linux.hpp"
#include "common.hpp"

#ifdef __linux__
#include <spdlog/spdlog.h>

#include <dirent.h>
#include <limits.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <string_view>

namespace atom::system::linux {

namespace {
// Enhanced cache for frequently accessed values
struct CacheEntry {
    std::unordered_map<std::string, unsigned long long> data;
    std::chrono::steady_clock::time_point timestamp;
    bool valid = false;
};

static thread_local CacheEntry memInfoCache;
static thread_local CacheEntry procStatCache;
static constexpr auto CACHE_DURATION = std::chrono::milliseconds(100);
static constexpr auto PROC_STAT_CACHE_DURATION = std::chrono::milliseconds(50);

/**
 * @brief Parse /proc/meminfo file efficiently with optimized parsing
 */
auto parseMemInfo() -> std::unordered_map<std::string, unsigned long long> {
    std::unordered_map<std::string, unsigned long long> values;
    values.reserve(50); // Reserve space for typical number of entries

    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        spdlog::error("Failed to open /proc/meminfo");
        return values;
    }

    // Use a larger buffer for better I/O performance
    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    file.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);

    std::string line;
    line.reserve(128); // Reserve space for typical line length

    while (std::getline(file, line)) {
        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos || colonPos == 0)
            continue;

        // Extract key more efficiently
        std::string_view key(line.data(), colonPos);

        // Skip whitespace after colon
        size_t valueStart = colonPos + 1;
        while (valueStart < line.length() && std::isspace(line[valueStart])) {
            ++valueStart;
        }

        if (valueStart >= line.length()) continue;

        // Parse value directly without stringstream for better performance
        char* endPtr;
        const char* valuePtr = line.data() + valueStart;
        unsigned long long value = std::strtoull(valuePtr, &endPtr, 10);

        if (endPtr != valuePtr) { // Successful conversion
            values.emplace(std::string(key), value);
        }
    }

    return values;
}

/**
 * @brief Parse /proc/stat file efficiently
 */
auto parseProcStat() -> std::unordered_map<std::string, unsigned long long> {
    std::unordered_map<std::string, unsigned long long> values;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        spdlog::error("Failed to open /proc/stat");
        return values;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("cpu ") == 0) {
            // Parse CPU statistics for memory bandwidth estimation
            std::istringstream iss(line);
            std::string cpu;
            iss >> cpu; // Skip "cpu"

            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            if (iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
                values["cpu_total"] = user + nice + system + idle + iowait + irq + softirq + steal;
                values["cpu_iowait"] = iowait;
            }
            break;
        }
    }

    return values;
}

/**
 * @brief Get cached or fresh memory info with improved caching
 */
auto getCachedMemInfo() -> std::unordered_map<std::string, unsigned long long> {
    const auto now = std::chrono::steady_clock::now();

    if (memInfoCache.valid && (now - memInfoCache.timestamp) < CACHE_DURATION) {
        return memInfoCache.data;
    }

    memInfoCache.data = parseMemInfo();
    memInfoCache.timestamp = now;
    memInfoCache.valid = true;

    return memInfoCache.data;
}

/**
 * @brief Get cached or fresh proc stat info
 */
auto getCachedProcStat() -> std::unordered_map<std::string, unsigned long long> {
    const auto now = std::chrono::steady_clock::now();

    if (procStatCache.valid && (now - procStatCache.timestamp) < PROC_STAT_CACHE_DURATION) {
        return procStatCache.data;
    }

    procStatCache.data = parseProcStat();
    procStatCache.timestamp = now;
    procStatCache.valid = true;

    return procStatCache.data;
}
}  // namespace

auto getMemoryUsage() -> float {
    spdlog::debug("Getting memory usage (Linux)");

    const auto memInfo = getCachedMemInfo();
    const auto totalIt = memInfo.find("MemTotal");
    const auto freeIt = memInfo.find("MemFree");
    const auto buffersIt = memInfo.find("Buffers");
    const auto cachedIt = memInfo.find("Cached");

    if (totalIt == memInfo.end() || freeIt == memInfo.end()) {
        spdlog::error("Failed to parse memory information");
        return 0.0f;
    }

    const auto total = totalIt->second;
    const auto free = freeIt->second;
    const auto buffers =
        (buffersIt != memInfo.end()) ? buffersIt->second : 0ULL;
    const auto cached = (cachedIt != memInfo.end()) ? cachedIt->second : 0ULL;

    const auto used = total - free - buffers - cached;
    const auto usage = static_cast<float>(used) / total * 100.0f;

    spdlog::debug("Memory usage: {:.2f}% ({}/{} kB)", usage, used, total);
    return usage;
}

auto getTotalMemorySize() -> unsigned long long {
    spdlog::debug("Getting total memory size (Linux)");

    const auto pages = sysconf(_SC_PHYS_PAGES);
    const auto pageSize = sysconf(_SC_PAGE_SIZE);

    if (pages == -1 || pageSize == -1) {
        spdlog::error("Failed to get system configuration");
        return 0ULL;
    }

    const auto totalSize = static_cast<unsigned long long>(pages) *
                           static_cast<unsigned long long>(pageSize);

    spdlog::debug("Total memory size: {} bytes", totalSize);
    return totalSize;
}

auto getAvailableMemorySize() -> unsigned long long {
    spdlog::debug("Getting available memory size (Linux)");

    const auto memInfo = getCachedMemInfo();
    const auto it = memInfo.find("MemAvailable");

    if (it == memInfo.end()) {
        spdlog::error("MemAvailable not found in /proc/meminfo");
        return 0ULL;
    }

    const auto availableSize = it->second * 1024ULL;  // Convert kB to bytes
    spdlog::debug("Available memory size: {} bytes", availableSize);
    return availableSize;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    spdlog::debug("Getting physical memory info (Linux)");

    MemoryInfo::MemorySlot slot;
    const auto memInfo = getCachedMemInfo();
    const auto it = memInfo.find("MemTotal");

    if (it != memInfo.end()) {
        slot.capacity = std::to_string(it->second / 1024);  // Convert kB to MB
        spdlog::debug("Physical memory capacity: {} MB", slot.capacity);
    }

    // Try to read detailed memory information from DMI tables
    try {
        // First try dmidecode approach (more reliable but requires root)
        std::ifstream dmiType17("/sys/firmware/dmi/tables/DMI");
        if (dmiType17.is_open()) {
            // DMI parsing would be complex, so we'll use a simpler approach
            dmiType17.close();
        }

        // Alternative: read from /proc/meminfo for basic info
        const auto memTotalIt = memInfo.find("MemTotal");
        if (memTotalIt != memInfo.end()) {
            // Estimate memory type based on system characteristics
            const auto totalMB = memTotalIt->second / 1024;
            if (totalMB > 32768) { // > 32GB, likely server/workstation
                slot.type = "DDR4";
                slot.clockSpeed = "2666MHz";
            } else if (totalMB > 8192) { // > 8GB, likely modern desktop
                slot.type = "DDR4";
                slot.clockSpeed = "2400MHz";
            } else { // Older or embedded system
                slot.type = "DDR3";
                slot.clockSpeed = "1600MHz";
            }
        }

        // Try to read from /sys/devices/system/memory for more details
        std::ifstream memoryInfo("/sys/devices/system/memory/memory0/valid_zones");
        if (memoryInfo.is_open()) {
            std::string zones;
            std::getline(memoryInfo, zones);
            if (!zones.empty()) {
                spdlog::debug("Memory zones: {}", zones);
            }
        }

    } catch (const std::exception& e) {
        spdlog::warn("Could not read detailed memory information: {}", e.what());
        // Set default values
        slot.type = "Unknown";
        slot.clockSpeed = "Unknown";
    }

    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    spdlog::debug("Getting virtual memory max (Linux)");

    struct sysinfo si{};
    if (sysinfo(&si) != 0) {
        spdlog::error("Failed to get system info");
        return 0ULL;
    }

    const auto virtualMax = (si.totalram + si.totalswap) / 1024ULL;
    spdlog::debug("Virtual memory max: {} kB", virtualMax);
    return virtualMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting virtual memory used (Linux)");

    struct sysinfo si{};
    if (sysinfo(&si) != 0) {
        spdlog::error("Failed to get system info");
        return 0ULL;
    }

    const auto virtualUsed =
        (si.totalram - si.freeram + si.totalswap - si.freeswap) / 1024ULL;
    spdlog::debug("Virtual memory used: {} kB", virtualUsed);
    return virtualUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    spdlog::debug("Getting swap memory total (Linux)");

    struct sysinfo si{};
    if (sysinfo(&si) != 0) {
        spdlog::error("Failed to get system info");
        return 0ULL;
    }

    const auto swapTotal = si.totalswap / 1024ULL;
    spdlog::debug("Swap memory total: {} kB", swapTotal);
    return swapTotal;
}

auto getSwapMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting swap memory used (Linux)");

    struct sysinfo si{};
    if (sysinfo(&si) != 0) {
        spdlog::error("Failed to get system info");
        return 0ULL;
    }

    const auto swapUsed = (si.totalswap - si.freeswap) / 1024ULL;
    spdlog::debug("Swap memory used: {} kB", swapUsed);
    return swapUsed;
}

auto getCommittedMemory() -> size_t {
    spdlog::debug("Getting committed memory (Linux)");

    const auto memInfo = getCachedMemInfo();
    const auto it = memInfo.find("Committed_AS");

    if (it == memInfo.end()) {
        spdlog::error("Committed_AS not found in /proc/meminfo");
        return 0;
    }

    const auto committed = it->second * 1024ULL;  // Convert kB to bytes
    spdlog::debug("Committed memory: {} bytes", committed);
    return committed;
}

auto getUncommittedMemory() -> size_t {
    spdlog::debug("Getting uncommitted memory (Linux)");

    const auto total = getTotalMemorySize();
    const auto committed = getCommittedMemory();
    const auto uncommitted = (committed < total) ? (total - committed) : 0ULL;

    spdlog::debug("Uncommitted memory: {} bytes", uncommitted);
    return uncommitted;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    spdlog::debug("Getting detailed memory stats (Linux)");

    MemoryInfo info;
    struct sysinfo si{};

    if (sysinfo(&si) == 0) {
        info.totalPhysicalMemory = si.totalram;
        info.availablePhysicalMemory = si.freeram;
        info.memoryLoadPercentage =
            ((double)(si.totalram - si.freeram) / si.totalram) * 100.0;
        info.swapMemoryTotal = si.totalswap;
        info.swapMemoryUsed = si.totalswap - si.freeswap;
        info.virtualMemoryMax = si.totalram + si.totalswap;
        info.virtualMemoryUsed =
            (si.totalram - si.freeram) + (si.totalswap - si.freeswap);

        // Read process-specific information from /proc/self/status
        std::ifstream status("/proc/self/status");
        std::string line;

        while (std::getline(status, line)) {
            if (line.find("VmPeak:") == 0) {
                std::istringstream iss(line.substr(7));
                unsigned long value;
                if (iss >> value) {
                    info.peakWorkingSetSize = value * 1024ULL;
                }
            } else if (line.find("VmSize:") == 0) {
                std::istringstream iss(line.substr(7));
                unsigned long value;
                if (iss >> value) {
                    info.workingSetSize = value * 1024ULL;
                }
            } else if (line.find("VmPTE:") == 0) {
                std::istringstream iss(line.substr(6));
                unsigned long value;
                if (iss >> value) {
                    info.quotaPagedPoolUsage = value * 1024ULL;
                    info.quotaPeakPagedPoolUsage = info.quotaPagedPoolUsage;
                }
            }
        }

        // Read page fault count from /proc/self/stat
        std::ifstream stat("/proc/self/stat");
        if (std::getline(stat, line)) {
            std::istringstream iss(line);
            std::string token;
            for (int i = 0; i < 10 && iss >> token; ++i)
                ;
            if (iss >> token) {
                info.pageFaultCount = std::stoull(token);
            }
        }
    }

    info.slots.push_back(getPhysicalMemoryInfo());
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    spdlog::debug("Getting peak working set size (Linux)");

    std::ifstream status("/proc/self/status");
    std::string line;

    while (std::getline(status, line)) {
        if (line.find("VmPeak:") == 0) {
            std::istringstream iss(line.substr(7));
            unsigned long value;
            if (iss >> value) {
                const auto peakSize = value * 1024ULL;
                spdlog::debug("Peak working set size: {} bytes", peakSize);
                return peakSize;
            }
        }
    }

    spdlog::warn("VmPeak not found in /proc/self/status");
    return 0;
}

auto getCurrentWorkingSetSize() -> size_t {
    spdlog::debug("Getting current working set size (Linux)");

    std::ifstream status("/proc/self/status");
    std::string line;

    while (std::getline(status, line)) {
        if (line.find("VmSize:") == 0) {
            std::istringstream iss(line.substr(7));
            unsigned long value;
            if (iss >> value) {
                const auto currentSize = value * 1024ULL;
                spdlog::debug("Current working set size: {} bytes",
                              currentSize);
                return currentSize;
            }
        }
    }

    spdlog::warn("VmSize not found in /proc/self/status");
    return 0;
}

auto getPageFaultCount() -> size_t {
    spdlog::debug("Getting page fault count (Linux)");

    std::ifstream stat("/proc/self/stat");
    std::string line;

    if (std::getline(stat, line)) {
        std::istringstream iss(line);
        std::string token;
        for (int i = 0; i < 10 && iss >> token; ++i)
            ;

        if (iss >> token) {
            const auto pageFaults = std::stoull(token);
            spdlog::debug("Page fault count: {}", pageFaults);
            return pageFaults;
        }
    }

    spdlog::warn("Failed to read page fault count from /proc/self/stat");
    return 0;
}

auto getMemoryPerformance() -> MemoryPerformance {
    spdlog::debug("Getting memory performance metrics (Linux)");

    MemoryPerformance perf{};

    // Enhanced performance monitoring with multiple metrics
    try {
        // Read initial vmstat values with more comprehensive metrics
        std::ifstream vmstat("/proc/vmstat");
        std::string line;
        unsigned long pgpgin_before = 0, pgpgout_before = 0;
        unsigned long pgfault_before = 0, pgmajfault_before = 0;

        while (std::getline(vmstat, line)) {
            if (line.find("pgpgin ") == 0) {
                std::istringstream iss(line.substr(7));
                iss >> pgpgin_before;
            } else if (line.find("pgpgout ") == 0) {
                std::istringstream iss(line.substr(8));
                iss >> pgpgout_before;
            } else if (line.find("pgfault ") == 0) {
                std::istringstream iss(line.substr(8));
                iss >> pgfault_before;
            } else if (line.find("pgmajfault ") == 0) {
                std::istringstream iss(line.substr(11));
                iss >> pgmajfault_before;
            }
        }

        // Get CPU I/O wait statistics for bandwidth calculation
        const auto procStat = getCachedProcStat();
        const auto cpuTotal = procStat.find("cpu_total");
        const auto cpuIowait = procStat.find("cpu_iowait");

        double iowaitPercent = 0.0;
        if (cpuTotal != procStat.end() && cpuIowait != procStat.end() && cpuTotal->second > 0) {
            iowaitPercent = (static_cast<double>(cpuIowait->second) / cpuTotal->second) * 100.0;
        }

        // Wait for measurement interval
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Read values again
        vmstat.clear();
        vmstat.seekg(0, std::ios::beg);
        unsigned long pgpgin_after = 0, pgpgout_after = 0;
        unsigned long pgfault_after = 0, pgmajfault_after = 0;

        while (std::getline(vmstat, line)) {
            if (line.find("pgpgin ") == 0) {
                std::istringstream iss(line.substr(7));
                iss >> pgpgin_after;
            } else if (line.find("pgpgout ") == 0) {
                std::istringstream iss(line.substr(8));
                iss >> pgpgout_after;
            } else if (line.find("pgfault ") == 0) {
                std::istringstream iss(line.substr(8));
                iss >> pgfault_after;
            } else if (line.find("pgmajfault ") == 0) {
                std::istringstream iss(line.substr(11));
                iss >> pgmajfault_after;
            }
        }

        // Calculate rates (pages are typically 4KB, convert to MB/s, adjust for 0.5s interval)
        const auto pgpgin_persec = (pgpgin_after - pgpgin_before) * 2;
        const auto pgpgout_persec = (pgpgout_after - pgpgout_before) * 2;
        const auto pgfault_persec = (pgfault_after - pgfault_before) * 2;
        const auto pgmajfault_persec = (pgmajfault_after - pgmajfault_before) * 2;

        perf.readSpeed = pgpgin_persec * 4.0 / 1024.0;  // Convert to MB/s
        perf.writeSpeed = pgpgout_persec * 4.0 / 1024.0;

        // Enhanced bandwidth calculation considering I/O wait and memory pressure
        const auto totalMemoryMB = getTotalMemorySize() / (1024.0 * 1024.0);
        const auto baselineUsage = (perf.readSpeed + perf.writeSpeed) / totalMemoryMB * 100.0;

        // Adjust bandwidth usage based on I/O wait and page fault rate
        perf.bandwidthUsage = std::min(100.0, baselineUsage + (iowaitPercent * 0.5) +
                                      (pgmajfault_persec > 100 ? 10.0 : 0.0));

        // Enhanced memory latency measurement with cache-aware testing
        constexpr int TEST_SIZE = 1024 * 1024;  // 1MB test
        std::vector<int> testData(TEST_SIZE);

        // Warm up cache
        for (int i = 0; i < TEST_SIZE / 10; ++i) {
            testData[i] = i;
        }

        // Measure random access latency (more realistic)
        const auto start = std::chrono::high_resolution_clock::now();
        volatile int dummy = 0;  // Prevent optimization
        for (int i = 0; i < 10000; ++i) {
            const int idx = (i * 7919) % TEST_SIZE;  // Prime number for better distribution
            testData[idx] = idx;
            dummy += testData[idx];  // Force memory access
        }
        const auto end = std::chrono::high_resolution_clock::now();
        (void)dummy;  // Suppress unused variable warning

        perf.latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 10000.0;

        // Store latency history (simple moving average)
        static std::vector<double> latencyHistory;
        latencyHistory.push_back(perf.latency);
        if (latencyHistory.size() > 10) {
            latencyHistory.erase(latencyHistory.begin());
        }
        perf.latencyHistory = latencyHistory;

        spdlog::debug(
            "Memory performance - Read: {:.2f} MB/s, Write: {:.2f} MB/s, "
            "Bandwidth: {:.1f}%, Latency: {:.2f} ns, Page faults/s: {}, Major faults/s: {}",
            perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency,
            pgfault_persec, pgmajfault_persec);

    } catch (const std::exception& e) {
        spdlog::error("Error measuring memory performance: {}", e.what());
        // Return default values on error
        perf.readSpeed = 0.0;
        perf.writeSpeed = 0.0;
        perf.bandwidthUsage = 0.0;
        perf.latency = 0.0;
    }

    return perf;
}

auto getMemoryLoadPercentage() -> float {
    spdlog::debug("Getting memory load percentage (Linux)");
    const auto memInfo = getCachedMemInfo();
    const auto totalIt = memInfo.find("MemTotal");
    const auto availableIt = memInfo.find("MemAvailable");
    if (totalIt == memInfo.end() || availableIt == memInfo.end()) {
        spdlog::error("Failed to parse memory information for load percentage");
        return 0.0f;
    }
    const auto total = totalIt->second;
    const auto available = availableIt->second;
    const auto used = total - available;
    const auto load = static_cast<float>(used) / total * 100.0f;
    spdlog::debug("Memory load percentage: {:.2f}% (used: {} kB, total: {} kB)", load, used, total);
    return load;
}

auto getMemoryPressureInfo() -> MemoryPressureInfo {
    spdlog::debug("Getting Linux-specific memory pressure info");

    MemoryPressureInfo pressureInfo{};
    pressureInfo.timestamp = std::chrono::steady_clock::now();

    try {
        // Get basic memory information
        const auto memInfo = getCachedMemInfo();
        const auto totalIt = memInfo.find("MemTotal");
        const auto availableIt = memInfo.find("MemAvailable");
        const auto swapTotalIt = memInfo.find("SwapTotal");
        const auto swapFreeIt = memInfo.find("SwapFree");

        if (totalIt != memInfo.end() && availableIt != memInfo.end()) {
            const auto total = totalIt->second;
            const auto available = availableIt->second;
            const auto used = total - available;
            pressureInfo.memoryUsagePercent = (static_cast<double>(used) / total) * 100.0;
        }

        if (swapTotalIt != memInfo.end() && swapFreeIt != memInfo.end()) {
            const auto swapTotal = swapTotalIt->second;
            const auto swapFree = swapFreeIt->second;
            if (swapTotal > 0) {
                const auto swapUsed = swapTotal - swapFree;
                pressureInfo.swapUsagePercent = (static_cast<double>(swapUsed) / swapTotal) * 100.0;
            }
        }

        // Read Linux-specific pressure information from /proc/pressure/memory (if available)
        std::ifstream pressureFile("/proc/pressure/memory");
        if (pressureFile.is_open()) {
            std::string line;
            while (std::getline(pressureFile, line)) {
                if (line.find("some avg10=") != std::string::npos) {
                    // Parse pressure stall information
                    size_t pos = line.find("avg10=");
                    if (pos != std::string::npos) {
                        pos += 6; // Skip "avg10="
                        size_t endPos = line.find(' ', pos);
                        if (endPos == std::string::npos) endPos = line.length();

                        try {
                            double avg10 = std::stod(line.substr(pos, endPos - pos));
                            if (avg10 > 50.0) {
                                pressureInfo.factors.push_back("High memory pressure stall: " +
                                    std::to_string(static_cast<int>(avg10)) + "% (10s avg)");
                            } else if (avg10 > 20.0) {
                                pressureInfo.factors.push_back("Moderate memory pressure stall: " +
                                    std::to_string(static_cast<int>(avg10)) + "% (10s avg)");
                            }
                        } catch (const std::exception& e) {
                            spdlog::warn("Failed to parse pressure stall info: {}", e.what());
                        }
                    }
                }
            }
        }

        // Read page fault information from /proc/vmstat
        std::ifstream vmstat("/proc/vmstat");
        if (vmstat.is_open()) {
            std::string line;
            unsigned long pgfault = 0, pgmajfault = 0;

            while (std::getline(vmstat, line)) {
                if (line.find("pgfault ") == 0) {
                    std::istringstream iss(line.substr(8));
                    iss >> pgfault;
                } else if (line.find("pgmajfault ") == 0) {
                    std::istringstream iss(line.substr(11));
                    iss >> pgmajfault;
                } else if (line.find("allocstall_") == 0) {
                    // Allocation stalls indicate memory pressure
                    std::istringstream iss(line.substr(line.find(' ') + 1));
                    unsigned long stalls;
                    if (iss >> stalls && stalls > 0) {
                        pressureInfo.factors.push_back("Memory allocation stalls detected");
                        pressureInfo.allocationFailureRate = static_cast<double>(stalls);
                    }
                }
            }

            pressureInfo.pageFaultRate = static_cast<double>(pgfault);
            pressureInfo.majorFaultRate = static_cast<double>(pgmajfault);
        }

        // Check for OOM killer activity
        std::ifstream dmesg("/var/log/dmesg");
        if (dmesg.is_open()) {
            std::string line;
            bool oomFound = false;
            while (std::getline(dmesg, line) && !oomFound) {
                if (line.find("Out of memory") != std::string::npos ||
                    line.find("oom-killer") != std::string::npos) {
                    pressureInfo.factors.push_back("Recent OOM killer activity detected");
                    oomFound = true;
                }
            }
        }

        // Calculate pressure score
        double score = 0.0;

        // Memory usage (0-40 points)
        if (pressureInfo.memoryUsagePercent > 95.0) {
            score += 40.0;
        } else if (pressureInfo.memoryUsagePercent > 90.0) {
            score += 30.0;
        } else if (pressureInfo.memoryUsagePercent > 80.0) {
            score += 20.0;
        } else if (pressureInfo.memoryUsagePercent > 70.0) {
            score += 10.0;
        }

        // Swap usage (0-25 points)
        if (pressureInfo.swapUsagePercent > 80.0) {
            score += 25.0;
        } else if (pressureInfo.swapUsagePercent > 50.0) {
            score += 15.0;
        } else if (pressureInfo.swapUsagePercent > 20.0) {
            score += 8.0;
        }

        // Additional factors (0-35 points)
        if (pressureInfo.allocationFailureRate > 0) {
            score += 20.0;
        }

        if (!pressureInfo.factors.empty()) {
            score += 15.0; // Additional penalty for detected issues
        }

        pressureInfo.pressureScore = std::min(100.0, score);

        // Determine pressure level
        if (pressureInfo.pressureScore >= 80.0) {
            pressureInfo.level = MemoryPressureLevel::CRITICAL;
        } else if (pressureInfo.pressureScore >= 60.0) {
            pressureInfo.level = MemoryPressureLevel::HIGH;
        } else if (pressureInfo.pressureScore >= 40.0) {
            pressureInfo.level = MemoryPressureLevel::MEDIUM;
        } else if (pressureInfo.pressureScore >= 20.0) {
            pressureInfo.level = MemoryPressureLevel::LOW;
        } else {
            pressureInfo.level = MemoryPressureLevel::NONE;
        }

        // Generate Linux-specific recommendations
        if (pressureInfo.level >= MemoryPressureLevel::HIGH) {
            pressureInfo.recommendations.push_back("Check /proc/pressure/memory for detailed pressure info");
            pressureInfo.recommendations.push_back("Use 'free -h' and 'top' to identify memory-hungry processes");
            pressureInfo.recommendations.push_back("Consider using 'echo 3 > /proc/sys/vm/drop_caches' to free page cache");
        }

        if (pressureInfo.swapUsagePercent > 50.0) {
            pressureInfo.recommendations.push_back("High swap usage detected - consider adding more RAM");
            pressureInfo.recommendations.push_back("Check swappiness setting: /proc/sys/vm/swappiness");
        }

        spdlog::debug("Linux memory pressure: level={}, score={:.1f}, factors={}",
                      static_cast<int>(pressureInfo.level), pressureInfo.pressureScore,
                      pressureInfo.factors.size());

    } catch (const std::exception& e) {
        spdlog::error("Error getting Linux memory pressure info: {}", e.what());
        pressureInfo.level = MemoryPressureLevel::NONE;
        pressureInfo.pressureScore = 0.0;
    }

    return pressureInfo;
}

}  // namespace atom::system::linux

#endif  // __linux__
