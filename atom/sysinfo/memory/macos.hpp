/**
 * @file macos.hpp
 * @brief macOS platform implementation for memory information
 *
 * This file contains macOS-specific implementations for retrieving memory information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP

#ifdef __APPLE__

#include <string>
#include "memory.hpp"

namespace atom::system {
namespace macos {

// Get memory usage as percentage (macOS implementation)
auto getMemoryUsage() -> float;

// Get total physical memory size in bytes (macOS implementation)
auto getTotalMemorySize() -> unsigned long long;

// Get available physical memory size in bytes (macOS implementation)
auto getAvailableMemorySize() -> unsigned long long;

// Get physical memory module information (macOS implementation)
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

// Get maximum virtual memory size in bytes (macOS implementation)
auto getVirtualMemoryMax() -> unsigned long long;

// Get used virtual memory size in bytes (macOS implementation)
auto getVirtualMemoryUsed() -> unsigned long long;

// Get total swap memory size in bytes (macOS implementation)
auto getSwapMemoryTotal() -> unsigned long long;

// Get used swap memory size in bytes (macOS implementation)
auto getSwapMemoryUsed() -> unsigned long long;

// Get committed memory size in bytes (macOS implementation)
auto getCommittedMemory() -> size_t;

// Get uncommitted memory size in bytes (macOS implementation)
auto getUncommittedMemory() -> size_t;

// Get detailed memory statistics (macOS implementation)
auto getDetailedMemoryStats() -> MemoryInfo;

// Get peak working set size of current process in bytes (macOS implementation)
auto getPeakWorkingSetSize() -> size_t;

// Get current working set size of process in bytes (macOS implementation)
auto getCurrentWorkingSetSize() -> size_t;

// Get page fault count (macOS implementation)
auto getPageFaultCount() -> size_t;

// Get memory load percentage (macOS implementation)
auto getMemoryLoadPercentage() -> double;

// Get memory performance metrics (macOS implementation)
auto getMemoryPerformance() -> MemoryPerformance;

} // namespace macos
} // namespace atom::system

#endif // __APPLE__
#endif // ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP
