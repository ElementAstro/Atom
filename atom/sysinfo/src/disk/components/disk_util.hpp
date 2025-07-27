/*
 * disk_util.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Utilities

**************************************************/

#ifndef ATOM_SYSTEM_DISK_UTIL_HPP
#define ATOM_SYSTEM_DISK_UTIL_HPP

#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <vector>

#include "atom/sysinfo/disk/disk_types.hpp"

namespace atom::system {

/**
 * @brief Calculates the disk usage percentage.
 *
 * @param totalSpace The total space on the disk, in bytes.
 * @param freeSpace The free (available) space on the disk, in bytes.
 * @return A double representing the disk usage percentage.
 */
[[nodiscard]] auto calculateDiskUsagePercentage(uint64_t totalSpace,
                                                uint64_t freeSpace) -> double;

/**
 * @brief Retrieves the file system type for a specified path.
 *
 * @param path A string representing the path to the disk or mount point.
 * @return A string containing the file system type.
 */
[[nodiscard]] auto getFileSystemType(const std::string& path) -> std::string;

/**
 * @brief Formats a size in bytes to human-readable format.
 *
 * @param sizeBytes The size in bytes to format.
 * @param useBinaryUnits Whether to use binary units (1024) or decimal units (1000).
 * @return A string containing the formatted size (e.g., "1.5 GB", "2.3 TiB").
 */
[[nodiscard]] auto formatSize(uint64_t sizeBytes, bool useBinaryUnits) -> std::string;

/**
 * @brief Asynchronously retrieves filesystem type for a path.
 *
 * @param path The path to check.
 * @return A future containing the filesystem type.
 */
[[nodiscard]] auto getFileSystemTypeAsync(const std::string& path) -> std::future<std::string>;

/**
 * @brief Benchmarks disk performance for a given path.
 *
 * @param path The path to benchmark.
 * @param testSizeMB Size of test data in megabytes.
 * @return Performance metrics including read/write speeds.
 */
[[nodiscard]] auto benchmarkDiskPerformance(const std::string& path, uint32_t testSizeMB = 100)
    -> DiskPerformanceMetrics;

/**
 * @brief Calculates disk usage percentage with SIMD optimization.
 *
 * @param diskInfos Vector of disk information to process.
 * @return Vector of usage percentages.
 */
[[nodiscard]] auto calculateDiskUsagePercentageBatch(const std::vector<DiskInfo>& diskInfos)
    -> std::vector<double>;

/**
 * @brief Gets cached filesystem type with automatic cache management.
 *
 * @param path The path to check.
 * @return Cached or newly computed filesystem type.
 */
[[nodiscard]] auto getFileSystemTypeCached(const std::string& path) -> std::string;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_UTIL_HPP
