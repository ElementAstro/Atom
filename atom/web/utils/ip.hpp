/*
 * ip.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: IP address validation and conversion

**************************************************/

#ifndef ATOM_WEB_UTILS_IP_HPP
#define ATOM_WEB_UTILS_IP_HPP

#include <string>

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
auto ipToString(const struct sockaddr* addr, char* strBuf, size_t bufSize) -> bool;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_IP_HPP
