/*
 * disk_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Monitoring

**************************************************/

#ifndef ATOM_SYSTEM_DISK_MONITOR_HPP
#define ATOM_SYSTEM_DISK_MONITOR_HPP

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <vector>

#include "disk_types.hpp"

namespace atom::system {

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
 * @brief Event types for disk monitoring
 */
enum class DiskMonitorEvent {
    DEVICE_ADDED,
    DEVICE_REMOVED,
    DEVICE_CHANGED,
    PERFORMANCE_ALERT,
    HEALTH_WARNING,
    TEMPERATURE_ALERT,
    SPACE_WARNING
};

/**
 * @brief Structure for disk monitoring events
 */
struct DiskMonitorEventData {
    DiskMonitorEvent eventType;
    std::string devicePath;
    StorageDevice device;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    DiskMonitorEventData(DiskMonitorEvent type, const std::string& path,
                        const StorageDevice& dev, const std::string& msg = "")
        : eventType(type), devicePath(path), device(dev), message(msg),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Configuration for advanced disk monitoring
 */
struct AdvancedMonitorConfig {
    std::chrono::milliseconds pollInterval{std::chrono::seconds(1)};
    bool enableEventDriven{true};
    bool enablePerformanceMonitoring{true};
    bool enableHealthMonitoring{true};
    bool enableTemperatureMonitoring{true};
    bool enableSpaceMonitoring{true};
    float temperatureThreshold{70.0f};  // Celsius
    float spaceWarningThreshold{90.0f}; // Percentage
    SecurityPolicy securityPolicy{SecurityPolicy::DEFAULT};

    AdvancedMonitorConfig() = default;
};

/**
 * @brief Starts advanced event-driven monitoring for disk devices
 *
 * @param callback Function to call when events occur
 * @param config Configuration for monitoring behavior
 * @return A future that can be used to stop monitoring
 */
auto startAdvancedMonitoring(std::function<void(const DiskMonitorEventData&)> callback,
                            const AdvancedMonitorConfig& config = {}) -> std::future<void>;

/**
 * @brief Starts real-time performance monitoring for specific devices
 *
 * @param devicePaths Vector of device paths to monitor
 * @param callback Function to call with performance updates
 * @param interval Monitoring interval
 * @return A future that can be used to stop monitoring
 */
auto startPerformanceMonitoring(const std::vector<std::string>& devicePaths,
                               std::function<void(const std::string&, const DiskPerformanceMetrics&)> callback,
                               std::chrono::milliseconds interval = std::chrono::seconds(5)) -> std::future<void>;

/**
 * @brief Starts health monitoring for all devices
 *
 * @param callback Function to call when health status changes
 * @param interval Monitoring interval
 * @return A future that can be used to stop monitoring
 */
auto startHealthMonitoring(std::function<void(const std::string&, DiskHealthStatus, DiskHealthStatus)> callback,
                          std::chrono::minutes interval = std::chrono::minutes(5)) -> std::future<void>;

/**
 * @brief Class for comprehensive disk monitoring with event-driven architecture
 */
class DiskMonitor {
public:
    DiskMonitor();
    explicit DiskMonitor(const AdvancedMonitorConfig& config);
    ~DiskMonitor();

    // Disable copy constructor and assignment
    DiskMonitor(const DiskMonitor&) = delete;
    DiskMonitor& operator=(const DiskMonitor&) = delete;

    // Enable move constructor and assignment
    DiskMonitor(DiskMonitor&&) noexcept;
    DiskMonitor& operator=(DiskMonitor&&) noexcept;

    /**
     * @brief Start monitoring with event callback
     */
    void startMonitoring(std::function<void(const DiskMonitorEventData&)> callback);

    /**
     * @brief Stop all monitoring
     */
    void stopMonitoring();

    /**
     * @brief Check if monitoring is active
     */
    [[nodiscard]] bool isMonitoring() const;

    /**
     * @brief Update monitoring configuration
     */
    void updateConfig(const AdvancedMonitorConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const AdvancedMonitorConfig& getConfig() const;

    /**
     * @brief Add device to monitoring list
     */
    void addDevice(const std::string& devicePath);

    /**
     * @brief Remove device from monitoring list
     */
    void removeDevice(const std::string& devicePath);

    /**
     * @brief Get list of monitored devices
     */
    [[nodiscard]] std::vector<std::string> getMonitoredDevices() const;

    /**
     * @brief Get monitoring statistics
     */
    struct MonitorStats {
        size_t eventsProcessed{0};
        size_t devicesMonitored{0};
        std::chrono::system_clock::time_point startTime;
        std::chrono::milliseconds uptime{0};
    };

    [[nodiscard]] MonitorStats getStats() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_MONITOR_HPP
