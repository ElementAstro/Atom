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

#include <functional>
#include <future>

#include "atom/sysinfo/disk/disk_types.hpp"

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

}  // namespace atom::system

#endif  // ATOM_SYSTEM_DISK_MONITOR_HPP
