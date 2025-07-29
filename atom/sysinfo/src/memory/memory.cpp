/*
 * memory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Memory Implementation

**************************************************/

#include "memory.hpp"
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
    spdlog::error("getMemoryUsage: Unsupported platform. Unable to retrieve memory usage.");
    return 0.0f;
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
    spdlog::error("getTotalMemorySize: Unsupported platform. Unable to retrieve total memory size.");
    return 0;
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
    spdlog::error("getAvailableMemorySize: Unsupported platform. Unable to retrieve available memory size.");
    return 0;
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
    spdlog::error("getPhysicalMemoryInfo: Unsupported platform. Unable to retrieve physical memory information.");
    return MemoryInfo::MemorySlot();
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
    spdlog::error("getVirtualMemoryMax: Unsupported platform. Unable to retrieve maximum virtual memory.");
    return 0;
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
    spdlog::error("getVirtualMemoryUsed: Unsupported platform. Unable to retrieve used virtual memory.");
    return 0;
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
    spdlog::error("getSwapMemoryTotal: Unsupported platform. Unable to retrieve total swap memory.");
    return 0;
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
    spdlog::error("getSwapMemoryUsed: Unsupported platform. Unable to retrieve used swap memory.");
    return 0;
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
    spdlog::error("getCommittedMemory: Unsupported platform. Unable to retrieve committed memory.");
    return 0;
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
    spdlog::error("getUncommittedMemory: Unsupported platform. Unable to retrieve uncommitted memory.");
    return 0;
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
    spdlog::error("getDetailedMemoryStats: Unsupported platform. Unable to retrieve detailed memory statistics.");
    return MemoryInfo();
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
    spdlog::error("getPeakWorkingSetSize: Unsupported platform. Unable to retrieve peak working set size.");
    return 0;
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
    spdlog::error("getCurrentWorkingSetSize: Unsupported platform. Unable to retrieve current working set size.");
    return 0;
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
    spdlog::error("getPageFaultCount: Unsupported platform. Unable to retrieve page fault count.");
    return 0;
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
    spdlog::error("getMemoryLoadPercentage: Unsupported platform. Unable to retrieve memory load percentage.");
    return 0.0;
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
    spdlog::error("getMemoryPerformance: Unsupported platform. Unable to retrieve memory performance information.");
    return MemoryPerformance();
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
    spdlog::error("detectMemoryPressure: Unsupported platform. Using fallback implementation.");
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

} // namespace atom::system
