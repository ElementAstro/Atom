/**
 * @file windows.hpp
 * @brief Windows platform implementation for memory information
 *
 * This file contains Windows-specific implementations for retrieving memory
 * information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_WINDOWS_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_WINDOWS_HPP

#ifdef _WIN32

#include "memory.hpp"

namespace atom::system {
namespace windows {

// Get memory usage as percentage (Windows implementation)
auto getMemoryUsage() -> float;

// Get total physical memory size in bytes (Windows implementation)
auto getTotalMemorySize() -> unsigned long long;

// Get available physical memory size in bytes (Windows implementation)
auto getAvailableMemorySize() -> unsigned long long;

// Get physical memory module information (Windows implementation)
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

// Get maximum virtual memory size in bytes (Windows implementation)
auto getVirtualMemoryMax() -> unsigned long long;

// Get used virtual memory size in bytes (Windows implementation)
auto getVirtualMemoryUsed() -> unsigned long long;

// Get total swap memory size in bytes (Windows implementation)
auto getSwapMemoryTotal() -> unsigned long long;

// Get used swap memory size in bytes (Windows implementation)
auto getSwapMemoryUsed() -> unsigned long long;

// Get committed memory size in bytes (Windows implementation)
auto getCommittedMemory() -> size_t;

// Get uncommitted memory size in bytes (Windows implementation)
auto getUncommittedMemory() -> size_t;

// Get detailed memory statistics (Windows implementation)
auto getDetailedMemoryStats() -> MemoryInfo;

// Get peak working set size of current process in bytes (Windows
// implementation)
auto getPeakWorkingSetSize() -> size_t;

// Get current working set size of process in bytes (Windows implementation)
auto getCurrentWorkingSetSize() -> size_t;

// Get page fault count (Windows implementation)
auto getPageFaultCount() -> size_t;

// Get memory load percentage (Windows implementation)
auto getMemoryLoadPercentage() -> double;

// Get memory performance metrics (Windows implementation)
auto getMemoryPerformance() -> MemoryPerformance;

}  // namespace windows
}  // namespace atom::system

#endif  // _WIN32
#endif  // ATOM_SYSTEM_MODULE_MEMORY_WINDOWS_HPP
