/**
 * @file macos.hpp
 * @brief macOS platform implementation for memory information
 *
 * This file contains macOS-specific implementations for retrieving memory
 * information using macOS system interfaces.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP

#ifdef __APPLE__

#include "memory.hpp"

namespace atom::system::macos {

/**
 * @brief Get current memory usage percentage
 * @return Memory usage as percentage (0.0 - 100.0)
 */
auto getMemoryUsage() -> float;

/**
 * @brief Get total physical memory size
 * @return Total physical memory in bytes
 */
auto getTotalMemorySize() -> unsigned long long;

/**
 * @brief Get available physical memory size
 * @return Available physical memory in bytes
 */
auto getAvailableMemorySize() -> unsigned long long;

/**
 * @brief Get physical memory module information
 * @return MemorySlot structure containing memory module details
 */
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

/**
 * @brief Get maximum virtual memory size
 * @return Maximum virtual memory in bytes
 */
auto getVirtualMemoryMax() -> unsigned long long;

/**
 * @brief Get used virtual memory size
 * @return Used virtual memory in bytes
 */
auto getVirtualMemoryUsed() -> unsigned long long;

/**
 * @brief Get total swap memory size
 * @return Total swap memory in bytes
 */
auto getSwapMemoryTotal() -> unsigned long long;

/**
 * @brief Get used swap memory size
 * @return Used swap memory in bytes
 */
auto getSwapMemoryUsed() -> unsigned long long;

/**
 * @brief Get committed memory size
 * @return Committed memory in bytes
 */
auto getCommittedMemory() -> size_t;

/**
 * @brief Get uncommitted memory size
 * @return Uncommitted memory in bytes
 */
auto getUncommittedMemory() -> size_t;

/**
 * @brief Get comprehensive memory statistics
 * @return MemoryInfo structure with detailed memory information
 */
auto getDetailedMemoryStats() -> MemoryInfo;

/**
 * @brief Get peak working set size of current process
 * @return Peak working set size in bytes
 */
auto getPeakWorkingSetSize() -> size_t;

/**
 * @brief Get current working set size of process
 * @return Current working set size in bytes
 */
auto getCurrentWorkingSetSize() -> size_t;

/**
 * @brief Get page fault count for current process
 * @return Number of page faults
 */
auto getPageFaultCount() -> size_t;

/**
 * @brief Get system memory load percentage
 * @return Memory load as percentage (0.0 - 100.0)
 */
auto getMemoryLoadPercentage() -> double;

/**
 * @brief Get memory performance metrics
 * @return MemoryPerformance structure with performance data
 */
auto getMemoryPerformance() -> MemoryPerformance;

/**
 * @brief Get memory bandwidth usage percentage
 * @return Bandwidth usage as percentage (0.0 - 100.0)
 */
auto getMemoryBandwidthUsage() -> double;

/**
 * @brief Get system cache information
 * @return CacheInfo structure with cache details
 */
auto getSystemCacheInfo() -> CacheInfo;

/**
 * @brief Get current memory pressure level
 * @return MemoryPressureLevel enumeration value
 */
auto getMemoryPressureLevel() -> MemoryPressureLevel;

}  // namespace atom::system::macos

#endif  // __APPLE__
#endif  // ATOM_SYSTEM_MODULE_MEMORY_MACOS_HPP
