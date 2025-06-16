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
#include <iterator>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace atom::system::linux {

namespace {
// Cache for frequently accessed values
static thread_local std::unordered_map<
    std::string,
    std::pair<unsigned long long, std::chrono::steady_clock::time_point>>
    cache;
static constexpr auto CACHE_DURATION = std::chrono::milliseconds(100);

/**
 * @brief Parse /proc/meminfo file efficiently
 */
auto parseMemInfo() -> std::unordered_map<std::string, unsigned long long> {
    std::unordered_map<std::string, unsigned long long> values;
    std::ifstream file("/proc/meminfo");

    if (!file.is_open()) {
        spdlog::error("Failed to open /proc/meminfo");
        return values;
    }

    std::string line;
    while (std::getline(file, line)) {
        const auto colonPos = line.find(':');
        if (colonPos == std::string::npos)
            continue;

        const auto key = line.substr(0, colonPos);
        std::istringstream iss(line.substr(colonPos + 1));
        unsigned long long value;
        if (iss >> value) {
            values[key] = value;
        }
    }

    return values;
}

/**
 * @brief Get cached or fresh memory info
 */
auto getCachedMemInfo() -> std::unordered_map<std::string, unsigned long long> {
    const auto now = std::chrono::steady_clock::now();
    const auto it = cache.find("meminfo");

    if (it != cache.end() && (now - it->second.second) < CACHE_DURATION) {
        return parseMemInfo();  // Return cached would require more complex
                                // caching
    }

    auto result = parseMemInfo();
    cache["meminfo"] = {0, now};  // Simplified cache entry
    return result;
}
}  // namespace

auto getMemoryUsage() -> float {
    spdlog::debug("Getting memory usage (Linux)");

    const auto memInfo = parseMemInfo();
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

    const auto memInfo = parseMemInfo();
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
    const auto memInfo = parseMemInfo();
    const auto it = memInfo.find("MemTotal");

    if (it != memInfo.end()) {
        slot.capacity = std::to_string(it->second / 1024);  // Convert kB to MB
        spdlog::debug("Physical memory capacity: {} MB", slot.capacity);
    }

    // Try to read detailed memory information from DMI (requires root)
    try {
        std::ifstream dmiInfo("/sys/devices/system/memory/memory0/dmi");
        if (dmiInfo.is_open()) {
            std::string line;
            while (std::getline(dmiInfo, line)) {
                const auto colonPos = line.find(':');
                if (colonPos == std::string::npos)
                    continue;

                const auto key = line.substr(0, colonPos);
                const auto value = line.substr(colonPos + 1);

                if (key == "Type") {
                    slot.type = value;
                } else if (key == "Speed") {
                    slot.clockSpeed = value;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Could not read detailed memory information: {}",
                     e.what());
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

    const auto memInfo = parseMemInfo();
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

    // Read initial vmstat values
    std::ifstream vmstat("/proc/vmstat");
    std::string line;
    unsigned long pgpgin_before = 0, pgpgout_before = 0;

    while (std::getline(vmstat, line)) {
        if (line.find("pgpgin ") == 0) {
            std::istringstream iss(line.substr(7));
            iss >> pgpgin_before;
        } else if (line.find("pgpgout ") == 0) {
            std::istringstream iss(line.substr(8));
            iss >> pgpgout_before;
        }
    }

    // Wait for measurement interval
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Read values again
    vmstat.clear();
    vmstat.seekg(0, std::ios::beg);
    unsigned long pgpgin_after = 0, pgpgout_after = 0;

    while (std::getline(vmstat, line)) {
        if (line.find("pgpgin ") == 0) {
            std::istringstream iss(line.substr(7));
            iss >> pgpgin_after;
        } else if (line.find("pgpgout ") == 0) {
            std::istringstream iss(line.substr(8));
            iss >> pgpgout_after;
        }
    }

    // Calculate rates (pages are typically 4KB, convert to MB/s)
    const auto pgpgin_persec = pgpgin_after - pgpgin_before;
    const auto pgpgout_persec = pgpgout_after - pgpgout_before;

    perf.readSpeed = pgpgin_persec * 4.0 / 1024.0;
    perf.writeSpeed = pgpgout_persec * 4.0 / 1024.0;

    const auto totalMemoryMB = getTotalMemorySize() / (1024.0 * 1024.0);
    perf.bandwidthUsage =
        (perf.readSpeed + perf.writeSpeed) / totalMemoryMB * 100.0;

    // Measure memory latency with a simple test
    constexpr int TEST_SIZE = 1024 * 1024;
    std::vector<int> testData(TEST_SIZE);

    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < TEST_SIZE; ++i) {
        testData[i] = i;
    }
    const auto end = std::chrono::high_resolution_clock::now();

    perf.latency =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count() /
        static_cast<double>(TEST_SIZE);

    spdlog::debug(
        "Memory performance - Read: {:.2f} MB/s, Write: {:.2f} MB/s, "
        "Bandwidth: {:.1f}%, Latency: {:.2f} ns",
        perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

auto getMemoryLoadPercentage() -> float {
    spdlog::debug("Getting memory load percentage (Linux)");
    const auto memInfo = parseMemInfo();
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

}  // namespace atom::system::linux

#endif  // __linux__
