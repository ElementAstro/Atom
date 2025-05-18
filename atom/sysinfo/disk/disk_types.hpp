/*
 * disk_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Types

**************************************************/

#ifndef ATOM_SYSTEM_DISK_TYPES_HPP
#define ATOM_SYSTEM_DISK_TYPES_HPP

#include <cstdint>
#include <string>

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

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_TYPES_HPP
