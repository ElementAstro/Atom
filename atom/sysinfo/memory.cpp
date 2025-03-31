/*
 * memory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory

**************************************************/

#include "atom/sysinfo/memory.hpp"

#include "atom/log/loguru.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#include <wincon.h>
#include <chrono>
#include <thread>
// clang-format on
#elif __linux__
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
#elif __APPLE__
#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <sys/mount.h>
#include <sys/param.h>
#endif

namespace atom::system {
auto getMemoryUsage() -> float {
    LOG_F(INFO, "Starting getMemoryUsage function");
    float memoryUsage = 0.0;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    float totalMemory = 0.0f;
    float availableMemory = 0.0f;
    if (GlobalMemoryStatusEx(&status) != 0) {
        totalMemory =
            static_cast<float>(status.ullTotalPhys) / 1024.0f / 1024.0f;
        availableMemory =
            static_cast<float>(status.ullAvailPhys) / 1024.0f / 1024.0f;
        memoryUsage = (totalMemory - availableMemory) / totalMemory * 100.0;
        LOG_F(INFO,
              "Total Memory: %.2f MB, Available Memory: %.2f MB, Memory Usage: "
              "%.2f%%",
              totalMemory, availableMemory, memoryUsage);
    } else {
        LOG_F(ERROR, "GetMemoryUsage error: GlobalMemoryStatusEx error");
    }
#elif __linux__
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        LOG_F(ERROR, "GetMemoryUsage error: open /proc/meminfo error");
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

    unsigned long usedMemory =
        totalMemory - freeMemory - bufferMemory - cacheMemory;
    memoryUsage = static_cast<float>(usedMemory) / totalMemory * 100.0;
    LOG_F(INFO,
          "Total Memory: {} kB, Free Memory: {} kB, Buffer Memory: {} kB, "
          "Cache Memory: {} kB, Memory Usage: {}",
          totalMemory, freeMemory, bufferMemory, cacheMemory, memoryUsage);
#elif __APPLE__
    struct statfs stats;
    statfs("/", &stats);

    unsigned long long total_space = stats.f_blocks * stats.f_bsize;
    unsigned long long free_space = stats.f_bfree * stats.f_bsize;

    unsigned long long used_space = total_space - free_space;
    memory_usage = static_cast<float>(used_space) / total_space * 100.0;
    LOG_F(INFO,
          "Total Space: {} bytes, Free Space: {} bytes, Used Space: {} "
          "bytes, Memory Usage: %.2f%%",
          total_space, free_space, used_space, memory_usage);
#elif defined(__ANDROID__)
    LOG_F(ERROR, "GetTotalMemorySize error: not support");
#endif

    LOG_F(INFO, "Finished getMemoryUsage function");
    return memoryUsage;
}

auto getTotalMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getTotalMemorySize function");
    unsigned long long totalMemorySize = 0;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        totalMemorySize = status.ullTotalPhys;
        LOG_F(INFO, "Total Memory Size: {} bytes", totalMemorySize);
    } else {
        LOG_F(ERROR, "GetTotalMemorySize error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl -n hw.memsize", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            totalMemorySize = std::stoull(buffer);
            LOG_F(INFO, "Total Memory Size: {} bytes", totalMemorySize);
        } else {
            LOG_F(ERROR, "GetTotalMemorySize error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    totalMemorySize = static_cast<unsigned long long>(pages) *
                      static_cast<unsigned long long>(pageSize);
    LOG_F(INFO, "Total Memory Size: {} bytes", totalMemorySize);
#endif

    LOG_F(INFO, "Finished getTotalMemorySize function");
    return totalMemorySize;
}

auto getAvailableMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getAvailableMemorySize function");
    unsigned long long availableMemorySize = 0;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        availableMemorySize = status.ullAvailPhys;
        LOG_F(INFO, "Available Memory Size: {} bytes", availableMemorySize);
    } else {
        LOG_F(ERROR,
              "GetAvailableMemorySize error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("vm_stat | grep 'Pages free:' | awk '{print $3}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            availableMemorySize = std::stoull(buffer) * getpagesize();
            LOG_F(INFO, "Available Memory Size: {} bytes", availableMemorySize);
        } else {
            LOG_F(ERROR, "GetAvailableMemorySize error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
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
            if (match.size() == 2) {
                availableMemorySize = std::stoull(match[1].str()) *
                                      1024;  // Convert from kB to bytes
                found = true;
                LOG_F(INFO, "Available Memory Size: {} bytes",
                      availableMemorySize);
                break;
            }
        }
    }

    if (!found) {
        LOG_F(ERROR, "GetAvailableMemorySize error: parse error");
        return -1;
    }
#endif
    LOG_F(INFO, "Finished getAvailableMemorySize function");
    return availableMemorySize;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    LOG_F(INFO, "Starting getPhysicalMemoryInfo function");
    MemoryInfo::MemorySlot slot;

#ifdef _WIN32
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        slot.capacity = std::to_string(memoryStatus.ullTotalPhys /
                                       (1024 * 1024));  // Convert bytes to MB
        LOG_F(INFO, "Physical Memory Capacity: {} MB", slot.capacity);
    } else {
        LOG_F(ERROR, "GetPhysicalMemoryInfo error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl hw.memsize | awk '{print $2}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            slot.capacity = std::string(buffer) / (1024 * 1024);
            LOG_F(INFO, "Physical Memory Capacity: {} MB", slot.capacity);
        } else {
            LOG_F(ERROR, "GetPhysicalMemoryInfo error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.substr(0, 10) == "MemTotal: ") {
            std::istringstream iss(line);
            std::vector<std::string> tokens{
                std::istream_iterator<std::string>{iss},
                std::istream_iterator<std::string>{}};
            slot.capacity = tokens[1];
            LOG_F(INFO, "Physical Memory Capacity: {} kB", slot.capacity);
            break;
        }
    }
#endif

    LOG_F(INFO, "Finished getPhysicalMemoryInfo function");
    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryMax function");
    unsigned long long virtualMemoryMax = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        virtualMemoryMax = memoryStatus.ullTotalVirtual / (1024 * 1024);
        LOG_F(INFO, "Virtual Memory Max: {} MB", virtualMemoryMax);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryMax error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $2}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            virtualMemoryMax = std::stoull(buffer) / (1024 * 1024);
            LOG_F(INFO, "Virtual Memory Max: {} MB", virtualMemoryMax);
        } else {
            LOG_F(ERROR, "GetVirtualMemoryMax error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        virtualMemoryMax = (si.totalram + si.totalswap) / 1024;
        LOG_F(INFO, "Virtual Memory Max: {} kB", virtualMemoryMax);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryMax error: sysinfo error");
    }
#endif

    LOG_F(INFO, "Finished getVirtualMemoryMax function");
    return virtualMemoryMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryUsed function");
    unsigned long long virtualMemoryUsed = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        virtualMemoryUsed =
            (memoryStatus.ullTotalVirtual - memoryStatus.ullAvailVirtual) /
            (1024 * 1024);
        LOG_F(INFO, "Virtual Memory Used: {} MB", virtualMemoryUsed);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryUsed error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $6}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            virtualMemoryUsed = std::stoull(buffer) / (1024 * 1024);
            LOG_F(INFO, "Virtual Memory Used: {} MB", virtualMemoryUsed);
        } else {
            LOG_F(ERROR, "GetVirtualMemoryUsed error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        virtualMemoryUsed =
            (si.totalram - si.freeram + si.totalswap - si.freeswap) / 1024;
        LOG_F(INFO, "Virtual Memory Used: {} kB", virtualMemoryUsed);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryUsed error: sysinfo error");
    }
#endif

    LOG_F(INFO, "Finished getVirtualMemoryUsed function");
    return virtualMemoryUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryTotal function");
    unsigned long long swapMemoryTotal = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        swapMemoryTotal = memoryStatus.ullTotalPageFile / (1024 * 1024);
        LOG_F(INFO, "Swap Memory Total: {} MB", swapMemoryTotal);
    } else {
        LOG_F(ERROR, "GetSwapMemoryTotal error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $2}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            swapMemoryTotal = std::stoull(buffer) / (1024 * 1024);
            LOG_F(INFO, "Swap Memory Total: {} MB", swapMemoryTotal);
        } else {
            LOG_F(ERROR, "GetSwapMemoryTotal error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        swapMemoryTotal = si.totalswap / 1024;
        LOG_F(INFO, "Swap Memory Total: {} kB", swapMemoryTotal);
    } else {
        LOG_F(ERROR, "GetSwapMemoryTotal error: sysinfo error");
    }
#endif

    LOG_F(INFO, "Finished getSwapMemoryTotal function");
    return swapMemoryTotal;
}

unsigned long long getSwapMemoryUsed() {
    LOG_F(INFO, "Starting getSwapMemoryUsed function");
    unsigned long long swapMemoryUsed = 0;

#ifdef _WIN32
    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        swapMemoryUsed =
            (memoryStatus.ullTotalPageFile - memoryStatus.ullAvailPageFile) /
            (1024 * 1024);
        LOG_F(INFO, "Swap Memory Used: {} MB", swapMemoryUsed);
    } else {
        LOG_F(ERROR, "GetSwapMemoryUsed error: GlobalMemoryStatusEx error");
    }
#elif defined(__APPLE__)
    FILE *pipe = popen("sysctl vm.swapusage | awk '{print $6}'", "r");
    if (pipe != nullptr) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            swapMemoryUsed = std::stoull(buffer) / (1024 * 1024);
            LOG_F(INFO, "Swap Memory Used: {} MB", swapMemoryUsed);
        } else {
            LOG_F(ERROR, "GetSwapMemoryUsed error: popen error");
        }
        pclose(pipe);
    }
#elif defined(__linux__)
    struct sysinfo si{};
    if (sysinfo(&si) == 0) {
        swapMemoryUsed = (si.totalswap - si.freeswap) / 1024;
        LOG_F(INFO, "Swap Memory Used: {} kB", swapMemoryUsed);
    } else {
        LOG_F(ERROR, "GetSwapMemoryUsed error: sysinfo error");
    }
#endif

    LOG_F(INFO, "Finished getSwapMemoryUsed function");
    return swapMemoryUsed;
}

auto getTotalMemory() -> size_t {
    LOG_F(INFO, "Starting getTotalMemory function");
    size_t totalMemory = 0;

#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        totalMemory = status.ullTotalPhys;
        LOG_F(INFO, "Total Memory: {} bytes", totalMemory);
    } else {
        LOG_F(ERROR, "GetTotalMemory error: GlobalMemoryStatusEx error");
    }
#elif defined(__linux__)
    std::ifstream memInfoFile("/proc/meminfo");
    if (!memInfoFile.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/meminfo");
        return -1;
    }

    std::string line;
    std::regex memTotalRegex(R"(MemTotal:\s+(\d+)\s+kB)");

    while (std::getline(memInfoFile, line)) {
        std::smatch match;
        if (std::regex_search(line, match, memTotalRegex)) {
            if (match.size() == 2) {
                totalMemory = std::stoull(match[1].str()) *
                              1024;  // Convert from kB to bytes
                LOG_F(INFO, "Total Memory: {} bytes", totalMemory);
                break;
            }
        }
    }

    if (totalMemory == 0) {
        LOG_F(ERROR, "GetTotalMemory error: parse error");
        return -1;
    }
#elif defined(__APPLE__)
    int mib[2];
    size_t length = sizeof(size_t);
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    if (sysctl(mib, 2, &totalMemory, &length, nullptr, 0) == 0) {
        LOG_F(INFO, "Total Memory: {} bytes", totalMemory);
    } else {
        LOG_F(ERROR, "GetTotalMemory error: sysctl error");
    }
#endif

    LOG_F(INFO, "Finished getTotalMemory function");
    return totalMemory;
}

auto getAvailableMemory() -> size_t {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullAvailPhys;
    }
    return 0;
#elif defined(__linux__)
    std::ifstream memInfoFile("/proc/meminfo");
    if (!memInfoFile.is_open()) {
        LOG_F(ERROR, "Failed to open /proc/meminfo");
        return 0;
    }

    std::string line;
    std::regex memAvailableRegex(R"(MemAvailable:\s+(\d+)\s+kB)");
    size_t availableMemory = 0;

    while (std::getline(memInfoFile, line)) {
        std::smatch match;
        if (std::regex_search(line, match, memAvailableRegex)) {
            if (match.size() == 2) {
                availableMemory = std::stoull(match[1].str()) *
                                  1024;  // Convert from kB to bytes
                LOG_F(INFO, "Available Memory: {} bytes", availableMemory);
                break;
            }
        }
    }

    if (availableMemory == 0) {
        LOG_F(ERROR, "GetAvailableMemory error: parse error");
    }
    return availableMemory;
#elif defined(__APPLE__)
    int mib[2];
    size_t length = sizeof(vm_statistics64);
    struct vm_statistics64 vm_stats;

    mib[0] = CTL_VM;
    mib[1] = VM_LOADAVG;
    if (sysctl(mib, 2, &vm_stats, &length, nullptr, 0) == 0) {
        return vm_stats.free_count * vm_page_size;
    }
    return 0;
#endif
}

auto getCommittedMemory() -> size_t {
    size_t totalMemory = getTotalMemory();
    size_t availableMemory = getAvailableMemory();
    return totalMemory - availableMemory;
}

auto getUncommittedMemory() -> size_t {
    size_t totalMemory = getTotalMemory();
    size_t committedMemory = getCommittedMemory();
    return totalMemory - committedMemory;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    LOG_F(INFO, "Starting getDetailedMemoryStats function");
    MemoryInfo info;

#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.memoryLoadPercentage = memStatus.dwMemoryLoad;
        info.totalPhysicalMemory = memStatus.ullTotalPhys;
        info.availablePhysicalMemory = memStatus.ullAvailPhys;
        info.virtualMemoryMax = memStatus.ullTotalVirtual;
        info.virtualMemoryUsed =
            memStatus.ullTotalVirtual - memStatus.ullAvailVirtual;

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            info.pageFaultCount = pmc.PageFaultCount;
            info.peakWorkingSetSize = pmc.PeakWorkingSetSize;
            info.workingSetSize = pmc.WorkingSetSize;
            info.quotaPeakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
            info.quotaPagedPoolUsage = pmc.QuotaPagedPoolUsage;
        }
    }
#elif defined(__linux__)
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.totalPhysicalMemory = si.totalram;
        info.availablePhysicalMemory = si.freeram;
        info.memoryLoadPercentage =
            ((double)(si.totalram - si.freeram) / si.totalram) * 100.0;
        info.swapMemoryTotal = si.totalswap;
        info.swapMemoryUsed = si.totalswap - si.freeswap;

        // 读取 /proc/self/status 获取进程相关信息
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmPeak:") != std::string::npos) {
                info.peakWorkingSetSize =
                    std::stoull(line.substr(line.find_last_of(" \t") + 1)) *
                    1024;
            } else if (line.find("VmSize:") != std::string::npos) {
                info.workingSetSize =
                    std::stoull(line.substr(line.find_last_of(" \t") + 1)) *
                    1024;
            }
        }
    }
#endif

    LOG_F(INFO, "Finished getDetailedMemoryStats function");
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getPeakWorkingSetSize function");
    size_t peakSize = 0;

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        peakSize = pmc.PeakWorkingSetSize;
    }
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmPeak:") != std::string::npos) {
            peakSize =
                std::stoull(line.substr(line.find_last_of(" \t") + 1)) * 1024;
            break;
        }
    }
#endif

    LOG_F(INFO, "Peak working set size: {} bytes", peakSize);
    return peakSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getCurrentWorkingSetSize function");
    size_t currentSize = 0;

#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        currentSize = pmc.WorkingSetSize;
    }
#elif defined(__linux__)
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.find("VmSize:") != std::string::npos) {
            currentSize =
                std::stoull(line.substr(line.find_last_of(" \t") + 1)) * 1024;
            break;
        }
    }
#endif

    LOG_F(INFO, "Current working set size: {} bytes", currentSize);
    return currentSize;
}

auto getMemoryPerformance() -> MemoryPerformance {
    LOG_F(INFO, "Getting memory performance metrics");
    MemoryPerformance perf{};
    // 测量内存读写速度
#ifdef _WIN32
    // 使用Windows性能计数器获取内存性能指标
    PDH_HQUERY query;
    PDH_HCOUNTER readCounter, writeCounter;
    if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
        PdhAddCounterW(query, L"\\Memory\\Pages/sec", 0, &readCounter);
        PdhAddCounterW(query, L"\\Memory\\Page Writes/sec", 0, &writeCounter);
        PdhCollectQueryData(query);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        PdhCollectQueryData(query);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        PdhCollectQueryData(query);

        PDH_FMT_COUNTERVALUE readValue, writeValue;
        PdhGetFormattedCounterValue(readCounter, PDH_FMT_DOUBLE, NULL,
                                    &readValue);
        PdhGetFormattedCounterValue(writeCounter, PDH_FMT_DOUBLE, NULL,
                                    &writeValue);

        perf.readSpeed =
            readValue.doubleValue * 4096 / 1024 / 1024;  // Convert to MB/s
        perf.writeSpeed = writeValue.doubleValue * 4096 / 1024 / 1024;

        PdhCloseQuery(query);
    }
#elif __linux__
    // 使用/proc/meminfo和/proc/vmstat获取内存性能指标
    std::ifstream vmstat("/proc/vmstat");
    std::string line;
    unsigned long pgpgin = 0, pgpgout = 0;
    while (std::getline(vmstat, line)) {
        if (line.find("pgpgin") == 0) {
            pgpgin = std::stoul(line.substr(7));
        } else if (line.find("pgpgout") == 0) {
            pgpgout = std::stoul(line.substr(8));
        }
    }
    perf.readSpeed = pgpgin * 4096.0 / 1024 / 1024;  // Convert to MB/s
    perf.writeSpeed = pgpgout * 4096.0 / 1024 / 1024;
#endif

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
          "Bandwidth: {:.1f}%, Latency: {:.2f} ns",
          perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

}  // namespace atom::system
