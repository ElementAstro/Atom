/**
 * @file windows.hpp
 * @brief Windows-specific OS information functions
 *
 * This file contains Windows-specific implementations for retrieving
 * operating system information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_OS_WINDOWS_HPP
#define ATOM_SYSINFO_OS_WINDOWS_HPP

#ifdef _WIN32

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace atom::system {

struct OperatingSystemInfo;

/**
 * @brief Retrieves the computer name from the Windows system
 * @return Optional string containing the computer name, or nullopt if failed
 */
auto getComputerNameWindows() -> std::optional<std::string>;

/**
 * @brief Retrieves Windows-specific operating system information
 * @param osInfo Reference to OperatingSystemInfo to populate
 */
void getOperatingSystemInfoWindows(OperatingSystemInfo& osInfo);

/**
 * @brief Retrieves the system uptime on Windows
 * @return The system uptime as a duration in seconds
 */
auto getSystemUptimeWindows() -> std::chrono::seconds;

/**
 * @brief Retrieves the system timezone on Windows
 * @return The system timezone as a string
 */
auto getSystemTimeZoneWindows() -> std::string;

/**
 * @brief Retrieves the list of installed updates on Windows
 * @return A vector containing the names of installed updates
 */
auto getInstalledUpdatesWindows() -> std::vector<std::string>;

/**
 * @brief Retrieves the system language on Windows
 * @return The system language as a string
 */
auto getSystemLanguageWindows() -> std::string;

/**
 * @brief Retrieves the system encoding on Windows
 * @return The system encoding as a string
 */
auto getSystemEncodingWindows() -> std::string;

/**
 * @brief Checks if the Windows OS is a server edition
 * @return true if the operating system is a server edition, false otherwise
 */
auto isServerEditionWindows() -> bool;

}  // namespace atom::system

#endif  // _WIN32

#endif  // ATOM_SYSINFO_OS_WINDOWS_HPP
