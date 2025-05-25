/**
 * @file windows.cpp
 * @brief Windows platform implementation for memory information
 *
 * This file contains Windows-specific implementations for retrieving memory
 * information using Windows API.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "windows.hpp"
#include "common.hpp"

#ifdef _WIN32
#include <intrin.h>
#include <iphlpapi.h>
#include <pdh.h>
#include <psapi.h>
#include <spdlog/spdlog.h>
#include <tlhelp32.h>
#include <wincon.h>
#include <windows.h>
#include <chrono>
#include <thread>


namespace atom::system::windows {

auto getMemoryUsage() -> float {
    spdlog::debug("Getting memory usage percentage");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get memory status: {}", GetLastError());
        return 0.0f;
    }

    const auto totalMemory =
        static_cast<float>(status.ullTotalPhys) / (1024.0f * 1024.0f);
    const auto availableMemory =
        static_cast<float>(status.ullAvailPhys) / (1024.0f * 1024.0f);
    const auto memoryUsage =
        (totalMemory - availableMemory) / totalMemory * 100.0f;

    spdlog::debug(
        "Memory usage: {:.2f}% (Total: {:.2f} MB, Available: {:.2f} MB)",
        memoryUsage, totalMemory, availableMemory);

    return memoryUsage;
}

auto getTotalMemorySize() -> unsigned long long {
    spdlog::debug("Getting total memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get total memory size: {}", GetLastError());
        return 0;
    }

    spdlog::debug("Total memory size: {} bytes", status.ullTotalPhys);
    return status.ullTotalPhys;
}

auto getAvailableMemorySize() -> unsigned long long {
    spdlog::debug("Getting available memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get available memory size: {}",
                      GetLastError());
        return 0;
    }

    spdlog::debug("Available memory size: {} bytes", status.ullAvailPhys);
    return status.ullAvailPhys;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    spdlog::debug("Getting physical memory information");

    MemoryInfo::MemorySlot slot;

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get physical memory info: {}", GetLastError());
        return slot;
    }

    slot.capacity = std::to_string(status.ullTotalPhys / (1024 * 1024));
    slot.type = "DDR";
    slot.clockSpeed = "Unknown";

    spdlog::debug("Physical memory capacity: {} MB", slot.capacity);
    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    spdlog::debug("Getting maximum virtual memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get virtual memory max: {}", GetLastError());
        return 0;
    }

    const auto virtualMemoryMax = status.ullTotalVirtual;
    spdlog::debug("Maximum virtual memory: {} bytes", virtualMemoryMax);
    return virtualMemoryMax;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used virtual memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get virtual memory used: {}", GetLastError());
        return 0;
    }

    const auto virtualMemoryUsed =
        status.ullTotalVirtual - status.ullAvailVirtual;
    spdlog::debug("Used virtual memory: {} bytes", virtualMemoryUsed);
    return virtualMemoryUsed;
}

auto getSwapMemoryTotal() -> unsigned long long {
    spdlog::debug("Getting total swap memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get swap memory total: {}", GetLastError());
        return 0;
    }

    const auto swapMemoryTotal = status.ullTotalPageFile;
    spdlog::debug("Total swap memory: {} bytes", swapMemoryTotal);
    return swapMemoryTotal;
}

auto getSwapMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used swap memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get swap memory used: {}", GetLastError());
        return 0;
    }

    const auto swapMemoryUsed =
        status.ullTotalPageFile - status.ullAvailPageFile;
    spdlog::debug("Used swap memory: {} bytes", swapMemoryUsed);
    return swapMemoryUsed;
}

auto getCommittedMemory() -> size_t {
    spdlog::debug("Getting committed memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get committed memory: {}", GetLastError());
        return 0;
    }

    const auto committedMemory = status.ullTotalPhys - status.ullAvailPhys;
    spdlog::debug("Committed memory: {} bytes", committedMemory);
    return static_cast<size_t>(committedMemory);
}

auto getUncommittedMemory() -> size_t {
    spdlog::debug("Getting uncommitted memory size");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get uncommitted memory: {}", GetLastError());
        return 0;
    }

    const auto uncommittedMemory = status.ullAvailPhys;
    spdlog::debug("Uncommitted memory: {} bytes", uncommittedMemory);
    return static_cast<size_t>(uncommittedMemory);
}

auto getDetailedMemoryStats() -> MemoryInfo {
    spdlog::debug("Getting detailed memory statistics");

    MemoryInfo info{};

    MEMORYSTATUSEX memStatus{};
    memStatus.dwLength = sizeof(memStatus);

    if (!GlobalMemoryStatusEx(&memStatus)) {
        spdlog::error("Failed to get memory status: {}", GetLastError());
        return info;
    }

    info.memoryLoadPercentage = memStatus.dwMemoryLoad;
    info.totalPhysicalMemory = memStatus.ullTotalPhys;
    info.availablePhysicalMemory = memStatus.ullAvailPhys;
    info.virtualMemoryMax = memStatus.ullTotalVirtual;
    info.virtualMemoryUsed =
        memStatus.ullTotalVirtual - memStatus.ullAvailVirtual;
    info.swapMemoryTotal = memStatus.ullTotalPageFile;
    info.swapMemoryUsed =
        memStatus.ullTotalPageFile - memStatus.ullAvailPageFile;

    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        info.pageFaultCount = pmc.PageFaultCount;
        info.peakWorkingSetSize = pmc.PeakWorkingSetSize;
        info.workingSetSize = pmc.WorkingSetSize;
        info.quotaPeakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
        info.quotaPagedPoolUsage = pmc.QuotaPagedPoolUsage;
        spdlog::debug("Process memory counters retrieved successfully");
    } else {
        spdlog::error("Failed to get process memory info: {}", GetLastError());
    }

    MemoryInfo::MemorySlot slot;
    slot.capacity = std::to_string(info.totalPhysicalMemory / (1024 * 1024));
    slot.type = "DDR";
    slot.clockSpeed = "Unknown";
    info.slots.push_back(slot);

    spdlog::debug("Detailed memory statistics retrieved successfully");
    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    spdlog::debug("Getting peak working set size");

    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        spdlog::error("Failed to get peak working set size: {}",
                      GetLastError());
        return 0;
    }

    spdlog::debug("Peak working set size: {} bytes", pmc.PeakWorkingSetSize);
    return pmc.PeakWorkingSetSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    spdlog::debug("Getting current working set size");

    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        spdlog::error("Failed to get current working set size: {}",
                      GetLastError());
        return 0;
    }

    spdlog::debug("Current working set size: {} bytes", pmc.WorkingSetSize);
    return pmc.WorkingSetSize;
}

auto getPageFaultCount() -> size_t {
    spdlog::debug("Getting page fault count");

    PROCESS_MEMORY_COUNTERS pmc{};
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        spdlog::error("Failed to get page fault count: {}", GetLastError());
        return 0;
    }

    spdlog::debug("Page fault count: {}", pmc.PageFaultCount);
    return pmc.PageFaultCount;
}

auto getMemoryLoadPercentage() -> double {
    spdlog::debug("Getting memory load percentage");

    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get memory load: {}", GetLastError());
        return 0.0;
    }

    const auto memoryLoad = static_cast<double>(status.dwMemoryLoad);
    spdlog::debug("Memory load: {}%", memoryLoad);
    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    spdlog::debug("Getting memory performance metrics");

    MemoryPerformance perf{};

    PDH_HQUERY query;
    PDH_HCOUNTER readCounter, writeCounter;

    if (PdhOpenQuery(nullptr, 0, &query) == ERROR_SUCCESS) {
        if (PdhAddCounterW(query, L"\\Memory\\Pages/sec", 0, &readCounter) ==
                ERROR_SUCCESS &&
            PdhAddCounterW(query, L"\\Memory\\Page Writes/sec", 0,
                           &writeCounter) == ERROR_SUCCESS) {
            PdhCollectQueryData(query);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            PdhCollectQueryData(query);

            PDH_FMT_COUNTERVALUE readValue, writeValue;
            if (PdhGetFormattedCounterValue(readCounter, PDH_FMT_DOUBLE,
                                            nullptr,
                                            &readValue) == ERROR_SUCCESS &&
                PdhGetFormattedCounterValue(writeCounter, PDH_FMT_DOUBLE,
                                            nullptr,
                                            &writeValue) == ERROR_SUCCESS) {
                constexpr double PAGE_SIZE_KB = 4.0;
                constexpr double KB_TO_MB = 1.0 / 1024.0;

                perf.readSpeed =
                    readValue.doubleValue * PAGE_SIZE_KB * KB_TO_MB;
                perf.writeSpeed =
                    writeValue.doubleValue * PAGE_SIZE_KB * KB_TO_MB;
            }
        }
        PdhCloseQuery(query);
    } else {
        spdlog::warn("Failed to open PDH query for memory performance");
    }

    const auto totalMemoryMB =
        static_cast<double>(getTotalMemorySize()) / (1024.0 * 1024.0);
    perf.bandwidthUsage =
        totalMemoryMB > 0
            ? (perf.readSpeed + perf.writeSpeed) / totalMemoryMB * 100.0
            : 0.0;

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
}  // namespace atom::system::windows

#endif  // _WIN32
