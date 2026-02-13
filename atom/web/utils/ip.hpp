/*
 * ip.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_WEB_UTILS_IP_HPP
#define ATOM_WEB_UTILS_IP_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sockaddr;

namespace atom::web {

/**
 * @brief Check if an IP address is a valid IPv4 address
 *
 * @param ipAddress The IP address to check
 * @return bool True if valid, false otherwise
 */
auto isValidIPv4(const std::string& ipAddress) -> bool;

/**
 * @brief Check if an IP address is a valid IPv6 address
 *
 * @param ipAddress The IP address to check
 * @return bool True if valid, false otherwise
 */
auto isValidIPv6(const std::string& ipAddress) -> bool;

/**
 * @brief Convert a sockaddr structure to a string representation
 *
 * @param addr The sockaddr structure to convert
 * @param strBuf Buffer to store the string representation
 * @param bufSize Size of the buffer
 * @return bool True if conversion succeeded, false otherwise
 */
auto ipToString(const struct sockaddr* addr, char* strBuf,
                size_t bufSize) -> bool;

/**
 * @brief Converts a sockaddr structure to an IP address string.
 * @param addr Pointer to the sockaddr structure.
 * @return The IP address as a string.
 */
auto ipToString(const struct sockaddr* addr) -> std::string;

/**
 * @brief Check if an IP address is in a CIDR range.
 * @param ip The IP address to check.
 * @param cidr The CIDR notation (e.g., "192.168.1.0/24").
 * @return True if the IP is in the range.
 */
[[nodiscard]] auto isIPInCIDR(std::string_view ip,
                              std::string_view cidr) -> bool;

/**
 * @brief Parse CIDR notation into IP and prefix length.
 * @param cidr The CIDR notation string.
 * @return Pair of IP string and prefix length, or nullopt on failure.
 */
[[nodiscard]] auto parseCIDR(std::string_view cidr)
    -> std::optional<std::pair<std::string, int>>;

/**
 * @brief Check if an IP address is a private/internal address.
 * @param ip The IP address to check.
 * @return True if private.
 */
[[nodiscard]] auto isPrivateIP(std::string_view ip) -> bool;

/**
 * @brief Check if an IP address is a loopback address.
 * @param ip The IP address to check.
 * @return True if loopback.
 */
[[nodiscard]] auto isLoopbackIP(std::string_view ip) -> bool;

/**
 * @brief Check if an IP address is a multicast address.
 * @param ip The IP address to check.
 * @return True if multicast.
 */
[[nodiscard]] auto isMulticastIP(std::string_view ip) -> bool;

/**
 * @brief Get all local IP addresses.
 * @param includeLoopback Whether to include loopback addresses.
 * @return Vector of local IP addresses.
 */
[[nodiscard]] auto getLocalIPAddresses(bool includeLoopback = false)
    -> std::vector<std::string>;

/**
 * @brief Normalize an IP address (expand IPv6, etc.).
 * @param ip The IP address to normalize.
 * @return Normalized IP address.
 */
[[nodiscard]] auto normalizeIP(std::string_view ip) -> std::string;

/**
 * @brief Compare two IP addresses.
 * @param ip1 First IP address.
 * @param ip2 Second IP address.
 * @return -1 if ip1 < ip2, 0 if equal, 1 if ip1 > ip2.
 */
[[nodiscard]] auto compareIP(std::string_view ip1, std::string_view ip2) -> int;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_IP_HPP
