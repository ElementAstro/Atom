/*
 * dns.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: DNS related utilities

**************************************************/

#ifndef ATOM_WEB_UTILS_DNS_HPP
#define ATOM_WEB_UTILS_DNS_HPP

#include <chrono>
#include <string>
#include <vector>

namespace atom::web {

/**
 * @brief Set the Time-To-Live for DNS cache entries
 * 
 * @param ttlSeconds The TTL in seconds
 */
void setDNSCacheTTL(std::chrono::seconds ttlSeconds);

/**
 * @brief Get IP addresses for a given hostname through DNS resolution
 *
 * @param hostname The hostname to resolve
 * @return std::vector<std::string> List of IP addresses
 */
auto getIPAddresses(const std::string& hostname) -> std::vector<std::string>;

/**
 * @brief Get all local IP addresses of the machine
 *
 * @return std::vector<std::string> List of local IP addresses
 */
auto getLocalIPAddresses() -> std::vector<std::string>;

/**
 * @brief Clear expired entries from the DNS cache
 */
void clearDNSCacheExpiredEntries();

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_DNS_HPP
