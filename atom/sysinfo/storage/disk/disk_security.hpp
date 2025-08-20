/*
 * disk_security.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Security

**************************************************/

#ifndef ATOM_SYSTEM_DISK_SECURITY_HPP
#define ATOM_SYSTEM_DISK_SECURITY_HPP

#include <string>
#include <utility>

namespace atom::system {

/**
 * @brief Adds a device to the security whitelist
 *
 * @param deviceIdentifier Device identifier (serial number, UUID, etc.)
 * @return true if successful, false otherwise
 */
auto addDeviceToWhitelist(const std::string& deviceIdentifier) -> bool;

/**
 * @brief Removes a device from the security whitelist
 *
 * @param deviceIdentifier Device identifier (serial number, UUID, etc.)
 * @return true if successful, false otherwise
 */
auto removeDeviceFromWhitelist(const std::string& deviceIdentifier) -> bool;

/**
 * @brief Checks if a device is in the whitelist
 *
 * @param deviceIdentifier Device identifier to check
 * @return true if in whitelist, false otherwise
 */
[[nodiscard]] auto isDeviceInWhitelist(const std::string& deviceIdentifier)
    -> bool;

/**
 * @brief Sets a disk to read-only mode for security
 *
 * @param path The path to the disk or mount point
 * @return true if successful, false otherwise
 */
auto setDiskReadOnly(const std::string& path) -> bool;

/**
 * @brief Scans a disk for malicious files
 *
 * @param path The path to the disk or mount point
 * @param scanDepth How many directory levels to scan (0 for unlimited)
 * @return A pair containing success status and number of suspicious files found
 */
auto scanDiskForThreats(const std::string& path, int scanDepth = 0)
    -> std::pair<bool, int>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_SECURITY_HPP
