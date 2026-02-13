/*
 * memory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory Implementation

**************************************************/

#include "../../hardware/memory.hpp"
#include <spdlog/spdlog.h>
#include "common.hpp"

// 包含平台特定的头文件
#ifdef _WIN32
#include "platform/windows.hpp"
#elif defined(__linux__)
#include "platform/linux.hpp"
#elif defined(__APPLE__)
#include "platform/macos.hpp"
#endif

namespace atom::system {

auto getMemoryUsage() -> float {
#ifdef _WIN32
    return windows::getMemoryUsage();
#elif defined(__linux__)
    return linux::getMemoryUsage();
#elif defined(__APPLE__)
    return macos::getMemoryUsage();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getMemoryUsage: Unsupported platform. Unable "
                               "to retrieve memory usage.");
    == == == == spdlog::error("getMemoryUsage: Unsupported platform");
    >>>>>>>> test - fixes / systematic -
                 testing : atom / sysinfo / hardware / memory /
                           memory.cpp return 0.0f;
#endif
}

auto getTotalMemorySize() -> unsigned long long {
#ifdef _WIN32
    return windows::getTotalMemorySize();
#elif defined(__linux__)
    return linux::getTotalMemorySize();
#elif defined(__APPLE__)
    return macos::getTotalMemorySize();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getTotalMemorySize: Unsupported platform. "
                               "Unable to retrieve total memory size.");
    == == == == spdlog::error("getTotalMemorySize: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getAvailableMemorySize() -> unsigned long long {
#ifdef _WIN32
    return windows::getAvailableMemorySize();
#elif defined(__linux__)
    return linux::getAvailableMemorySize();
#elif defined(__APPLE__)
    return macos::getAvailableMemorySize();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getAvailableMemorySize: Unsupported platform. "
                               "Unable to retrieve available memory size.");
    == == == == spdlog::error("getAvailableMemorySize: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot {
#ifdef _WIN32
    return windows::getPhysicalMemoryInfo();
#elif defined(__linux__)
    return linux::getPhysicalMemoryInfo();
#elif defined(__APPLE__)
    return macos::getPhysicalMemoryInfo();
#else
    < < < < < < < <
        HEAD : atom / sysinfo / src / memory /
               memory.cpp spdlog::error(
                   "getPhysicalMemoryInfo: Unsupported platform. Unable to "
                   "retrieve physical memory information.");
    == == == == spdlog::error("getPhysicalMemoryInfo: Unsupported platform");
    >>>>>>>> test - fixes / systematic -
                 testing : atom / sysinfo / hardware / memory /
                           memory.cpp return MemoryInfo::MemorySlot();
#endif
}

auto getVirtualMemoryMax() -> unsigned long long {
#ifdef _WIN32
    return windows::getVirtualMemoryMax();
#elif defined(__linux__)
    return linux::getVirtualMemoryMax();
#elif defined(__APPLE__)
    return macos::getVirtualMemoryMax();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getVirtualMemoryMax: Unsupported platform. "
                               "Unable to retrieve maximum virtual memory.");
    == == == == spdlog::error("getVirtualMemoryMax: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getVirtualMemoryUsed() -> unsigned long long {
#ifdef _WIN32
    return windows::getVirtualMemoryUsed();
#elif defined(__linux__)
    return linux::getVirtualMemoryUsed();
#elif defined(__APPLE__)
    return macos::getVirtualMemoryUsed();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getVirtualMemoryUsed: Unsupported platform. "
                               "Unable to retrieve used virtual memory.");
    == == == == spdlog::error("getVirtualMemoryUsed: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getSwapMemoryTotal() -> unsigned long long {
#ifdef _WIN32
    return windows::getSwapMemoryTotal();
#elif defined(__linux__)
    return linux::getSwapMemoryTotal();
#elif defined(__APPLE__)
    return macos::getSwapMemoryTotal();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getSwapMemoryTotal: Unsupported platform. "
                               "Unable to retrieve total swap memory.");
    == == == == spdlog::error("getSwapMemoryTotal: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getSwapMemoryUsed() -> unsigned long long {
#ifdef _WIN32
    return windows::getSwapMemoryUsed();
#elif defined(__linux__)
    return linux::getSwapMemoryUsed();
#elif defined(__APPLE__)
    return macos::getSwapMemoryUsed();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getSwapMemoryUsed: Unsupported platform. "
                               "Unable to retrieve used swap memory.");
    == == == == spdlog::error("getSwapMemoryUsed: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getCommittedMemory() -> size_t {
#ifdef _WIN32
    return windows::getCommittedMemory();
#elif defined(__linux__)
    return linux::getCommittedMemory();
#elif defined(__APPLE__)
    return macos::getCommittedMemory();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getCommittedMemory: Unsupported platform. "
                               "Unable to retrieve committed memory.");
    == == == == spdlog::error("getCommittedMemory: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getUncommittedMemory() -> size_t {
#ifdef _WIN32
    return windows::getUncommittedMemory();
#elif defined(__linux__)
    return linux::getUncommittedMemory();
#elif defined(__APPLE__)
    return macos::getUncommittedMemory();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getUncommittedMemory: Unsupported platform. "
                               "Unable to retrieve uncommitted memory.");
    == == == == spdlog::error("getUncommittedMemory: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getDetailedMemoryStats() -> MemoryInfo {
#ifdef _WIN32
    return windows::getDetailedMemoryStats();
#elif defined(__linux__)
    return linux::getDetailedMemoryStats();
#elif defined(__APPLE__)
    return macos::getDetailedMemoryStats();
#else
    < < < < < < < <
        HEAD : atom / sysinfo / src / memory /
               memory.cpp spdlog::error(
                   "getDetailedMemoryStats: Unsupported platform. Unable to "
                   "retrieve detailed memory statistics.");
    == == == == spdlog::error("getDetailedMemoryStats: Unsupported platform");
    >>>>>>>> test - fixes / systematic -
                 testing : atom / sysinfo / hardware / memory /
                           memory.cpp return MemoryInfo();
#endif
}

auto getPeakWorkingSetSize() -> size_t {
#ifdef _WIN32
    return windows::getPeakWorkingSetSize();
#elif defined(__linux__)
    return linux::getPeakWorkingSetSize();
#elif defined(__APPLE__)
    return macos::getPeakWorkingSetSize();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getPeakWorkingSetSize: Unsupported platform. "
                               "Unable to retrieve peak working set size.");
    == == == == spdlog::error("getPeakWorkingSetSize: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getCurrentWorkingSetSize() -> size_t {
#ifdef _WIN32
    return windows::getCurrentWorkingSetSize();
#elif defined(__linux__)
    return linux::getCurrentWorkingSetSize();
#elif defined(__APPLE__)
    return macos::getCurrentWorkingSetSize();
#else
    < < < < < < < <
        HEAD : atom / sysinfo / src / memory /
               memory.cpp spdlog::error(
                   "getCurrentWorkingSetSize: Unsupported platform. Unable to "
                   "retrieve current working set size.");
    == == == == spdlog::error("getCurrentWorkingSetSize: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getPageFaultCount() -> size_t {
#ifdef _WIN32
    return windows::getPageFaultCount();
#elif defined(__linux__)
    return linux::getPageFaultCount();
#elif defined(__APPLE__)
    return macos::getPageFaultCount();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getPageFaultCount: Unsupported platform. "
                               "Unable to retrieve page fault count.");
    == == == == spdlog::error("getPageFaultCount: Unsupported platform");
    >>>>>>>>
        test - fixes / systematic -
            testing : atom / sysinfo / hardware / memory / memory.cpp return 0;
#endif
}

auto getMemoryLoadPercentage() -> double {
#ifdef _WIN32
    return windows::getMemoryLoadPercentage();
#elif defined(__linux__)
    return linux::getMemoryLoadPercentage();
#elif defined(__APPLE__)
    return macos::getMemoryLoadPercentage();
#else
    < < < < < < < < HEAD : atom / sysinfo / src / memory /
                           memory.cpp spdlog::error(
                               "getMemoryLoadPercentage: Unsupported platform. "
                               "Unable to retrieve memory load percentage.");
    == == == == spdlog::error("getMemoryLoadPercentage: Unsupported platform");
    >>>>>>>> test - fixes / systematic -
                 testing : atom / sysinfo / hardware / memory /
                           memory.cpp return 0.0;
#endif
}

auto getMemoryPerformance() -> MemoryPerformance {
#ifdef _WIN32
    return windows::getMemoryPerformance();
#elif defined(__linux__)
    return linux::getMemoryPerformance();
#elif defined(__APPLE__)
    return macos::getMemoryPerformance();
#else
    < < < < < < < <
        HEAD : atom / sysinfo / src / memory /
               memory.cpp spdlog::error(
                   "getMemoryPerformance: Unsupported platform. Unable to "
                   "retrieve memory performance information.");
    == == == == spdlog::error("getMemoryPerformance: Unsupported platform");
    >>>>>>>> test - fixes / systematic -
                 testing : atom / sysinfo / hardware / memory /
                           memory.cpp return MemoryPerformance();
#endif
}

auto detectMemoryPressure() -> MemoryPressureInfo {
#ifdef _WIN32
    // For now, use common implementation - could add Windows-specific later
    return ::atom::system::detectMemoryPressure();
#elif defined(__linux__)
    return linux::getMemoryPressureInfo();
#elif defined(__APPLE__)
    // For now, use common implementation - could add macOS-specific later
    return ::atom::system::detectMemoryPressure();
#else
    spdlog::error(
        "detectMemoryPressure: Unsupported platform. Using fallback "
        "implementation.");
    return ::atom::system::detectMemoryPressure();
#endif
}

auto getMemoryPressureLevel() -> MemoryPressureLevel {
    const auto pressureInfo = detectMemoryPressure();
    return pressureInfo.level;
}

// Enhanced performance functions are implemented in common.cpp
// and will be called directly from there

// 以下函数在common.cpp中已经实现了通用逻辑
// - startMemoryMonitoring
// - stopMemoryMonitoring
// - getMemoryTimeline
// - detectMemoryLeaks
// - getMemoryFragmentation
// - optimizeMemoryUsage
// - analyzeMemoryBottlenecks
// - startMemoryPressureMonitoring
// - stopMemoryPressureMonitoring
// - getMemoryPressureHistory

}  // namespace atom::system
