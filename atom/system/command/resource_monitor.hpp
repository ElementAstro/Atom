/*
 * resource_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_RESOURCE_MONITOR_HPP
#define ATOM_SYSTEM_COMMAND_RESOURCE_MONITOR_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Resource usage limits
 */
struct ResourceLimits {
    std::chrono::milliseconds maxExecutionTime{300000}; // 5 minutes
    size_t maxMemoryUsage = 1024 * 1024 * 1024; // 1GB
    size_t maxOutputSize = 100 * 1024 * 1024;   // 100MB
    size_t maxConcurrentProcesses = 10;
    double maxCpuUsage = 80.0; // Percentage
};

/**
 * @brief Resource monitor for tracking system resource usage
 */
class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    /**
     * @brief Start monitoring system resources
     *
     * @param limits Resource limits to enforce
     * @param callback Function called when limits are exceeded
     */
    void startMonitoring(const ResourceLimits& limits,
                        std::function<void(const std::string&)> callback = nullptr);

    /**
     * @brief Stop resource monitoring
     */
    void stopMonitoring();

    /**
     * @brief Get current resource usage
     *
     * @return String containing formatted resource usage information
     */
    ATOM_NODISCARD auto getCurrentResourceUsage() const -> std::string;

    /**
     * @brief Check if resource limits are being exceeded
     *
     * @param limits Resource limits to check against
     * @return true if any limits are exceeded
     */
    ATOM_NODISCARD auto areResourceLimitsExceeded(const ResourceLimits& limits) const -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Create a global resource monitor instance
 *
 * @return Shared pointer to ResourceMonitor
 */
ATOM_NODISCARD auto getGlobalResourceMonitor() -> std::shared_ptr<ResourceMonitor>;

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_RESOURCE_MONITOR_HPP
