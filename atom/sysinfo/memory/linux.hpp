/**
 * @file linux.hpp
 * @brief Linux platform implementation for memory information
 *
 * This file contains Linux-specific implementations for retrieving memory information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_LINUX_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_LINUX_HPP

#ifdef __linux__

#include <string>
#include "memory.hpp"

namespace atom::system {
namespace linux {

// Get memory usage as percentage (Linux implementation)
auto getMemoryUsage() -> float;

// Get total physical memory size in bytes (Linux implementation)
auto getTotalMemorySize() -> unsigned long long;

// Get available physical memory size in bytes (Linux implementation)
auto getAvailableMemorySize() -> unsigned long long;

// Get physical memory module information (Linux implementation)
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

// Get maximum virtual memory size in bytes (Linux implementation)
auto getVirtualMemoryMax() -> unsigned long long;

// Get used virtual memory size in bytes (Linux implementation)
auto getVirtualMemoryUsed() -> unsigned long long;

// Get total swap memory size in bytes (Linux implementation)
auto getSwapMemoryTotal() -> unsigned long long;

// Get used swap memory size in bytes (Linux implementation)
auto getSwapMemoryUsed() -> unsigned long long;

// Get committed memory size in bytes (Linux implementation)
auto getCommittedMemory() -> size_t;

// Get uncommitted memory size in bytes (Linux implementation)
auto getUncommittedMemory() -> size_t;

// Get detailed memory statistics (Linux implementation)
auto getDetailedMemoryStats() -> MemoryInfo;

// Get peak working set size of current process in bytes (Linux implementation)
auto getPeakWorkingSetSize() -> size_t;

// Get current working set size of process in bytes (Linux implementation)
auto getCurrentWorkingSetSize() -> size_t;

// Get page fault count (Linux implementation)
auto getPageFaultCount() -> size_t;

// Get memory load percentage (Linux implementation)
auto getMemoryLoadPercentage() -> double;

// Get memory performance metrics (Linux implementation)
auto getMemoryPerformance() -> MemoryPerformance;

} // namespace linux
} // namespace atom::system

#endif // __linux__
#endif // ATOM_SYSTEM_MODULE_MEMORY_LINUX_HPP
