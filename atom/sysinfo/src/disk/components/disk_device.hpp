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

#include <functional>
#include <future>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "disk_types.hpp"

namespace atom::system {

/**
 * @brief Retrieves the models of all connected storage devices.
 * @param includeRemovable Whether to include removable storage devices
 * @return A vector of StorageDevice structures
 */
[[nodiscard]] auto getStorageDevices(bool includeRemovable = true) -> std::vector<StorageDevice>;

/**
 * @brief Asynchronously retrieves all connected storage devices.
 * @param includeRemovable Whether to include removable storage devices
 * @return A future containing a vector of StorageDevice structures
 */
[[nodiscard]] auto getStorageDevicesAsync(bool includeRemovable = true) -> std::future<std::vector<StorageDevice>>;

/**
 * @brief Retrieves NVMe devices specifically.
 * @return A vector of NVMe StorageDevice structures
 */
[[nodiscard]] auto getNVMeDevices() -> std::vector<StorageDevice>;

/**
 * @brief Detects device interface type for a given device path.
 * @param devicePath Path to the device
 * @return The interface type of the device
 */
[[nodiscard]] auto detectDeviceInterfaceType(const std::string& devicePath) -> DiskInterfaceType;

/**
 * @brief Detects device type (HDD, SSD, etc.) for a given device path.
 * @param devicePath Path to the device
 * @return The type of the device
 */
[[nodiscard]] auto detectDeviceType(const std::string& devicePath) -> DiskType;

/**
 * @brief Retrieves device temperature if available.
 * @param devicePath Path to the device
 * @return Temperature in Celsius, or nullopt if not available
 */
[[nodiscard]] auto getDeviceTemperature(const std::string& devicePath) -> std::optional<float>;

/**
 * @brief Checks if a device supports hotplug.
 * @param devicePath Path to the device
 * @return True if device supports hotplug, false otherwise
 */
[[nodiscard]] auto isDeviceHotpluggable(const std::string& devicePath) -> bool;

/**
 * @brief Retrieves detailed device information including performance metrics.
 * @param devicePath Path to the device
 * @return Enhanced StorageDevice with all available information
 */
[[nodiscard]] auto getEnhancedDeviceInfo(const std::string& devicePath) -> std::optional<StorageDevice>;

/**
 * @brief Optimized device enumeration with caching and filtering.
 * @param filter Function to filter devices
 * @param useCache Whether to use cached results
 * @return Filtered vector of StorageDevice structures
 */
[[nodiscard]] auto getStorageDevicesFiltered(
    std::function<bool(const StorageDevice&)> filter,
    bool useCache = true) -> std::vector<StorageDevice>;

/**
 * @brief Monitors device hotplug events with callback.
 * @param onDeviceAdded Callback for device addition
 * @param onDeviceRemoved Callback for device removal
 * @return Future that can be used to stop monitoring
 */
[[nodiscard]] auto startHotplugMonitoring(
    std::function<void(const StorageDevice&)> onDeviceAdded,
    std::function<void(const std::string&)> onDeviceRemoved) -> std::future<void>;

/**
 * @brief Class for advanced device management with caching and monitoring.
 */
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // Disable copy constructor and assignment
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    // Enable move constructor and assignment
    DeviceManager(DeviceManager&&) noexcept;
    DeviceManager& operator=(DeviceManager&&) noexcept;

    /**
     * @brief Get all storage devices with intelligent caching
     */
    [[nodiscard]] auto getStorageDevices(bool includeRemovable = true) -> std::vector<StorageDevice>;

    /**
     * @brief Start monitoring for device changes
     */
    void startMonitoring(std::function<void(const StorageDevice&)> onDeviceAdded,
                        std::function<void(const std::string&)> onDeviceRemoved);

    /**
     * @brief Stop device monitoring
     */
    void stopMonitoring();

    /**
     * @brief Clear device cache
     */
    void clearCache();

    /**
     * @brief Get device by path
     */
    [[nodiscard]] auto getDevice(const std::string& devicePath) -> std::optional<StorageDevice>;

    /**
     * @brief Check if device exists
     */
    [[nodiscard]] auto deviceExists(const std::string& devicePath) -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Legacy function that returns pairs of device paths and models.
 * @return A vector of pairs where each pair consists of device path and model.
 */
[[nodiscard]] auto getStorageDeviceModels() -> std::vector<std::pair<std::string, std::string>>;

/**
 * @brief Retrieves a list of all available drives on the system.
 * @param includeRemovable Whether to include removable drives
 * @return A vector of strings where each string represents an available drive.
 */
[[nodiscard]] auto getAvailableDrives(bool includeRemovable = true) -> std::vector<std::string>;

/**
 * @brief Gets the serial number of a storage device
 * @param devicePath Path to the device
 * @return An optional string containing the serial number if available
 */
[[nodiscard]] auto getDeviceSerialNumber(const std::string& devicePath) -> std::optional<std::string>;

/**
 * @brief Gets disk health information if available
 * @param devicePath Path to the device
 * @return A variant containing either a health percentage or an error message
 */
[[nodiscard]] auto getDiskHealth(const std::string& devicePath) -> std::variant<int, std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_DEVICE_HPP
