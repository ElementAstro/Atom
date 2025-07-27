/**
 * @file linux.hpp
 * @brief Linux-specific OS information functions
 *
 * This file contains Linux-specific implementations for retrieving
 * operating system information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_OS_LINUX_HPP
#define ATOM_SYSINFO_OS_LINUX_HPP

#if defined(__linux__) || defined(__linux)

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace atom::system {

struct OperatingSystemInfo;

/**
 * @brief Retrieves the computer name from the Linux system
 * @return Optional string containing the computer name, or nullopt if failed
 */
auto getComputerNameLinux() -> std::optional<std::string>;

/**
 * @brief Retrieves Linux-specific operating system information
 * @param osInfo Reference to OperatingSystemInfo to populate
 */
void getOperatingSystemInfoLinux(OperatingSystemInfo& osInfo);

/**
 * @brief Retrieves the system uptime on Linux
 * @return The system uptime as a duration in seconds
 */
auto getSystemUptimeLinux() -> std::chrono::seconds;

/**
 * @brief Retrieves the system timezone on Linux
 * @return The system timezone as a string
 */
auto getSystemTimeZoneLinux() -> std::string;

/**
 * @brief Retrieves the list of installed updates on Linux
 * @return A vector containing the names of installed updates
 */
auto getInstalledUpdatesLinux() -> std::vector<std::string>;

/**
 * @brief Retrieves the system language on Linux
 * @return The system language as a string
 */
auto getSystemLanguageLinux() -> std::string;

/**
 * @brief Retrieves the system encoding on Linux
 * @return The system encoding as a string
 */
auto getSystemEncodingLinux() -> std::string;

}  // namespace atom::system

#endif  // defined(__linux__) || defined(__linux)

#endif  // ATOM_SYSINFO_OS_LINUX_HPP
