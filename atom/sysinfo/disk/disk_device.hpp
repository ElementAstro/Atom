/*
 * disk_device.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Devices

**************************************************/

#ifndef ATOM_SYSTEM_DISK_DEVICE_HPP
#define ATOM_SYSTEM_DISK_DEVICE_HPP

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "atom/sysinfo/disk/disk_types.hpp"

namespace atom::system {

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
 * @brief Gets the serial number of a storage device
 *
 * @param devicePath Path to the device
 * @return An optional string containing the serial number if available
 */
[[nodiscard]] auto getDeviceSerialNumber(const std::string& devicePath)
    -> std::optional<std::string>;

/**
 * @brief Gets disk health information if available
 *
 * @param devicePath Path to the device
 * @return A variant containing either a health percentage or an error message
 */
[[nodiscard]] auto getDiskHealth(const std::string& devicePath)
    -> std::variant<int, std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_DEVICE_HPP
