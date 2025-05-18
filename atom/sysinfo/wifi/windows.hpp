/*
 * windows.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Windows WiFi Implementation

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP
#define ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP

#ifdef _WIN32

#include "common.hpp"
#include "wifi.hpp"

namespace atom::system::windows {

// Platform-specific implementations for Windows
auto getCurrentWifi_impl() -> std::string;
auto getCurrentWiredNetwork_impl() -> std::string;
auto isHotspotConnected_impl() -> bool;
auto getHostIPs_impl() -> std::vector<std::string>;
auto getInterfaceNames_impl() -> std::vector<std::string>;
auto getNetworkStats_impl() -> NetworkStats;
auto isConnectedToInternet_impl() -> bool;
auto measurePing_impl(const std::string& host, int timeout) -> float;

} // namespace atom::system::windows

#endif // _WIN32
#endif // ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP
