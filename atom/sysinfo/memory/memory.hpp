/**
 * @file memory.hpp
 * @brief System memory information functionality
 *
 * This file contains definitions for retrieving and monitoring system memory
 * information across different platforms. It provides utilities for querying
 * physical memory, virtual memory, and swap memory statistics.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_HPP

#include <string>
#include <utility>
#include <vector>
#include <functional>
#include <chrono>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @struct MemoryInfo
 * @brief Comprehensive information about system memory
 *
 * Contains detailed information about physical memory slots,
 * virtual memory, swap memory, and process-specific memory metrics.
 */
struct MemoryInfo {
    /**
     * @struct MemorySlot
     * @brief Information about a physical memory module/slot
     *
     * Contains details about a specific physical memory module
     * including capacity, speed, and memory type.
     */
    struct MemorySlot {
        std::string capacity;   /**< Memory module capacity (e.g., "8GB") */
        std::string clockSpeed; /**< Memory clock speed (e.g., "3200MHz") */
        std::string type;       /**< Memory type (e.g., "DDR4", "DDR5") */

        /**
         * @brief Default constructor
         */
        MemorySlot() = default;

        /**
         * @brief Parameterized constructor
         * @param capacity Memory module capacity
         * @param clockSpeed Memory clock speed
         * @param type Memory module type
         */
        MemorySlot(std::string capacity, std::string clockSpeed,
                   std::string type)
            : capacity(std::move(capacity)),
              clockSpeed(std::move(clockSpeed)),
              type(std::move(type)) {}
    } ATOM_ALIGNAS(128);

    std::vector<MemorySlot> slots; /**< Collection of physical memory slots */
    unsigned long long
        virtualMemoryMax; /**< Maximum virtual memory size in bytes */
    unsigned long long virtualMemoryUsed; /**< Used virtual memory in bytes */
    unsigned long long swapMemoryTotal;   /**< Total swap memory in bytes */
    unsigned long long swapMemoryUsed;    /**< Used swap memory in bytes */

    double memoryLoadPercentage; /**< Current memory usage percentage */
    unsigned long long totalPhysicalMemory; /**< Total physical RAM in bytes */
    unsigned long long
        availablePhysicalMemory;       /**< Available physical RAM in bytes */
    unsigned long long pageFaultCount; /**< Number of page faults */
    unsigned long long
        peakWorkingSetSize;            /**< Peak working set size in bytes */
    unsigned long long workingSetSize; /**< Current working set size in bytes */
    unsigned long long
        quotaPeakPagedPoolUsage; /**< Peak paged pool usage in bytes */
    unsigned long long
        quotaPagedPoolUsage; /**< Current paged pool usage in bytes */
} ATOM_ALIGNAS(64);

/**
 * @struct MemoryPerformance
 * @brief Memory performance monitoring structure
 *
 * Contains metrics for memory read/write speeds, bandwidth usage,
 * latency, and historical latency data.
 */
struct MemoryPerformance {
    double readSpeed;        /**< Memory read speed in MB/s */
    double writeSpeed;       /**< Memory write speed in MB/s */
    double bandwidthUsage;   /**< Memory bandwidth usage percentage */
    double latency;          /**< Memory latency in nanoseconds */
    std::vector<double> latencyHistory; /**< Historical latency data */
} ATOM_ALIGNAS(32);

/**
 * @brief Get the memory usage percentage
 *
 * Calculates and returns the percentage of physical memory currently in use
 * by the system.
 *
 * @return Float value representing memory usage percentage (0-100)
 */
auto getMemoryUsage() -> float;

/**
 * @brief Get the total physical memory size
 *
 * Retrieves the total amount of physical RAM installed in the system.
 *
 * @return Total physical memory size in bytes
 */
auto getTotalMemorySize() -> unsigned long long;

/**
 * @brief Get the available physical memory size
 *
 * Retrieves the amount of physical RAM currently available for allocation.
 *
 * @return Available physical memory size in bytes
 */
auto getAvailableMemorySize() -> unsigned long long;

/**
 * @brief Get information about physical memory modules
 *
 * Retrieves details about the physical memory modules installed in the system.
 *
 * @return MemorySlot object containing information about the memory modules
 */
auto getPhysicalMemoryInfo() -> MemoryInfo::MemorySlot;

/**
 * @brief Get the maximum virtual memory size
 *
 * Retrieves the maximum amount of virtual memory available to processes.
 *
 * @return Maximum virtual memory size in bytes
 */
auto getVirtualMemoryMax() -> unsigned long long;

/**
 * @brief Get the currently used virtual memory
 *
 * Retrieves the amount of virtual memory currently in use by the system.
 *
 * @return Used virtual memory size in bytes
 */
auto getVirtualMemoryUsed() -> unsigned long long;

/**
 * @brief Get the total swap/page file size
 *
 * Retrieves the total amount of swap space or page file configured on the
 * system.
 *
 * @return Total swap memory size in bytes
 */
auto getSwapMemoryTotal() -> unsigned long long;

/**
 * @brief Get the used swap/page file size
 *
 * Retrieves the amount of swap space or page file currently in use.
 *
 * @return Used swap memory size in bytes
 */
auto getSwapMemoryUsed() -> unsigned long long;

/**
 * @brief Get the committed memory size
 *
 * Retrieves the amount of memory that has been committed by the system.
 * This is physical memory that has been allocated to processes.
 *
 * @return Committed memory size in bytes
 */
auto getCommittedMemory() -> size_t;

/**
 * @brief Get the uncommitted memory size
 *
 * Retrieves the amount of memory that is available for commitment.
 * This is physical memory that is available but not yet allocated.
 *
 * @return Uncommitted memory size in bytes
 */
auto getUncommittedMemory() -> size_t;

/**
 * @brief Get comprehensive memory statistics
 *
 * Retrieves detailed information about the system's memory including
 * physical memory, virtual memory, swap space, and various performance metrics.
 *
 * @return MemoryInfo structure containing comprehensive memory statistics
 */
auto getDetailedMemoryStats() -> MemoryInfo;

/**
 * @brief Get the peak working set size of the current process
 *
 * Retrieves the maximum amount of physical memory used by the current process
 * since it was started.
 *
 * @return Peak working set size in bytes
 */
auto getPeakWorkingSetSize() -> size_t;

/**
 * @brief Get the current working set size of the process
 *
 * Retrieves the current amount of physical memory used by the process.
 * The working set is the set of memory pages currently visible to the process.
 *
 * @return Current working set size in bytes
 */
auto getCurrentWorkingSetSize() -> size_t;

/**
 * @brief Get the page fault count
 *
 * Retrieves the number of page faults that have occurred in the system.
 * Page faults occur when a process accesses a memory page that is not
 * currently mapped into its address space.
 *
 * @return Number of page faults
 */
auto getPageFaultCount() -> size_t;

/**
 * @brief Get memory load percentage
 *
 * Retrieves the percentage of memory currently in use by the system.
 * This is a normalized value indicating the overall memory pressure.
 *
 * @return Memory load as a percentage (0-100)
 */
auto getMemoryLoadPercentage() -> double;

/**
 * @brief Get memory performance metrics
 *
 * Retrieves detailed performance metrics for memory including read/write
 * speeds, bandwidth usage, and latency.
 *
 * @return MemoryPerformance structure containing performance metrics
 */
auto getMemoryPerformance() -> MemoryPerformance;

/**
 * @brief Start memory monitoring
 *
 * Initiates memory monitoring and invokes the provided callback function
 * with updated memory information.
 *
 * @param callback Function to be called with memory information updates
 */
auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback) -> void;

/**
 * @brief Stop memory monitoring
 *
 * Stops the ongoing memory monitoring process.
 */
auto stopMemoryMonitoring() -> void;

/**
 * @brief Get memory timeline
 *
 * Retrieves a timeline of memory statistics over a specified duration.
 *
 * @param duration Duration for which memory statistics are collected
 * @return Vector of MemoryInfo objects representing the memory timeline
 */
auto getMemoryTimeline(std::chrono::minutes duration) -> std::vector<MemoryInfo>;

/**
 * @brief Detect memory leaks
 *
 * Analyzes the system for potential memory leaks and returns a list of
 * detected issues.
 *
 * @return Vector of strings describing detected memory leaks
 */
auto detectMemoryLeaks() -> std::vector<std::string>;

/**
 * @brief Get memory fragmentation percentage
 *
 * Calculates the percentage of memory fragmentation in the system.
 *
 * @return Memory fragmentation percentage
 */
auto getMemoryFragmentation() -> double;

/**
 * @brief Optimize memory usage
 *
 * Attempts to optimize memory usage by defragmenting and reallocating resources.
 *
 * @return Boolean indicating success or failure of optimization
 */
auto optimizeMemoryUsage() -> bool;

/**
 * @brief Analyze memory bottlenecks
 *
 * Identifies potential bottlenecks in memory usage and provides suggestions
 * for improvement.
 *
 * @return Vector of strings describing memory bottlenecks
 */
auto analyzeMemoryBottlenecks() -> std::vector<std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_MEMORY_HPP
