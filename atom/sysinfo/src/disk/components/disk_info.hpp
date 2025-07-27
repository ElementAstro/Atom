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

#include <future>
#include <memory>
#include <optional>
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
[[nodiscard]] auto getDiskInfo(bool includeRemovable = true) -> std::vector<DiskInfo>;

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
 * @brief Asynchronously retrieves detailed disk information for all available disks.
 *
 * @param includeRemovable Whether to include removable drives in the results
 * @return A future containing a vector of DiskInfo structures
 */
[[nodiscard]] auto getDiskInfoAsync(bool includeRemovable = true) -> std::future<std::vector<DiskInfo>>;

/**
 * @brief Retrieves disk information with advanced caching and optimization.
 *
 * @param includeRemovable Whether to include removable drives
 * @param forceRefresh Whether to force cache refresh
 * @return A vector of DiskInfo structures
 */
[[nodiscard]] auto getDiskInfoOptimized(bool includeRemovable = true, bool forceRefresh = false)
    -> std::vector<DiskInfo>;

/**
 * @brief Retrieves SMART data for a specific disk.
 *
 * @param devicePath Path to the disk device
 * @return Optional DiskPerformanceMetrics with SMART data
 */
[[nodiscard]] auto getDiskSmartData(const std::string& devicePath)
    -> std::optional<DiskPerformanceMetrics>;

/**
 * @brief Monitors disk health status for a specific device.
 *
 * @param devicePath Path to the disk device
 * @return Current health status of the disk
 */
[[nodiscard]] auto getDiskHealthStatus(const std::string& devicePath) -> DiskHealthStatus;

/**
 * @brief Retrieves disk temperature if available.
 *
 * @param devicePath Path to the disk device
 * @return Temperature in Celsius, or nullopt if not available
 */
[[nodiscard]] auto getDiskTemperature(const std::string& devicePath) -> std::optional<float>;

/**
 * @brief Batch retrieval of disk information with memory optimization.
 *
 * @param paths Vector of paths to check
 * @param includeRemovable Whether to include removable drives
 * @return Vector of DiskInfo structures
 */
[[nodiscard]] auto getDiskInfoBatch(const std::vector<std::string>& paths,
                                   bool includeRemovable = true) -> std::vector<DiskInfo>;

/**
 * @brief Class for advanced disk information management with caching.
 */
class DiskInfoManager {
public:
    DiskInfoManager();
    ~DiskInfoManager();

    // Disable copy constructor and assignment
    DiskInfoManager(const DiskInfoManager&) = delete;
    DiskInfoManager& operator=(const DiskInfoManager&) = delete;

    // Enable move constructor and assignment
    DiskInfoManager(DiskInfoManager&&) noexcept;
    DiskInfoManager& operator=(DiskInfoManager&&) noexcept;

    /**
     * @brief Get disk information with intelligent caching
     */
    [[nodiscard]] auto getDiskInfo(bool includeRemovable = true) -> std::vector<DiskInfo>;

    /**
     * @brief Get disk information asynchronously
     */
    [[nodiscard]] auto getDiskInfoAsync(bool includeRemovable = true) -> std::future<std::vector<DiskInfo>>;

    /**
     * @brief Clear all cached data
     */
    void clearCache();

    /**
     * @brief Set cache expiration time
     */
    void setCacheExpiration(std::chrono::minutes expiration);

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t hitCount{0};
        size_t missCount{0};
        size_t entryCount{0};
        std::chrono::minutes expiration{std::chrono::minutes(5)};
    };

    [[nodiscard]] auto getCacheStats() const -> CacheStats;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_INFO_HPP
