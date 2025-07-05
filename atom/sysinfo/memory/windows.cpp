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

namespace {
constexpr double MB_DIVISOR = 1024.0 * 1024.0;
constexpr double KB_TO_MB = 1.0 / 1024.0;
constexpr double PAGE_SIZE_KB = 4.0;
constexpr int MEMORY_TEST_SIZE = 1024 * 1024;

static MEMORYSTATUSEX getMemoryStatus() {
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx(&status)) {
        spdlog::error("Failed to get memory status: {}", GetLastError());
    }

    return status;
}

static PROCESS_MEMORY_COUNTERS getProcessMemoryCounters() {
    PROCESS_MEMORY_COUNTERS pmc{};

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        spdlog::error("Failed to get process memory info: {}", GetLastError());
    }

    return pmc;
}
}

auto getMemoryUsage() -> float {
    spdlog::debug("Getting memory usage percentage");

    const auto status = getMemoryStatus();
    if (status.ullTotalPhys == 0) {
        return 0.0f;
    }

    const auto totalMemoryMB = static_cast<float>(status.ullTotalPhys) / MB_DIVISOR;
    const auto availableMemoryMB = static_cast<float>(status.ullAvailPhys) / MB_DIVISOR;
    const auto usagePercent = (totalMemoryMB - availableMemoryMB) / totalMemoryMB * 100.0f;

    spdlog::debug("Memory usage: {:.2f}% (Total: {:.2f} MB, Available: {:.2f} MB)",
                  usagePercent, totalMemoryMB, availableMemoryMB);

    return usagePercent;
}

auto getTotalMemorySize() -> unsigned long long {
    spdlog::debug("Getting total memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalPhys > 0) {
        spdlog::debug("Total memory size: {} bytes", status.ullTotalPhys);
    }

    return status.ullTotalPhys;
}

auto getAvailableMemorySize() -> unsigned long long {
    spdlog::debug("Getting available memory size");

    const auto status = getMemoryStatus();
    if (status.ullAvailPhys > 0) {
        spdlog::debug("Available memory size: {} bytes", status.ullAvailPhys);
    }

    return status.ullAvailPhys;
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    spdlog::debug("Getting physical memory information");

    MemoryInfo::MemorySlot slot;
    const auto status = getMemoryStatus();

    if (status.ullTotalPhys > 0) {
        slot.capacity = std::to_string(status.ullTotalPhys / (1024 * 1024));
        slot.type = "DDR";
        slot.clockSpeed = "Unknown";
        spdlog::debug("Physical memory capacity: {} MB", slot.capacity);
    }

    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    spdlog::debug("Getting maximum virtual memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalVirtual > 0) {
        spdlog::debug("Maximum virtual memory: {} bytes", status.ullTotalVirtual);
    }

    return status.ullTotalVirtual;
}

auto getVirtualMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used virtual memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalVirtual > 0 && status.ullAvailVirtual <= status.ullTotalVirtual) {
        const auto usedVirtual = status.ullTotalVirtual - status.ullAvailVirtual;
        spdlog::debug("Used virtual memory: {} bytes", usedVirtual);
        return usedVirtual;
    }

    return 0;
}

auto getSwapMemoryTotal() -> unsigned long long {
    spdlog::debug("Getting total swap memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalPageFile > 0) {
        spdlog::debug("Total swap memory: {} bytes", status.ullTotalPageFile);
    }

    return status.ullTotalPageFile;
}

auto getSwapMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used swap memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalPageFile > 0 && status.ullAvailPageFile <= status.ullTotalPageFile) {
        const auto usedSwap = status.ullTotalPageFile - status.ullAvailPageFile;
        spdlog::debug("Used swap memory: {} bytes", usedSwap);
        return usedSwap;
    }

    return 0;
}

auto getCommittedMemory() -> size_t {
    spdlog::debug("Getting committed memory size");

    const auto status = getMemoryStatus();
    if (status.ullTotalPhys > 0 && status.ullAvailPhys <= status.ullTotalPhys) {
        const auto committed = status.ullTotalPhys - status.ullAvailPhys;
        spdlog::debug("Committed memory: {} bytes", committed);
        return static_cast<size_t>(committed);
    }

    return 0;
}

auto getUncommittedMemory() -> size_t {
    spdlog::debug("Getting uncommitted memory size");

    const auto status = getMemoryStatus();
    if (status.ullAvailPhys > 0) {
        spdlog::debug("Uncommitted memory: {} bytes", status.ullAvailPhys);
        return static_cast<size_t>(status.ullAvailPhys);
    }

    return 0;
}

auto getDetailedMemoryStats() -> MemoryInfo {
    spdlog::debug("Getting detailed memory statistics");

    MemoryInfo info{};
    const auto memStatus = getMemoryStatus();

    if (memStatus.ullTotalPhys > 0) {
        info.memoryLoadPercentage = memStatus.dwMemoryLoad;
        info.totalPhysicalMemory = memStatus.ullTotalPhys;
        info.availablePhysicalMemory = memStatus.ullAvailPhys;
        info.virtualMemoryMax = memStatus.ullTotalVirtual;
        info.virtualMemoryUsed = memStatus.ullTotalVirtual - memStatus.ullAvailVirtual;
        info.swapMemoryTotal = memStatus.ullTotalPageFile;
        info.swapMemoryUsed = memStatus.ullTotalPageFile - memStatus.ullAvailPageFile;

        const auto pmc = getProcessMemoryCounters();
        if (pmc.WorkingSetSize > 0) {
            info.pageFaultCount = pmc.PageFaultCount;
            info.peakWorkingSetSize = pmc.PeakWorkingSetSize;
            info.workingSetSize = pmc.WorkingSetSize;
            info.quotaPeakPagedPoolUsage = pmc.QuotaPeakPagedPoolUsage;
            info.quotaPagedPoolUsage = pmc.QuotaPagedPoolUsage;
            spdlog::debug("Process memory counters retrieved successfully");
        }

        MemoryInfo::MemorySlot slot;
        slot.capacity = std::to_string(info.totalPhysicalMemory / (1024 * 1024));
        slot.type = "DDR";
        slot.clockSpeed = "Unknown";
        info.slots.push_back(std::move(slot));

        spdlog::debug("Detailed memory statistics retrieved successfully");
    }

    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    spdlog::debug("Getting peak working set size");

    const auto pmc = getProcessMemoryCounters();
    if (pmc.PeakWorkingSetSize > 0) {
        spdlog::debug("Peak working set size: {} bytes", pmc.PeakWorkingSetSize);
    }

    return pmc.PeakWorkingSetSize;
}

auto getCurrentWorkingSetSize() -> size_t {
    spdlog::debug("Getting current working set size");

    const auto pmc = getProcessMemoryCounters();
    if (pmc.WorkingSetSize > 0) {
        spdlog::debug("Current working set size: {} bytes", pmc.WorkingSetSize);
    }

    return pmc.WorkingSetSize;
}

auto getPageFaultCount() -> size_t {
    spdlog::debug("Getting page fault count");

    const auto pmc = getProcessMemoryCounters();
    spdlog::debug("Page fault count: {}", pmc.PageFaultCount);

    return pmc.PageFaultCount;
}

auto getMemoryLoadPercentage() -> double {
    spdlog::debug("Getting memory load percentage");

    const auto status = getMemoryStatus();
    const auto memoryLoad = static_cast<double>(status.dwMemoryLoad);

    if (status.ullTotalPhys > 0) {
        spdlog::debug("Memory load: {}%", memoryLoad);
    }

    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    spdlog::debug("Getting memory performance metrics");

    MemoryPerformance perf{};

    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER readCounter = nullptr;
    PDH_HCOUNTER writeCounter = nullptr;

    if (PdhOpenQuery(nullptr, 0, &query) == ERROR_SUCCESS) {
        const auto addCounterResult1 = PdhAddCounterW(query, L"\\Memory\\Pages/sec", 0, &readCounter);
        const auto addCounterResult2 = PdhAddCounterW(query, L"\\Memory\\Page Writes/sec", 0, &writeCounter);

        if (addCounterResult1 == ERROR_SUCCESS && addCounterResult2 == ERROR_SUCCESS) {
            PdhCollectQueryData(query);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            PdhCollectQueryData(query);

            PDH_FMT_COUNTERVALUE readValue{};
            PDH_FMT_COUNTERVALUE writeValue{};

            const auto getValueResult1 = PdhGetFormattedCounterValue(readCounter, PDH_FMT_DOUBLE, nullptr, &readValue);
            const auto getValueResult2 = PdhGetFormattedCounterValue(writeCounter, PDH_FMT_DOUBLE, nullptr, &writeValue);

            if (getValueResult1 == ERROR_SUCCESS && getValueResult2 == ERROR_SUCCESS) {
                perf.readSpeed = readValue.doubleValue * PAGE_SIZE_KB * KB_TO_MB;
                perf.writeSpeed = writeValue.doubleValue * PAGE_SIZE_KB * KB_TO_MB;
            } else {
                spdlog::warn("Failed to get formatted counter values");
            }
        } else {
            spdlog::warn("Failed to add PDH counters");
        }
        PdhCloseQuery(query);
    } else {
        spdlog::warn("Failed to open PDH query for memory performance");
    }

    const auto totalMemoryMB = static_cast<double>(getTotalMemorySize()) / MB_DIVISOR;
    perf.bandwidthUsage = totalMemoryMB > 0 ? (perf.readSpeed + perf.writeSpeed) / totalMemoryMB * 100.0 : 0.0;

    std::vector<int> testData;
    testData.reserve(MEMORY_TEST_SIZE);

    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < MEMORY_TEST_SIZE; ++i) {
        testData.push_back(i);
    }
    const auto end = std::chrono::high_resolution_clock::now();

    perf.latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / static_cast<double>(MEMORY_TEST_SIZE);

    spdlog::debug("Memory performance - Read: {:.2f} MB/s, Write: {:.2f} MB/s, Bandwidth: {:.1f}%, Latency: {:.2f} ns",
                  perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    return perf;
}

}  // namespace atom::system::windows

#endif  // _WIN32
