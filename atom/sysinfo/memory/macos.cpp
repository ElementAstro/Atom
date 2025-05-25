/**
 * @file macos.cpp
 * @brief macOS platform implementation for memory information
 *
 * This file contains macOS-specific implementations for retrieving memory
 * information using macOS system interfaces.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "macos.hpp"
#include "common.hpp"

#ifdef __APPLE__
#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <spdlog/spdlog.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <chrono>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>


namespace atom::system::macos {

namespace {
constexpr size_t MB_TO_BYTES = 1024 * 1024;
constexpr size_t PAGE_SIZE_BYTES = 4096;
constexpr double DEFAULT_MAX_BANDWIDTH_GBPS = 25.6;

class PipeDeleter {
public:
    void operator()(FILE* pipe) const {
        if (pipe) {
            pclose(pipe);
        }
    }
};

using PipePtr = std::unique_ptr<FILE, PipeDeleter>;

auto executePipeCommand(const std::string& command) -> std::string {
    PipePtr pipe(popen(command.c_str(), "r"));
    if (!pipe) {
        throw std::runtime_error("Failed to execute command: " + command);
    }

    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }

    return result;
}

auto parseMemoryValue(const std::string& value) -> unsigned long long {
    double numValue;
    char unit;
    if (sscanf(value.c_str(), "%lf%c", &numValue, &unit) == 2) {
        switch (unit) {
            case 'M':
            case 'm':
                return static_cast<unsigned long long>(numValue * MB_TO_BYTES);
            case 'G':
            case 'g':
                return static_cast<unsigned long long>(numValue * MB_TO_BYTES *
                                                       1024);
            case 'K':
            case 'k':
                return static_cast<unsigned long long>(numValue * 1024);
            default:
                break;
        }
    }
    return static_cast<unsigned long long>(std::stoull(value));
}

auto getSysctlValue(const std::string& name) -> unsigned long long {
    try {
        const auto command = "sysctl -n " + name;
        const auto result = executePipeCommand(command);
        return std::stoull(result);
    } catch (const std::exception& e) {
        spdlog::error("Failed to get sysctl value for {}: {}", name, e.what());
        return 0;
    }
}
}  // namespace

auto getMemoryUsage() -> float {
    spdlog::debug("Getting memory usage percentage");

    try {
        const auto totalMemory = getTotalMemorySize();
        const auto availableMemory = getAvailableMemorySize();

        if (totalMemory == 0) {
            spdlog::error("Total memory is zero");
            return 0.0f;
        }

        const auto usedMemory = totalMemory - availableMemory;
        const auto memoryUsage =
            static_cast<float>(usedMemory) / totalMemory * 100.0f;

        spdlog::debug("Memory usage: {:.2f}% (Total: {} bytes, Used: {} bytes)",
                      memoryUsage, totalMemory, usedMemory);

        return memoryUsage;
    } catch (const std::exception& e) {
        spdlog::error("Error getting memory usage: {}", e.what());
        return 0.0f;
    }
}

auto getTotalMemorySize() -> unsigned long long {
    spdlog::debug("Getting total memory size");

    const auto totalMemorySize = getSysctlValue("hw.memsize");
    spdlog::debug("Total memory size: {} bytes", totalMemorySize);
    return totalMemorySize;
}

auto getAvailableMemorySize() -> unsigned long long {
    spdlog::debug("Getting available memory size");

    try {
        const auto result = executePipeCommand(
            "vm_stat | grep 'Pages free:' | awk '{print $3}' | tr -d '.'");
        const auto freePages = std::stoull(result);
        const auto availableMemorySize = freePages * PAGE_SIZE_BYTES;

        spdlog::debug("Available memory size: {} bytes ({} pages)",
                      availableMemorySize, freePages);
        return availableMemorySize;
    } catch (const std::exception& e) {
        spdlog::error("Error getting available memory size: {}", e.what());
        return 0;
    }
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
    spdlog::debug("Getting physical memory information");

    MemoryInfo::MemorySlot slot;

    try {
        const auto totalBytes = getTotalMemorySize();
        slot.capacity = std::to_string(totalBytes / MB_TO_BYTES);

        try {
            const auto typeResult = executePipeCommand(
                "system_profiler SPMemoryDataType | grep 'Type:' | head -n 1 | "
                "awk -F': ' '{print $2}' | tr -d '\\n\\r'");
            if (!typeResult.empty()) {
                slot.type = typeResult;
            } else {
                slot.type = "DDR";
            }
        } catch (...) {
            slot.type = "DDR";
        }

        try {
            const auto speedResult = executePipeCommand(
                "system_profiler SPMemoryDataType | grep 'Speed:' | head -n 1 "
                "| awk -F': ' '{print $2}' | tr -d '\\n\\r'");
            if (!speedResult.empty()) {
                slot.clockSpeed = speedResult;
            } else {
                slot.clockSpeed = "Unknown";
            }
        } catch (...) {
            slot.clockSpeed = "Unknown";
        }

        spdlog::debug("Physical memory - Capacity: {} MB, Type: {}, Speed: {}",
                      slot.capacity, slot.type, slot.clockSpeed);

    } catch (const std::exception& e) {
        spdlog::error("Error getting physical memory info: {}", e.what());
        slot.capacity = "0";
        slot.type = "Unknown";
        slot.clockSpeed = "Unknown";
    }

    return slot;
}

auto getVirtualMemoryMax() -> unsigned long long {
    spdlog::debug("Getting maximum virtual memory size");

    try {
        const auto result = executePipeCommand(
            "sysctl vm.swapusage | awk '{print $4}' | tr -d ','");
        const auto virtualMemoryMax = parseMemoryValue(result);

        spdlog::debug("Virtual memory max: {} bytes", virtualMemoryMax);
        return virtualMemoryMax;
    } catch (const std::exception& e) {
        spdlog::error("Error getting virtual memory max: {}", e.what());
        return 0;
    }
}

auto getVirtualMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used virtual memory size");

    try {
        const auto result = executePipeCommand(
            "sysctl vm.swapusage | awk '{print $7}' | tr -d ','");
        const auto virtualMemoryUsed = parseMemoryValue(result);

        spdlog::debug("Virtual memory used: {} bytes", virtualMemoryUsed);
        return virtualMemoryUsed;
    } catch (const std::exception& e) {
        spdlog::error("Error getting virtual memory used: {}", e.what());
        return 0;
    }
}

auto getSwapMemoryTotal() -> unsigned long long {
    spdlog::debug("Getting total swap memory size");

    try {
        const auto result = executePipeCommand(
            "sysctl vm.swapusage | awk '{print $4}' | tr -d ','");
        const auto swapMemoryTotal = parseMemoryValue(result);

        spdlog::debug("Swap memory total: {} bytes", swapMemoryTotal);
        return swapMemoryTotal;
    } catch (const std::exception& e) {
        spdlog::error("Error getting swap memory total: {}", e.what());
        return 0;
    }
}

auto getSwapMemoryUsed() -> unsigned long long {
    spdlog::debug("Getting used swap memory size");

    try {
        const auto result = executePipeCommand(
            "sysctl vm.swapusage | awk '{print $7}' | tr -d ','");
        const auto swapMemoryUsed = parseMemoryValue(result);

        spdlog::debug("Swap memory used: {} bytes", swapMemoryUsed);
        return swapMemoryUsed;
    } catch (const std::exception& e) {
        spdlog::error("Error getting swap memory used: {}", e.what());
        return 0;
    }
}

auto getCommittedMemory() -> size_t {
    spdlog::debug("Getting committed memory size");

    const auto totalMemory = getTotalMemorySize();
    const auto availableMemory = getAvailableMemorySize();
    const auto committedMemory = totalMemory - availableMemory;

    spdlog::debug("Committed memory: {} bytes", committedMemory);
    return static_cast<size_t>(committedMemory);
}

auto getUncommittedMemory() -> size_t {
    spdlog::debug("Getting uncommitted memory size");

    const auto uncommittedMemory = getAvailableMemorySize();
    spdlog::debug("Uncommitted memory: {} bytes", uncommittedMemory);
    return static_cast<size_t>(uncommittedMemory);
}

auto getDetailedMemoryStats() -> MemoryInfo {
    spdlog::debug("Getting detailed memory statistics");

    MemoryInfo info{};

    try {
        info.totalPhysicalMemory = getTotalMemorySize();
        info.availablePhysicalMemory = getAvailableMemorySize();

        if (info.totalPhysicalMemory > 0) {
            info.memoryLoadPercentage =
                static_cast<double>(info.totalPhysicalMemory -
                                    info.availablePhysicalMemory) /
                info.totalPhysicalMemory * 100.0;
        }

        info.swapMemoryTotal = getSwapMemoryTotal();
        info.swapMemoryUsed = getSwapMemoryUsed();
        info.virtualMemoryMax = info.totalPhysicalMemory + info.swapMemoryTotal;
        info.virtualMemoryUsed =
            (info.totalPhysicalMemory - info.availablePhysicalMemory) +
            info.swapMemoryUsed;

        task_basic_info_data_t taskInfo{};
        mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), TASK_BASIC_INFO,
                      reinterpret_cast<task_info_t>(&taskInfo),
                      &count) == KERN_SUCCESS) {
            info.workingSetSize = taskInfo.resident_size;
            info.peakWorkingSetSize = taskInfo.resident_size;
        }

        task_events_info_data_t eventsInfo{};
        count = TASK_EVENTS_INFO_COUNT;
        if (task_info(mach_task_self(), TASK_EVENTS_INFO,
                      reinterpret_cast<task_info_t>(&eventsInfo),
                      &count) == KERN_SUCCESS) {
            info.pageFaultCount = eventsInfo.faults;
        }

        info.quotaPagedPoolUsage = 0;
        info.quotaPeakPagedPoolUsage = 0;

        const auto slot = getPhysicalMemoryInfo();
        info.slots.push_back(slot);

        spdlog::debug("Detailed memory statistics retrieved successfully");

    } catch (const std::exception& e) {
        spdlog::error("Error getting detailed memory stats: {}", e.what());
    }

    return info;
}

auto getPeakWorkingSetSize() -> size_t {
    spdlog::debug("Getting peak working set size");

    task_basic_info_data_t taskInfo{};
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&taskInfo),
                  &count) == KERN_SUCCESS) {
        spdlog::debug("Peak working set size: {} bytes",
                      taskInfo.resident_size);
        return taskInfo.resident_size;
    }

    spdlog::error("Failed to get peak working set size");
    return 0;
}

auto getCurrentWorkingSetSize() -> size_t {
    spdlog::debug("Getting current working set size");

    task_basic_info_data_t taskInfo{};
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&taskInfo),
                  &count) == KERN_SUCCESS) {
        spdlog::debug("Current working set size: {} bytes",
                      taskInfo.resident_size);
        return taskInfo.resident_size;
    }

    spdlog::error("Failed to get current working set size");
    return 0;
}

auto getPageFaultCount() -> size_t {
    spdlog::debug("Getting page fault count");

    task_events_info_data_t eventsInfo{};
    mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_EVENTS_INFO,
                  reinterpret_cast<task_info_t>(&eventsInfo),
                  &count) == KERN_SUCCESS) {
        spdlog::debug("Page fault count: {}", eventsInfo.faults);
        return eventsInfo.faults;
    }

    spdlog::error("Failed to get page fault count");
    return 0;
}

auto getMemoryLoadPercentage() -> double {
    spdlog::debug("Getting memory load percentage");

    const auto total = getTotalMemorySize();
    const auto available = getAvailableMemorySize();

    if (total == 0) {
        spdlog::error("Total memory is zero");
        return 0.0;
    }

    const auto memoryLoad =
        static_cast<double>(total - available) / total * 100.0;
    spdlog::debug("Memory load: {:.2f}%", memoryLoad);
    return memoryLoad;
}

auto getMemoryPerformance() -> MemoryPerformance {
    spdlog::debug("Getting memory performance metrics");

    MemoryPerformance perf{};

    try {
        constexpr int TEST_SIZE = 1024 * 1024;
        std::vector<int> testData(TEST_SIZE);

        const auto writeStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < TEST_SIZE; ++i) {
            testData[i] = i;
        }
        const auto writeEnd = std::chrono::high_resolution_clock::now();

        const auto writeTime =
            std::chrono::duration<double>(writeEnd - writeStart).count();
        const auto writeSpeedMBps =
            (TEST_SIZE * sizeof(int)) / (1024.0 * 1024.0) / writeTime;

        volatile int sum = 0;
        const auto readStart = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < TEST_SIZE; ++i) {
            sum += testData[i];
        }
        const auto readEnd = std::chrono::high_resolution_clock::now();

        const auto readTime =
            std::chrono::duration<double>(readEnd - readStart).count();
        const auto readSpeedMBps =
            (TEST_SIZE * sizeof(int)) / (1024.0 * 1024.0) / readTime;

        perf.readSpeed = readSpeedMBps;
        perf.writeSpeed = writeSpeedMBps;
        perf.latency = (readTime + writeTime) / (2.0 * TEST_SIZE) * 1e9;

        const auto maxBandwidthMBps = DEFAULT_MAX_BANDWIDTH_GBPS * 1024.0;
        perf.bandwidthUsage =
            (readSpeedMBps + writeSpeedMBps) / maxBandwidthMBps * 100.0;

        spdlog::debug(
            "Memory performance - Read: {:.2f} MB/s, Write: {:.2f} MB/s, "
            "Bandwidth: {:.1f}%, Latency: {:.2f} ns",
            perf.readSpeed, perf.writeSpeed, perf.bandwidthUsage, perf.latency);

    } catch (const std::exception& e) {
        spdlog::error("Error getting memory performance: {}", e.what());
    }

    return perf;
}

auto getMemoryBandwidthUsage() -> double {
    spdlog::debug("Getting memory bandwidth usage");

    const auto performance = getMemoryPerformance();
    spdlog::debug("Memory bandwidth usage: {:.2f}%",
                  performance.bandwidthUsage);
    return performance.bandwidthUsage;
}

auto getSystemCacheInfo() -> CacheInfo {
    spdlog::debug("Getting system cache information");

    CacheInfo cacheInfo{};

    try {
        const auto result = executePipeCommand(
            "vm_stat | grep 'Pages wired down:' | awk '{print $4}' | tr -d "
            "'.'");
        const auto wiredPages = std::stoull(result);

        cacheInfo.totalSize = wiredPages * PAGE_SIZE_BYTES;
        cacheInfo.usedSize = cacheInfo.totalSize;
        cacheInfo.hitRate = 95.0;

        spdlog::debug("System cache info - Total: {} bytes, Used: {} bytes",
                      cacheInfo.totalSize, cacheInfo.usedSize);

    } catch (const std::exception& e) {
        spdlog::error("Error getting system cache info: {}", e.what());
    }

    return cacheInfo;
}

auto getMemoryPressureLevel() -> MemoryPressureLevel {
    spdlog::debug("Getting memory pressure level");

    const auto memoryLoad = getMemoryLoadPercentage();

    MemoryPressureLevel pressureLevel;
    if (memoryLoad < 60.0) {
        pressureLevel = MemoryPressureLevel::Low;
    } else if (memoryLoad < 80.0) {
        pressureLevel = MemoryPressureLevel::Medium;
    } else if (memoryLoad < 95.0) {
        pressureLevel = MemoryPressureLevel::High;
    } else {
        pressureLevel = MemoryPressureLevel::Critical;
    }

    spdlog::debug("Memory pressure level: {} (load: {:.2f}%)",
                  static_cast<int>(pressureLevel), memoryLoad);
    return pressureLevel;
}

}  // namespace atom::system::macos

#endif  // __APPLE__
