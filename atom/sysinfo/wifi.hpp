/*
 * wifi.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Wifi Information

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_HPP
#define ATOM_SYSTEM_MODULE_WIFI_HPP

#include <string>
#include <vector>
#include <chrono>

#include "atom/macro.hpp"

namespace atom::system {
/**
 * @brief Get current wifi name
 * @return Current wifi name
 */
ATOM_NODISCARD auto getCurrentWifi() -> std::string;

/**
 * @brief Get current wired network name
 * @return Current wired network name
 */
ATOM_NODISCARD auto getCurrentWiredNetwork() -> std::string;

/**
 * @brief Check if hotspot is connected
 * @return True if hotspot is connected
 */
ATOM_NODISCARD auto isHotspotConnected() -> bool;

/*
 * @brief Get host IP addresses
 * @return Vector of host IP addresses
 */
ATOM_NODISCARD auto getHostIPs() -> std::vector<std::string>;

/**
 * @brief Get IPv4 addresses
 * @return Vector of IPv4 addresses
 */
ATOM_NODISCARD auto getIPv4Addresses() -> std::vector<std::string>;

/**
 * @brief Get IPv6 addresses
 * @return Vector of IPv6 addresses
 */
ATOM_NODISCARD auto getIPv6Addresses() -> std::vector<std::string>;

/**
 * @brief Get interface names
 * @return Vector of interface names
 */
ATOM_NODISCARD auto getInterfaceNames() -> std::vector<std::string>;

/**
 * @brief Network statistics structure
 */
struct NetworkStats {
    double downloadSpeed;    // MB/s
    double uploadSpeed;      // MB/s
    double latency;          // ms
    double packetLoss;       // Percentage
    double signalStrength;   // dBm
    std::vector<std::string> connectedDevices;
} ATOM_ALIGNAS(32);

/**
 * @brief Get current network statistics
 * @return Network statistics
 */
ATOM_NODISCARD auto getNetworkStats() -> NetworkStats;

/**
 * @brief Get network history over a duration
 * @param duration Duration in minutes
 * @return Vector of network statistics
 */
ATOM_NODISCARD auto getNetworkHistory(std::chrono::minutes duration) -> std::vector<NetworkStats>;

/**
 * @brief Scan available networks
 * @return Vector of available network names
 */
ATOM_NODISCARD auto scanAvailableNetworks() -> std::vector<std::string>;

/**
 * @brief Get network security information
 * @return Network security details
 */
ATOM_NODISCARD auto getNetworkSecurity() -> std::string;

/**
 * @brief Measure bandwidth
 * @return Pair of upload and download speeds
 */
ATOM_NODISCARD auto measureBandwidth() -> std::pair<double, double>;

/**
 * @brief Analyze network quality
 * @return Network quality analysis result
 */
ATOM_NODISCARD auto analyzeNetworkQuality() -> std::string;

/**
 * @brief Get connected devices
 * @return Vector of connected device names
 */
ATOM_NODISCARD auto getConnectedDevices() -> std::vector<std::string>;

}  // namespace atom::system

#endif
