/*
 * disk.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_DISK_HPP
#define ATOM_SYSTEM_MODULE_DISK_HPP

#include <chrono>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace atom::system {

/**
 * @brief Structure to represent disk information
 */
struct DiskInfo {
    std::string path;        // Mount point or device path
    std::string devicePath;  // Physical device path
    std::string model;       // Disk model
    std::string fsType;      // File system type
    uint64_t totalSpace;     // Total space in bytes
    uint64_t freeSpace;      // Free space in bytes
    float usagePercent;      // Usage percentage
    bool isRemovable;        // Whether the disk is removable

    // Default constructor with initialization
    DiskInfo()
        : totalSpace(0), freeSpace(0), usagePercent(0.0f), isRemovable(false) {}
};

/**
 * @brief Structure to represent a storage device
 */
struct StorageDevice {
    std::string devicePath;    // Path to the device (e.g., /dev/sda)
    std::string model;         // Device model
    std::string serialNumber;  // Serial number if available
    uint64_t sizeBytes;        // Size in bytes
    bool isRemovable;          // Whether the device is removable

    // Default constructor with initialization
    StorageDevice() : sizeBytes(0), isRemovable(false) {}
};

/**
 * @brief Enum for device security policies
 */
enum class SecurityPolicy {
    DEFAULT,          // System default
    READ_ONLY,        // Force read-only access
    SCAN_BEFORE_USE,  // Scan for malware before allowing access
    WHITELIST_ONLY,   // Only allow whitelisted devices
    QUARANTINE        // Isolate device content
};

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

/**
 * @brief Retrieves the models of all connected storage devices.
 *
 * @param includeRemovable Whether to include removable storage devices
 * @return A vector of StorageDevice structures
 */
[[nodiscard]] auto getStorageDevices(bool includeRemovable = true)
    -> std::vector<StorageDevice>;

/**
 * @brief Legacy function that returns pairs of device paths and models.
 *
 * @return A vector of pairs where each pair consists of device path and model.
 */
[[nodiscard]] auto getStorageDeviceModels()
    -> std::vector<std::pair<std::string, std::string>>;

/**
 * @brief Retrieves a list of all available drives on the system.
 *
 * @param includeRemovable Whether to include removable drives
 * @return A vector of strings where each string represents an available drive.
 */
[[nodiscard]] auto getAvailableDrives(bool includeRemovable = true)
    -> std::vector<std::string>;

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
auto scanDiskForThreats(const std::string& path,
                        int scanDepth = 0) -> std::pair<bool, int>;

/**
 * @brief Starts monitoring for device insertion events
 *
 * @param callback Function to call when a device is inserted
 * @param securityPolicy Security policy to apply to new devices
 * @return A future that can be used to stop monitoring
 */
auto startDeviceMonitoring(std::function<void(const StorageDevice&)> callback,
                           SecurityPolicy securityPolicy =
                               SecurityPolicy::DEFAULT) -> std::future<void>;

/**
 * @brief Gets the serial number of a storage device
 *
 * @param devicePath Path to the device
 * @return An optional string containing the serial number if available
 */
[[nodiscard]] auto getDeviceSerialNumber(const std::string& devicePath)
    -> std::optional<std::string>;

/**
 * @brief Checks if a device is in the whitelist
 *
 * @param deviceIdentifier Device identifier to check
 * @return true if in whitelist, false otherwise
 */
[[nodiscard]] auto isDeviceInWhitelist(const std::string& deviceIdentifier)
    -> bool;

/**
 * @brief Gets disk health information if available
 *
 * @param devicePath Path to the device
 * @return A variant containing either a health percentage or an error message
 */
[[nodiscard]] auto getDiskHealth(const std::string& devicePath)
    -> std::variant<int, std::string>;

}  // namespace atom::system

#endif