/*
 * addr_info.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_WEB_UTILS_ADDR_INFO_HPP
#define ATOM_WEB_UTILS_ADDR_INFO_HPP

#include <memory>
#include <string>

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#if defined(__linux__) || defined(__APPLE__)
#include <netdb.h>
#elif defined(_WIN32)
#include <ws2tcpip.h>
#endif
#endif

namespace atom::web {

/**
 * @brief Dump address information from source to destination.
 *
 * This function copies address information from the source to the destination.
 *
 * @param dst Destination address information.
 * @param src Source address information.
 * @return `0` on success, `-1` on failure.
 *
 * @throws std::invalid_argument if src is nullptr
 * @throws std::runtime_error if memory allocation fails
 */
auto dumpAddrInfo(
    std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>& dst,
    const struct addrinfo* src) -> int;

/**
 * @brief Convert address information to string.
 *
 * This function converts address information to a string representation.
 *
 * @param addrInfo Address information.
 * @param jsonFormat If `true`, output in JSON format.
 * @return String representation of address information.
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 */
auto addrInfoToString(const struct addrinfo* addrInfo, bool jsonFormat = false)
    -> std::string;

/**
 * @brief Get address information for a given hostname and service.
 *
 * This function retrieves address information for a given hostname and service.
 *
 * @param hostname The hostname to resolve.
 * @param service The service to resolve.
 * @return Smart pointer to the address information.
 *
 * @throws std::runtime_error if getaddrinfo fails
 * @throws std::invalid_argument if hostname is empty
 */
auto getAddrInfo(const std::string& hostname, const std::string& service)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

/**
 * @brief Compare two address information structures.
 *
 * This function compares two address information structures for equality.
 *
 * @param addrInfo1 First address information structure.
 * @param addrInfo2 Second address information structure.
 * @return `true` if the structures are equal, `false` otherwise.
 *
 * @throws std::invalid_argument if either addrInfo1 or addrInfo2 is nullptr
 */
auto compareAddrInfo(const struct addrinfo* addrInfo1,
                     const struct addrinfo* addrInfo2) -> bool;

/**
 * @brief Filter address information by family.
 *
 * This function filters address information by the specified family.
 *
 * @param addrInfo Address information to filter.
 * @param family The family to filter by (e.g., AF_INET).
 * @return Filtered address information (smart pointer).
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 */
auto filterAddrInfo(const struct addrinfo* addrInfo, int family)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

/**
 * @brief Sort address information by family.
 *
 * This function sorts address information by family.
 *
 * @param addrInfo Address information to sort.
 * @return Sorted address information (smart pointer).
 *
 * @throws std::invalid_argument if addrInfo is nullptr
 */
auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_ADDR_INFO_HPP
