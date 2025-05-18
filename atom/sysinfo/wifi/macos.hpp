/*
 * macos.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - macOS WiFi Implementation

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_MACOS_HPP
#define ATOM_SYSTEM_MODULE_WIFI_MACOS_HPP

#ifdef __APPLE__

#include "common.hpp"
#include "wifi.hpp"

namespace atom::system::macos {

// Platform-specific implementations for macOS
auto getCurrentWifi_impl() -> std::string;
auto getCurrentWiredNetwork_impl() -> std::string;
auto isHotspotConnected_impl() -> bool;
auto getHostIPs_impl() -> std::vector<std::string>;
auto getInterfaceNames_impl() -> std::vector<std::string>;
auto getNetworkStats_impl() -> NetworkStats;
auto isConnectedToInternet_impl() -> bool;

} // namespace atom::system::macos

#endif // __APPLE__
#endif // ATOM_SYSTEM_MODULE_WIFI_MACOS_HPP
