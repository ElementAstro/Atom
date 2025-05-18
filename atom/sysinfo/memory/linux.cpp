/*
 * linux.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory Linux Implementation

**************************************************/

#include "linux.hpp"
#include "common.hpp"

#ifdef __linux__
#include "atom/log/loguru.hpp"

#include <dirent.h>
#include <limits.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <thread>
#include <chrono>

namespace atom::system {
namespace linux {

auto getMemoryUsage() -> float {
    LOG_F(INFO, "Starting getMemoryUsage function (Linux)");
    float memoryUsage = 0.0;

    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        LOG_F(ERROR, "GetMemoryUsage error: open /proc/meminfo error");
        return memoryUsage;
    }
    
    std::string line;
    unsigned long totalMemory = 0;
    unsigned long freeMemory = 0;
    unsigned long bufferMemory = 0;
    unsigned long cacheMemory = 0;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string name;
        unsigned long value;

        if (iss >> name >> value) {
            if (name == "MemTotal:") {
                totalMemory = value;
            } else if (name == "MemFree:") {
                freeMemory = value;
            } else if (name == "Buffers:") {
                bufferMemory = value;
            } else if (name == "Cached:") {
                cacheMemory = value;
            }
        }
    }

    unsigned long usedMemory = totalMemory - freeMemory - bufferMemory - cacheMemory;
    memoryUsage = static_cast<float>(usedMemory) / totalMemory * 100.0;
    LOG_F(INFO,
          "Total Memory: {} kB, Free Memory: {} kB, Buffer Memory: {} kB, "
          "Cache Memory: {} kB, Memory Usage: {}%",
          totalMemory, freeMemory, bufferMemory, cacheMemory, memoryUsage);

    LOG_F(INFO, "Finished getMemoryUsage function (Linux)");
    return memoryUsage;
}

auto getTotalMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getTotalMemorySize function (Linux)");
    unsigned long long totalMemorySize = 0;

    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    totalMemorySize = static_cast<unsigned long long>(pages) *
                      static_cast<unsigned long long>(pageSize);
    LOG_F(INFO, "Total Memory Size: {} bytes (Linux)", totalMemorySize);

    LOG_F(INFO, "Finished getTotalMemorySize function (Linux)");
    return totalMemorySize;
}

auto getAvailableMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getAvailableMemorySize function (Linux)");
    unsigned long long availableMemorySize = 0;

    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/meminfo");
        return -1;
    }

    std::string line;
    std::regex memAvailableRegex(R"(MemAvailable:\s+(\d+)\s+kB)");
    bool found = false;

    while (std::getline(meminfo, line)) {
        std::smatch match;
        if (std::regex_search(line, match, memAvailableRegex)) {
            availableMemorySize = std::stoull(match[1]) * 1024; // Convert kB to bytes
            found = true;
            break;
        }
    }

    if (!found) {
        LOG_F(ERROR, "GetAvailableMemorySize error: parse error");
        return -1;
    }

    LOG_F(INFO, "Available Memory Size: {} bytes (Linux)", availableMemorySize);
    LOG_F(INFO, "Finished getAvailableMemorySize function (Linux)");
    return availableMemorySize;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    LOG_F(INFO, "Starting getPhysicalMemoryInfo function (Linux)");
    MemoryInfo::MemorySlot slot;

    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.substr(0, 10) == "MemTotal: ") {
            std::istringstream iss(line.substr(10));
            unsigned long total;
            iss >> total;
            slot.capacity = std::to_string(total / 1024); // Convert kB to MB
            LOG_F(INFO, "Physical Memory Capacity: {} MB", slot.capacity);
            break;
        }
    }

    // 尝试从DMI读取更详细的内存信息（需要root权限）
    try {
        std::ifstream dmiInfo("/sys/devices/system/memory/memory0/dmi");
        if (dmiInfo.is_open()) {
            std::string dmiLine;
            while (std::getline(dmiInfo, dmiLine)) {
                if (dmiLine.find("Type:") != std::string::npos) {
                    slot.type = dmiLine.substr(dmiLine.find(":") + 1);
                } else if (dmiLine.find("Speed:") != std::string::npos) {
                    slot.clockSpeed = dmiLine.substr(dmiLine.find(":") + 1);
                }
            }
        }
    } catch (...) {
        LOG_F(WARNING, "Could not read detailed memory information from DMI (may require root)");
    }

    LOG_F(INFO, "Finished getPhysicalMemoryInfo function (Linux)");
    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryMax function (Linux)");
    unsigned long long virtualMemoryMax = 0;

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        virtualMemoryMax = (si.totalram + si.totalswap) / 1024;
        LOG_F(INFO, "Virtual Memory Max: {} kB (Linux)", virtualMemoryMax);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryMax error: sysinfo error");
    }

    LOG_F(INFO, "Finished getVirtualMemoryMax function (Linux)");
    return virtualMemoryMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryUsed function (Linux)");
    unsigned long long virtualMemoryUsed = 0;

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        virtualMemoryUsed =
            (si.totalram - si.freeram + si.totalswap - si.freeswap) / 1024;
        LOG_F(INFO, "Virtual Memory Used: {} kB (Linux)", virtualMemoryUsed);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryUsed error: sysinfo error");
    }

    LOG_F(INFO, "Finished getVirtualMemoryUsed function (Linux)");
    return virtualMemoryUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryTotal function (Linux)");
    unsigned long long swapMemoryTotal = 0;

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        swapMemoryTotal = si.totalswap / 1024;
        LOG_F(INFO, "Swap Memory Total: {} kB (Linux)", swapMemoryTotal);
    } else {
        LOG_F(ERROR, "GetSwapMemoryTotal error: sysinfo error");
    }

    LOG_F(INFO, "Finished getSwapMemoryTotal function (Linux)");
    return swapMemoryTotal;
}

auto getSwapMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryUsed function (Linux)");
    unsigned long long swapMemoryUsed = 0;

    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        swapMemoryUsed = (si.totalswap - si.freeswap) / 1024;
        LOG_F(INFO, "Swap Memory Used: {} kB (Linux)", swapMemoryUsed);
    } else {
        LOG_F(ERROR, "GetSwapMemoryUsed error: sysinfo error");
    }

    LOG_F(INFO, "Finished getSwapMemoryUsed function (Linux)");
    return swapMemoryUsed;
}

auto getCommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getCommittedMemory function (Linux)");
    
    std::ifstream memInfoFile("/proc/meminfo");
    if (!memInfoFile.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/meminfo");
        return 0;
    }

    std::string line;
    std::regex commitRegex(R"(Committed_AS:\s+(\d+)\s+kB)");
    size_t committedMemory = 0;

    while (std::getline(memInfoFile, line)) {
        std::smatch match;
        if (std::regex_search(line, match, commitRegex)) {
            committedMemory = std::stoull(match[1]) * 1024; // Convert kB to bytes
            break;
        }
    }
    
    LOG_F(INFO, "Committed Memory: {} bytes (Linux)", committedMemory);
    LOG_F(INFO, "Finished getCommittedMemory function (Linux)");
    return committedMemory;
}

auto getUncommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getUncommittedMemory function (Linux)");
    
    // 在Linux中，未提交内存可以通过总内存减去已提交内存来计算
    size_t totalMemory = getTotalMemorySize();
    size_t committedMemory = getCommittedMemory();
    size_t uncommittedMemory = (committedMemory < totalMemory) ? 
                               (totalMemory - committedMemory) : 0;
    
    LOG_F(INFO, "Uncommitted Memory: {} bytes (Linux)", uncommittedMemory);
    LOG_F(INFO, "Finished getUncommittedMemory function (Linux)");
    return uncommittedMemory;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    LOG_F(INFO, "Starting getDetailedMemoryStats function (Linux)");
    MemoryInfo info;

    struct sysinfo si;
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

        // 读取 /proc/self/status 获取进程相关信息
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmPeak:") != std::string::npos) {
                std::istringstream iss(line.substr(line.find(":") + 1));
                unsigned long value;
                iss >> value;
                info.peakWorkingSetSize = value * 1024; // Convert kB to bytes
            } else if (line.find("VmSize:") != std::string::npos) {
                std::istringstream iss(line.substr(line.find(":") + 1));
                unsigned long value;
                iss >> value;
                info.workingSetSize = value * 1024; // Convert kB to bytes
            } else if (line.find("VmPTE:") != std::string::npos) {
                std::istringstream iss(line.substr(line.find(":") + 1));
                unsigned long value;
                iss >> value;
                info.quotaPagedPoolUsage = value * 1024; // Convert kB to bytes
                info.quotaPeakPagedPoolUsage = info.quotaPagedPoolUsage; // No peak value available
            }
        }

        // 读取 /proc/self/stat 获取页面错误数
        std::ifstream stat("/proc/self/stat");
        std::string statLine;
        if (std::getline(stat, statLine)) {
            std::istringstream iss(statLine);
            std::string token;
            int count = 0;
            while (iss >> token && count < 10) {
                count++;
            }
            if (count == 10 && iss >> token) {
                info.pageFaultCount = std::stoull(token);
            }
        }
    }

    // 获取物理内存插槽信息
    MemoryInfo::MemorySlot slot = getPhysicalMemoryInfo();
    info.slots.push_back(slot);

    LOG_F(INFO, "Finished getDetailedMemoryStats function (Linux)");
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getPeakWorkingSetSize function (Linux)");
    size_t peakSize = 0;

    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmPeak:") != std::string::npos) {
            std::istringstream iss(line.substr(line.find(":") + 1));
            unsigned long value;
            iss >> value;
            peakSize = value * 1024; // Convert kB to bytes
            break;
        }
    }

    LOG_F(INFO, "Peak working set size: {} bytes (Linux)", peakSize);
    return peakSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getCurrentWorkingSetSize function (Linux)");
    size_t currentSize = 0;

    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmSize:") != std::string::npos) {
            std::istringstream iss(line.substr(line.find(":") + 1));
            unsigned long value;
            iss >> value;
            currentSize = value * 1024; // Convert kB to bytes
            break;
        }
    }

    LOG_F(INFO, "Current working set size: {} bytes (Linux)", currentSize);
    return currentSize;
}

auto getPageFaultCount() -> size_t {
    LOG_F(INFO, "Starting getPageFaultCount function (Linux)");
    size_t pageFaults = 0;

    std::ifstream stat("/proc/self/stat");
    std::string statLine;
    if (std::getline(stat, statLine)) {
        std::istringstream iss(statLine);
        std::string token;
        int count = 0;
        while (iss >> token && count < 10) {
            count++;
        }
        if (count == 10 && iss >> token) {
            pageFaults = std::stoull(token);
        }
    }

    LOG_F(INFO, "Page fault count: {} (Linux)", pageFaults);
    return pageFaults;
}

auto getMemoryLoadPercentage() -> double {
    LOG_F(INFO, "Starting getMemoryLoadPercentage function (Linux)");
    
    unsigned long long total = getTotalMemorySize();
    unsigned long long available = getAvailableMemorySize();
    
    double memoryLoad = 0.0;
    if (total > 0) {
        memoryLoad = ((double)(total - available) / total) * 100.0;
    }
    
    LOG_F(INFO, "Memory load: {}% (Linux)", memoryLoad);
    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    LOG_F(INFO, "Getting memory performance metrics (Linux)");
    MemoryPerformance perf{};

    // 使用/proc/meminfo和/proc/vmstat获取内存性能指标
    std::ifstream vmstat("/proc/vmstat");
    std::string line;
    unsigned long pgpgin = 0, pgpgout = 0;
    
    while (std::getline(vmstat, line)) {
        if (line.find("pgpgin") == 0) {
            std::istringstream iss(line);
            std::string name;
            iss >> name >> pgpgin;
        } else if (line.find("pgpgout") == 0) {
            std::istringstream iss(line);
            std::string name;
            iss >> name >> pgpgout;
        }
    }
    
    // 获取采样前的值
    unsigned long pgpgin_before = pgpgin;
    unsigned long pgpgout_before = pgpgout;
    
    // 等待一秒钟
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 重新读取值
    vmstat.clear();
    vmstat.seekg(0, std::ios::beg);
    
    while (std::getline(vmstat, line)) {
        if (line.find("pgpgin") == 0) {
            std::istringstream iss(line);
            std::string name;
            iss >> name >> pgpgin;
        } else if (line.find("pgpgout") == 0) {
            std::istringstream iss(line);
            std::string name;
            iss >> name >> pgpgout;
        }
    }
    
    // 计算每秒的页面换入换出
    unsigned long pgpgin_persec = pgpgin - pgpgin_before;
    unsigned long pgpgout_persec = pgpgout - pgpgout_before;
    
    // 页面大小通常为4KB，转换为MB/s
    perf.readSpeed = pgpgin_persec * 4.0 / 1024;
    perf.writeSpeed = pgpgout_persec * 4.0 / 1024;
    
    // 计算带宽使用率
    perf.bandwidthUsage = (perf.readSpeed + perf.writeSpeed) /
                        (getTotalMemorySize() / 1024.0 / 1024) * 100.0;

    // 测量内存延迟
    const int TEST_SIZE = 1024 * 1024;  // 1MB
    std::vector<int> testData(TEST_SIZE);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < TEST_SIZE; i++) {
        testData[i] = i;
    }
    auto end = std::chrono::high_resolution_clock::now();
    perf.latency =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
            .count() /
        static_cast<double>(TEST_SIZE);

    LOG_F(INFO,
          "Memory performance metrics: Read: {:.2f} MB/s, Write: {:.2f} MB/s, "
          "Bandwidth: {:.1f}%, Latency: {:.2f} ns (Linux)",
          perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

} // namespace linux
} // namespace atom::system

#endif // __linux__
