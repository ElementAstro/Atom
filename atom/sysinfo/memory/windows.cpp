/*
 * windows.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory Windows Implementation

**************************************************/

#include "windows.hpp"
#include "common.hpp"

#ifdef _WIN32
#include "atom/log/loguru.hpp"

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

namespace atom::system {
namespace windows {

auto getMemoryUsage() -> float {
    LOG_F(INFO, "Starting getMemoryUsage function (Windows)");
    float memoryUsage = 0.0;

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

    LOG_F(INFO, "Finished getMemoryUsage function (Windows)");
    return memoryUsage;
}

auto getTotalMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getTotalMemorySize function (Windows)");
    unsigned long long totalMemorySize = 0;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        totalMemorySize = status.ullTotalPhys;
        LOG_F(INFO, "Total Memory Size: {} bytes", totalMemorySize);
    } else {
        LOG_F(ERROR, "GetTotalMemorySize error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getTotalMemorySize function (Windows)");
    return totalMemorySize;
}

auto getAvailableMemorySize() -> unsigned long long {
    LOG_F(INFO, "Starting getAvailableMemorySize function (Windows)");
    unsigned long long availableMemorySize = 0;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        availableMemorySize = status.ullAvailPhys;
        LOG_F(INFO, "Available Memory Size: {} bytes", availableMemorySize);
    } else {
        LOG_F(ERROR,
              "GetAvailableMemorySize error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getAvailableMemorySize function (Windows)");
    return availableMemorySize;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    LOG_F(INFO, "Starting getPhysicalMemoryInfo function (Windows)");
    MemoryInfo::MemorySlot slot;

    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        slot.capacity = std::to_string(memoryStatus.ullTotalPhys /
                                       (1024 * 1024));
        LOG_F(INFO, "Physical Memory Capacity: {} MB", slot.capacity);
    } else {
        LOG_F(ERROR, "GetPhysicalMemoryInfo error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getPhysicalMemoryInfo function (Windows)");
    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryMax function (Windows)");
    unsigned long long virtualMemoryMax = 0;

    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        virtualMemoryMax = memoryStatus.ullTotalVirtual / (1024 * 1024);
        LOG_F(INFO, "Virtual Memory Max: {} MB", virtualMemoryMax);
    } else {
        LOG_F(ERROR, "GetVirtualMemoryMax error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getVirtualMemoryMax function (Windows)");
    return virtualMemoryMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getVirtualMemoryUsed function (Windows)");
    unsigned long long virtualMemoryUsed = 0;

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

    LOG_F(INFO, "Finished getVirtualMemoryUsed function (Windows)");
    return virtualMemoryUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryTotal function (Windows)");
    unsigned long long swapMemoryTotal = 0;

    MEMORYSTATUSEX memoryStatus;
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (GlobalMemoryStatusEx(&memoryStatus)) {
        swapMemoryTotal = memoryStatus.ullTotalPageFile / (1024 * 1024);
        LOG_F(INFO, "Swap Memory Total: {} MB", swapMemoryTotal);
    } else {
        LOG_F(ERROR, "GetSwapMemoryTotal error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getSwapMemoryTotal function (Windows)");
    return swapMemoryTotal;
}

auto getSwapMemoryUsed() -> unsigned long long {
    LOG_F(INFO, "Starting getSwapMemoryUsed function (Windows)");
    unsigned long long swapMemoryUsed = 0;

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

    LOG_F(INFO, "Finished getSwapMemoryUsed function (Windows)");
    return swapMemoryUsed;
}

auto getCommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getCommittedMemory function (Windows)");
    size_t committedMemory = 0;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        committedMemory = status.ullTotalPhys - status.ullAvailPhys;
        LOG_F(INFO, "Committed Memory: {} bytes", committedMemory);
    } else {
        LOG_F(ERROR, "GetCommittedMemory error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getCommittedMemory function (Windows)");
    return committedMemory;
}

auto getUncommittedMemory() -> size_t {
    LOG_F(INFO, "Starting getUncommittedMemory function (Windows)");
    size_t uncommittedMemory = 0;

    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        uncommittedMemory = status.ullAvailPhys;
        LOG_F(INFO, "Uncommitted Memory: {} bytes", uncommittedMemory);
    } else {
        LOG_F(ERROR, "GetUncommittedMemory error: GlobalMemoryStatusEx error");
    }

    LOG_F(INFO, "Finished getUncommittedMemory function (Windows)");
    return uncommittedMemory;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    LOG_F(INFO, "Starting getDetailedMemoryStats function (Windows)");
    MemoryInfo info;

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        info.memoryLoadPercentage = memStatus.dwMemoryLoad;
        info.totalPhysicalMemory = memStatus.ullTotalPhys;
        info.availablePhysicalMemory = memStatus.ullAvailPhys;
        info.virtualMemoryMax = memStatus.ullTotalVirtual;
        info.virtualMemoryUsed =
            memStatus.ullTotalVirtual - memStatus.ullAvailVirtual;
        info.swapMemoryTotal = memStatus.ullTotalPageFile;
        info.swapMemoryUsed = memStatus.ullTotalPageFile - memStatus.ullAvailPageFile;

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            info.pageFaultCount = pmc.PageFaultCount;
            info.peakWorkingSetSize = pmc.PeakWorkingSetSize;
            info.workingSetSize = pmc.WorkingSetSize;
            info.quotaPeakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
            info.quotaPagedPoolUsage = pmc.QuotaPagedPoolUsage;
            
            LOG_F(INFO, "Process memory counters retrieved successfully");
        } else {
            LOG_F(ERROR, "GetProcessMemoryInfo failed");
        }
    } else {
        LOG_F(ERROR, "GlobalMemoryStatusEx failed");
    }

    // 获取物理内存插槽信息 (这里使用简化实现，实际需要使用WMI)
    MemoryInfo::MemorySlot slot;
    slot.capacity = std::to_string(info.totalPhysicalMemory / (1024 * 1024));
    slot.type = "Unknown"; // 在实际应用中，可以通过WMI获取详细信息
    slot.clockSpeed = "Unknown";
    info.slots.push_back(slot);

    LOG_F(INFO, "Finished getDetailedMemoryStats function (Windows)");
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getPeakWorkingSetSize function (Windows)");
    size_t peakSize = 0;

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        peakSize = pmc.PeakWorkingSetSize;
    }

    LOG_F(INFO, "Peak working set size: {} bytes (Windows)", peakSize);
    return peakSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    LOG_F(INFO, "Starting getCurrentWorkingSetSize function (Windows)");
    size_t currentSize = 0;

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        currentSize = pmc.WorkingSetSize;
    }

    LOG_F(INFO, "Current working set size: {} bytes (Windows)", currentSize);
    return currentSize;
}

auto getPageFaultCount() -> size_t {
    LOG_F(INFO, "Starting getPageFaultCount function (Windows)");
    size_t pageFaults = 0;

    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        pageFaults = pmc.PageFaultCount;
    }

    LOG_F(INFO, "Page fault count: {} (Windows)", pageFaults);
    return pageFaults;
}

auto getMemoryLoadPercentage() -> double {
    LOG_F(INFO, "Starting getMemoryLoadPercentage function (Windows)");
    double memoryLoad = 0.0;

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        memoryLoad = memStatus.dwMemoryLoad;
    }

    LOG_F(INFO, "Memory load: {}% (Windows)", memoryLoad);
    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    LOG_F(INFO, "Getting memory performance metrics (Windows)");
    MemoryPerformance perf{};
    
    // 使用Windows性能计数器获取内存性能指标
    PDH_HQUERY query;
    PDH_HCOUNTER readCounter, writeCounter;
    if (PdhOpenQuery(NULL, 0, &query) == ERROR_SUCCESS) {
        PdhAddCounterW(query, L"\\Memory\\Pages/sec", 0, &readCounter);
        PdhAddCounterW(query, L"\\Memory\\Page Writes/sec", 0, &writeCounter);
        PdhCollectQueryData(query);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        PdhCollectQueryData(query);
        
        PDH_FMT_COUNTERVALUE readValue, writeValue;
        PdhGetFormattedCounterValue(readCounter, PDH_FMT_DOUBLE, NULL, &readValue);
        PdhGetFormattedCounterValue(writeCounter, PDH_FMT_DOUBLE, NULL, &writeValue);
        
        // 页面大小通常为4KB
        perf.readSpeed = readValue.doubleValue * 4.0 / 1024; // 转换为MB/s
        perf.writeSpeed = writeValue.doubleValue * 4.0 / 1024; // 转换为MB/s
        
        PdhCloseQuery(query);
    }

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
          "Bandwidth: {:.1f}%, Latency: {:.2f} ns (Windows)",
          perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

} // namespace windows
} // namespace atom::system

#endif // _WIN32
