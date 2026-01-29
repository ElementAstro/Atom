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

#include <cstdint>
#include <string>

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

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_UTIL_HPP
