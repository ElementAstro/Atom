/*
 * windows.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP
#define ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP

#ifdef _WIN32

#include "common.hpp"
#include "wifi.hpp"

namespace atom::system::windows {

/**
 * @brief Get the current WiFi network name
 * @return Current WiFi SSID or empty string if not connected
 */
auto getCurrentWifi_impl() -> std::string;

/**
 * @brief Get the current wired network adapter name
 * @return Current wired network adapter description or empty string if not
 * connected
 */
auto getCurrentWiredNetwork_impl() -> std::string;

/**
 * @brief Check if connected to a mobile hotspot
 * @return True if connected to a hotspot, false otherwise
 */
auto isHotspotConnected_impl() -> bool;

/**
 * @brief Get all IP addresses of the local host
 * @return Vector of IP address strings (IPv4 and IPv6)
 */
auto getHostIPs_impl() -> std::vector<std::string>;

/**
 * @brief Get all network interface names
 * @return Vector of network interface friendly names
 */
auto getInterfaceNames_impl() -> std::vector<std::string>;

/**
 * @brief Get comprehensive network statistics
 * @return NetworkStats structure with speed, latency, and signal information
 */
auto getNetworkStats_impl() -> NetworkStats;

/**
 * @brief Check internet connectivity by connecting to a reliable host
 * @return True if internet is accessible, false otherwise
 */
auto isConnectedToInternet_impl() -> bool;

/**
 * @brief Measure ping latency to a specific host
 * @param host Target hostname or IP address
 * @param timeout Timeout in milliseconds
 * @return Ping latency in milliseconds, -1.0 if failed
 */
auto measurePing_impl(const std::string& host, int timeout) -> float;

}  // namespace atom::system::windows

#endif  // _WIN32
#endif  // ATOM_SYSTEM_MODULE_WIFI_WINDOWS_HPP
