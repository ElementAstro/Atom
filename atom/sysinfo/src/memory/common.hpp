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
#include <map>
#include <chrono>

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
 * @brief Advanced byte size formatting with custom options
 * @param bytes Number of bytes to format
 * @param binary Use binary (1024) or decimal (1000) units
 * @param precision Number of decimal places
 * @param showUnit Whether to include unit suffix
 * @return Formatted string
 */
auto formatByteSizeAdvanced(unsigned long long bytes, bool binary = true,
                           int precision = 2, bool showUnit = true) -> std::string;

/**
 * @brief Format memory bandwidth with appropriate units
 * @param bytesPerSecond Bandwidth in bytes per second
 * @return Formatted bandwidth string
 */
auto formatBandwidth(double bytesPerSecond) -> std::string;

/**
 * @brief Format time duration with appropriate units
 * @param duration Duration in nanoseconds
 * @return Formatted duration string
 */
auto formatDuration(std::chrono::nanoseconds duration) -> std::string;

/**
 * @brief Parse byte size string to bytes
 * @param sizeStr String like "1.5GB", "512MB", etc.
 * @return Number of bytes
 */
auto parseByteSize(const std::string& sizeStr) -> unsigned long long;

/**
 * @brief Benchmarks memory performance by measuring read/write throughput
 * @param testSizeBytes Size of test buffer in bytes (default: 1MB)
 * @return Memory throughput in MB/s
 */
auto benchmarkMemoryPerformance(size_t testSizeBytes = 1024 * 1024) -> double;

/**
 * @brief Enhanced memory benchmarking with statistical analysis
 * @param testSizeBytes Size of test buffer in bytes
 * @param iterations Number of benchmark iterations
 * @param pattern Test pattern (sequential, random, mixed)
 * @param warmup Whether to perform warmup
 * @return Map of benchmark statistics
 */
auto benchmarkMemoryAdvanced(size_t testSizeBytes, int iterations,
                           const std::string& pattern, bool warmup = true) -> std::map<std::string, double>;

/**
 * @brief Memory latency profiler with cache analysis
 * @param maxSize Maximum memory size to test
 * @return Map of memory size to average latency
 */
auto profileMemoryLatency(size_t maxSize = 64 * 1024 * 1024) -> std::map<size_t, double>;

/**
 * @brief System memory stress test
 * @param duration Duration to run stress test
 * @param maxMemoryMB Maximum memory to allocate in MB
 * @return Map of stress test results
 */
auto stressTestMemory(std::chrono::seconds duration, size_t maxMemoryMB = 1024) -> std::map<std::string, double>;

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

/**
 * @brief Get enhanced memory performance metrics
 * @return MemoryPerformance structure with comprehensive metrics
 */
auto getEnhancedMemoryPerformance() -> MemoryPerformance;

/**
 * @brief Benchmark memory performance with custom parameters
 * @param testSizeBytes Size of test buffer in bytes
 * @param iterations Number of test iterations
 * @param testPattern Test pattern (sequential, random, mixed)
 * @return MemoryPerformance structure with benchmark results
 */
auto benchmarkMemoryPerformance(size_t testSizeBytes, int iterations,
                               const std::string& testPattern) -> MemoryPerformance;

/**
 * @brief Get memory cache statistics
 * @return MemoryPerformance structure focused on cache metrics
 */
auto getMemoryCacheStats() -> MemoryPerformance;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
