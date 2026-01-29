/**
 * @file common.hpp
 * @brief Common definitions for memory information module
 *
 * This file contains common definitions and utilities used by the memory
 * information module across different platforms.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
#define ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace atom::system {

struct MemoryInfo;
struct MemoryPerformance;

namespace internal {

extern std::atomic<bool> g_monitoringActive;

/**
 * @brief Converts bytes to human-readable size string (KB, MB, GB, etc.)
 * @param bytes Number of bytes to convert
 * @return Formatted size string with appropriate unit
 */
auto formatByteSize(unsigned long long bytes) -> std::string;

/**
 * @brief Benchmarks memory performance by measuring read/write throughput
 * @param testSizeBytes Size of test buffer in bytes (default: 1MB)
 * @return Memory throughput in MB/s
 */
auto benchmarkMemoryPerformance(size_t testSizeBytes = 1024 * 1024) -> double;

}  // namespace internal

/**
 * @brief Starts continuous memory monitoring with callback
 * @param callback Function to be called with memory information updates
 */
auto startMemoryMonitoring(std::function<void(const MemoryInfo&)> callback)
    -> void;

/**
 * @brief Stops memory monitoring
 */
auto stopMemoryMonitoring() -> void;

/**
 * @brief Collects memory usage timeline over specified duration
 * @param duration Time period to collect samples
 * @return Vector of memory information samples
 */
auto getMemoryTimeline(std::chrono::minutes duration)
    -> std::vector<MemoryInfo>;

/**
 * @brief Performs basic memory leak detection
 * @return Vector of potential memory leak descriptions
 */
auto detectMemoryLeaks() -> std::vector<std::string>;

/**
 * @brief Calculates memory fragmentation percentage
 * @return Fragmentation percentage (0-100)
 */
auto getMemoryFragmentation() -> double;

/**
 * @brief Attempts to optimize memory usage
 * @return True if optimization was successful
 */
auto optimizeMemoryUsage() -> bool;

/**
 * @brief Analyzes system for memory bottlenecks
 * @return Vector of bottleneck descriptions
 */
auto analyzeMemoryBottlenecks() -> std::vector<std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
