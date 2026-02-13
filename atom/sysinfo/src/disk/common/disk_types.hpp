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

#include <chrono>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace atom::system {

// Forward declarations
[[nodiscard]] auto formatSize(uint64_t sizeBytes, bool useBinaryUnits = true) -> std::string;

/**
 * @brief Enum for disk health status
 */
enum class DiskHealthStatus : uint8_t {
    UNKNOWN = 0,
    HEALTHY = 1,
    WARNING = 2,
    CRITICAL = 3,
    FAILING = 4
};

/**
 * @brief Enum for disk interface type
 */
enum class DiskInterfaceType : uint8_t {
    UNKNOWN = 0,
    SATA = 1,
    NVME = 2,
    USB = 3,
    SCSI = 4,
    IDE = 5,
    THUNDERBOLT = 6
};

/**
 * @brief Enum for disk type
 */
enum class DiskType : uint8_t {
    UNKNOWN = 0,
    HDD = 1,      // Hard Disk Drive
    SSD = 2,      // Solid State Drive
    HYBRID = 3,   // Hybrid Drive (SSHD)
    OPTICAL = 4,  // CD/DVD/Blu-ray
    FLOPPY = 5,   // Floppy disk
    TAPE = 6      // Tape drive
};

/**
 * @brief Structure for disk performance metrics
 */
struct DiskPerformanceMetrics {
    uint64_t readOperations{0};     // Total read operations
    uint64_t writeOperations{0};    // Total write operations
    uint64_t readBytes{0};          // Total bytes read
    uint64_t writeBytes{0};         // Total bytes written
    uint32_t readLatencyMs{0};      // Average read latency in milliseconds
    uint32_t writeLatencyMs{0};     // Average write latency in milliseconds
    uint32_t queueDepth{0};         // Current queue depth
    float temperature{0.0f};        // Temperature in Celsius

    constexpr DiskPerformanceMetrics() noexcept = default;

    constexpr DiskPerformanceMetrics(uint64_t readOps, uint64_t writeOps,
                                   uint64_t readB, uint64_t writeB) noexcept
        : readOperations(readOps), writeOperations(writeOps),
          readBytes(readB), writeBytes(writeB) {}

    // Calculate IOPS (Input/Output Operations Per Second)
    [[nodiscard]] constexpr double calculateIOPS(std::chrono::seconds duration) const noexcept {
        if (duration.count() == 0) return 0.0;
        return static_cast<double>(readOperations + writeOperations) / duration.count();
    }

    // Calculate throughput in MB/s
    [[nodiscard]] constexpr double calculateThroughputMBps(std::chrono::seconds duration) const noexcept {
        if (duration.count() == 0) return 0.0;
        constexpr double MB = 1024.0 * 1024.0;
        return static_cast<double>(readBytes + writeBytes) / (duration.count() * MB);
    }

    auto operator<=>(const DiskPerformanceMetrics&) const = default;
};

/**
 * @brief Structure to represent disk information with enhanced features
 */
struct DiskInfo {
    std::string path;                    // Mount point or device path
    std::string devicePath;              // Physical device path
    std::string model;                   // Disk model
    std::string fsType;                  // File system type
    std::string serialNumber;            // Device serial number
    std::string firmwareVersion;         // Firmware version
    uint64_t totalSpace{0};              // Total space in bytes
    uint64_t freeSpace{0};               // Free space in bytes
    uint64_t availableSpace{0};          // Available space (may differ from free)
    float usagePercent{0.0f};            // Usage percentage
    bool isRemovable{false};             // Whether the disk is removable
    bool isEncrypted{false};             // Whether the disk is encrypted
    bool isReadOnly{false};              // Whether the disk is read-only
    DiskHealthStatus healthStatus{DiskHealthStatus::UNKNOWN};
    DiskInterfaceType interfaceType{DiskInterfaceType::UNKNOWN};
    DiskType diskType{DiskType::UNKNOWN};
    DiskPerformanceMetrics performance;  // Performance metrics
    std::chrono::system_clock::time_point lastUpdated{std::chrono::system_clock::now()};

    // Default constructor
    DiskInfo() noexcept = default;

    // Constructor with basic parameters
    DiskInfo(std::string_view pathView, std::string_view devicePathView,
                      uint64_t total, uint64_t free) noexcept
        : path(pathView), devicePath(devicePathView), totalSpace(total),
          freeSpace(free), availableSpace(free) {
        if (totalSpace > 0) {
            usagePercent = static_cast<float>((totalSpace - freeSpace) * 100.0 / totalSpace);
        }
    }

    // Move constructor
    DiskInfo(DiskInfo&&) noexcept = default;

    // Move assignment
    DiskInfo& operator=(DiskInfo&&) noexcept = default;

    // Copy constructor
    DiskInfo(const DiskInfo&) = default;

    // Copy assignment
    DiskInfo& operator=(const DiskInfo&) = default;

    // Destructor
    ~DiskInfo() = default;

    // Calculate used space
    [[nodiscard]] constexpr uint64_t getUsedSpace() const noexcept {
        return totalSpace > freeSpace ? totalSpace - freeSpace : 0;
    }

    // Check if disk is nearly full (>90% usage)
    [[nodiscard]] constexpr bool isNearlyFull() const noexcept {
        return usagePercent > 90.0f;
    }

    // Check if disk is healthy
    [[nodiscard]] constexpr bool isHealthy() const noexcept {
        return healthStatus == DiskHealthStatus::HEALTHY;
    }

    // Update usage percentage
    constexpr void updateUsagePercent() noexcept {
        if (totalSpace > 0) {
            usagePercent = static_cast<float>((totalSpace - freeSpace) * 100.0 / totalSpace);
        }
    }

    // Comparison operators
    auto operator<=>(const DiskInfo& other) const = default;
    bool operator==(const DiskInfo& other) const = default;
};

/**
 * @brief Structure to represent a storage device with enhanced features
 */
struct StorageDevice {
    std::string devicePath;              // Path to the device (e.g., /dev/sda)
    std::string model;                   // Device model
    std::string serialNumber;            // Serial number if available
    std::string firmwareVersion;         // Firmware version
    std::string vendor;                  // Device vendor
    uint64_t sizeBytes{0};               // Size in bytes
    uint32_t sectorSize{512};            // Sector size in bytes
    bool isRemovable{false};             // Whether the device is removable
    bool isHotpluggable{false};          // Whether the device supports hotplug
    bool supportsSmartData{false};       // Whether device supports SMART
    DiskHealthStatus healthStatus{DiskHealthStatus::UNKNOWN};
    DiskInterfaceType interfaceType{DiskInterfaceType::UNKNOWN};
    DiskType diskType{DiskType::UNKNOWN};
    DiskPerformanceMetrics performance;  // Performance metrics
    std::chrono::system_clock::time_point lastSeen{std::chrono::system_clock::now()};

    // Default constructor
    StorageDevice() noexcept = default;

    // Constructor with basic parameters
    StorageDevice(std::string_view devicePathView, std::string_view modelView,
                           uint64_t size) noexcept
        : devicePath(devicePathView), model(modelView), sizeBytes(size) {}

    // Move constructor
    StorageDevice(StorageDevice&&) noexcept = default;

    // Move assignment
    StorageDevice& operator=(StorageDevice&&) noexcept = default;

    // Copy constructor
    StorageDevice(const StorageDevice&) = default;

    // Copy assignment
    StorageDevice& operator=(const StorageDevice&) = default;

    // Destructor
    ~StorageDevice() = default;

    // Get size in human-readable format
    [[nodiscard]] std::string getFormattedSize() const {
        return formatSize(sizeBytes);
    }

    // Check if device is healthy
    [[nodiscard]] constexpr bool isHealthy() const noexcept {
        return healthStatus == DiskHealthStatus::HEALTHY;
    }

    // Get total number of sectors
    [[nodiscard]] constexpr uint64_t getTotalSectors() const noexcept {
        return sectorSize > 0 ? sizeBytes / sectorSize : 0;
    }

    // Check if device is SSD
    [[nodiscard]] constexpr bool isSSD() const noexcept {
        return diskType == DiskType::SSD || diskType == DiskType::HYBRID;
    }

    // Comparison operators
    auto operator<=>(const StorageDevice& other) const = default;
    bool operator==(const StorageDevice& other) const = default;
};

/**
 * @brief Enum for device security policies
 */
enum class SecurityPolicy : uint8_t {
    DEFAULT = 0,          // System default
    READ_ONLY = 1,        // Force read-only access
    SCAN_BEFORE_USE = 2,  // Scan for malware before allowing access
    WHITELIST_ONLY = 3,   // Only allow whitelisted devices
    QUARANTINE = 4,       // Isolate device content
    ENCRYPT_REQUIRED = 5, // Require encryption
    AUDIT_ALL = 6         // Audit all access
};

/**
 * @brief Structure for disk monitoring configuration
 */
struct DiskMonitorConfig {
    std::chrono::milliseconds pollInterval{std::chrono::seconds(2)};
    bool enablePerformanceMonitoring{true};
    bool enableHealthMonitoring{true};
    bool enableSecurityScanning{false};
    SecurityPolicy defaultSecurityPolicy{SecurityPolicy::DEFAULT};
    uint32_t maxCachedDevices{100};
    std::chrono::minutes cacheExpiration{std::chrono::minutes(5)};

    constexpr DiskMonitorConfig() noexcept = default;

    constexpr DiskMonitorConfig(std::chrono::milliseconds interval,
                               SecurityPolicy policy) noexcept
        : pollInterval(interval), defaultSecurityPolicy(policy) {}
};

/**
 * @brief Structure for disk operation result
 */
template<typename T>
struct DiskOperationResult {
    bool success{false};
    T data{};
    std::string errorMessage;
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};

    constexpr DiskOperationResult() noexcept = default;

    constexpr DiskOperationResult(bool success, T data) noexcept
        : success(success), data(std::move(data)) {}

    constexpr DiskOperationResult(bool success, std::string_view error) noexcept
        : success(success), errorMessage(error) {}

    [[nodiscard]] constexpr bool isSuccess() const noexcept { return success; }
    [[nodiscard]] constexpr bool hasError() const noexcept { return !success; }
    [[nodiscard]] const std::string& getError() const noexcept { return errorMessage; }
    [[nodiscard]] const T& getData() const noexcept { return data; }
};

// Type aliases for common operation results
using DiskInfoResult = DiskOperationResult<DiskInfo>;
using StorageDeviceResult = DiskOperationResult<StorageDevice>;
using DiskListResult = DiskOperationResult<std::vector<DiskInfo>>;
using DeviceListResult = DiskOperationResult<std::vector<StorageDevice>>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_TYPES_HPP
