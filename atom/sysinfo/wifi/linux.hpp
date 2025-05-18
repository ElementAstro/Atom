/*
 * linux.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Linux WiFi Implementation

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_LINUX_HPP
#define ATOM_SYSTEM_MODULE_WIFI_LINUX_HPP

#ifdef __linux__

#include "common.hpp"
#include "wifi.hpp"

namespace atom::system::linux {

// Platform-specific implementations for Linux
auto getCurrentWifi_impl() -> std::string;
auto getCurrentWiredNetwork_impl() -> std::string;
auto isHotspotConnected_impl() -> bool;
auto getHostIPs_impl() -> std::vector<std::string>;
auto getInterfaceNames_impl() -> std::vector<std::string>;
auto getNetworkStats_impl() -> NetworkStats;
auto isConnectedToInternet_impl() -> bool;
auto measurePing_impl(const std::string& host, int timeout) -> float;

} // namespace atom::system::linux

#endif // __linux__
#endif // ATOM_SYSTEM_MODULE_WIFI_LINUX_HPP
