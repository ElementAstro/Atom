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
#include "windows.hpp"
#elif defined(__linux__)
#include "linux.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
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
    LOG_F(ERROR, "getMemoryUsage: Unsupported platform");
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
    LOG_F(ERROR, "getTotalMemorySize: Unsupported platform");
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
    LOG_F(ERROR, "getAvailableMemorySize: Unsupported platform");
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
    LOG_F(ERROR, "getPhysicalMemoryInfo: Unsupported platform");
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
    LOG_F(ERROR, "getVirtualMemoryMax: Unsupported platform");
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
    LOG_F(ERROR, "getVirtualMemoryUsed: Unsupported platform");
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
    LOG_F(ERROR, "getSwapMemoryTotal: Unsupported platform");
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
    LOG_F(ERROR, "getSwapMemoryUsed: Unsupported platform");
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
    LOG_F(ERROR, "getCommittedMemory: Unsupported platform");
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
    LOG_F(ERROR, "getUncommittedMemory: Unsupported platform");
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
    LOG_F(ERROR, "getDetailedMemoryStats: Unsupported platform");
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
    LOG_F(ERROR, "getPeakWorkingSetSize: Unsupported platform");
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
    LOG_F(ERROR, "getCurrentWorkingSetSize: Unsupported platform");
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
    LOG_F(ERROR, "getPageFaultCount: Unsupported platform");
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
    LOG_F(ERROR, "getMemoryLoadPercentage: Unsupported platform");
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
    LOG_F(ERROR, "getMemoryPerformance: Unsupported platform");
    return MemoryPerformance();
#endif
}

// 以下函数在common.cpp中已经实现了通用逻辑
// - startMemoryMonitoring
// - stopMemoryMonitoring
// - getMemoryTimeline
// - detectMemoryLeaks
// - getMemoryFragmentation
// - optimizeMemoryUsage
// - analyzeMemoryBottlenecks

} // namespace atom::system
