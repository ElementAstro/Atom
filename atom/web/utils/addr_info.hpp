/*
 * addr_info.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: Address information handling functions

**************************************************/

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
 *
 * @code
 * struct addrinfo* src = ...;
 * std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> dst(nullptr,
 * freeaddrinfo); if (atom::web::dumpAddrInfo(&dst, src) == 0) { std::cout <<
 * "Address information dumped successfully." << std::endl; } else { std::cout
 * << "Failed to dump address information." << std::endl;
 * }
 * @endcode
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
 * @code
 * struct addrinfo* addrInfo = ...;
 * std::string addrStr = atom::web::addrInfoToString(addrInfo, true);
 * std::cout << addrStr << std::endl;
 * @endcode
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
 * @throws std::invalid_argument if hostname or service is empty
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     std::cout << "Address information retrieved successfully." << std::endl;
 *     // No need to manually free, handled by smart pointer
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
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
 *
 * @code
 * struct addrinfo* addrInfo1 = ...;
 * struct addrinfo* addrInfo2 = ...;
 * if (atom::web::compareAddrInfo(addrInfo1, addrInfo2)) {
 *     std::cout << "Address information structures are equal." << std::endl;
 * } else {
 *     std::cout << "Address information structures are not equal." <<
 * std::endl;
 * }
 * @endcode
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
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     auto filtered = atom::web::filterAddrInfo(addrInfo.get(), AF_INET);
 *     if (filtered) {
 *         std::cout << "Filtered address information retrieved successfully."
 * << std::endl; } else { std::cout << "No address information matched the
 * filter." << std::endl;
 *     }
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
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
 *
 * @code
 * try {
 *     auto addrInfo = atom::web::getAddrInfo("www.google.com", "http");
 *     auto sorted = atom::web::sortAddrInfo(addrInfo.get());
 *     if (sorted) {
 *         std::cout << "Sorted address information retrieved successfully." <<
 * std::endl; } else { std::cout << "Failed to sort address information." <<
 * std::endl;
 *     }
 * } catch (const std::exception& e) {
 *     std::cout << "Error: " << e.what() << std::endl;
 * }
 * @endcode
 */
auto sortAddrInfo(const struct addrinfo* addrInfo)
    -> std::unique_ptr<struct addrinfo, decltype(&::freeaddrinfo)>;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_ADDR_INFO_HPP
