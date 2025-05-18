/*
 * disk_info.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Information

**************************************************/

#ifndef ATOM_SYSTEM_DISK_INFO_HPP
#define ATOM_SYSTEM_DISK_INFO_HPP

#include <string>
#include <utility>
#include <vector>


#include "atom/sysinfo/disk/disk_types.hpp"

namespace atom::system {

/**
 * @brief Retrieves detailed disk information for all available disks.
 *
 * This function scans the system for all available disks and returns
 * detailed information for each one, including usage, filesystem type,
 * and device model information.
 *
 * @param includeRemovable Whether to include removable drives in the results
 * @return A vector of DiskInfo structures
 */
[[nodiscard]] auto getDiskInfo(bool includeRemovable = true)
    -> std::vector<DiskInfo>;

/**
 * @brief Retrieves the disk usage information for all available disks.
 *
 * This function is a simplified version that focuses only on getting disk paths
 * and usage. For more detailed information, use getDiskInfo() instead.
 *
 * @return A vector of pairs where each pair consists of:
 *         - A string representing the disk path.
 *         - A float representing the usage percentage of the disk.
 */
[[nodiscard]] auto getDiskUsage() -> std::vector<std::pair<std::string, float>>;

/**
 * @brief Retrieves the model of a specified drive.
 *
 * @param drivePath A string representing the path of the drive.
 * @return A string containing the model name of the drive.
 */
[[nodiscard]] auto getDriveModel(const std::string& drivePath) -> std::string;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_INFO_HPP
