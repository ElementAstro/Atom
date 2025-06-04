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


#include "atom/log/loguru.hpp"

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

}  // namespace atom::system

#endif  // ATOM_SYSTEM_MODULE_MEMORY_COMMON_HPP
