/**
 * @file macos.hpp
 * @brief macOS-specific OS information functions
 *
 * This file contains macOS-specific implementations for retrieving
 * operating system information.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_OS_MACOS_HPP
#define ATOM_SYSINFO_OS_MACOS_HPP

#ifdef __APPLE__

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace atom::system {

struct OperatingSystemInfo;

/**
 * @brief Retrieves the computer name from the macOS system
 * @return Optional string containing the computer name, or nullopt if failed
 */
auto getComputerNameMacOS() -> std::optional<std::string>;

/**
 * @brief Retrieves macOS-specific operating system information
 * @param osInfo Reference to OperatingSystemInfo to populate
 */
void getOperatingSystemInfoMacOS(OperatingSystemInfo& osInfo);

/**
 * @brief Retrieves the system uptime on macOS
 * @return The system uptime as a duration in seconds
 */
auto getSystemUptimeMacOS() -> std::chrono::seconds;

/**
 * @brief Retrieves the system timezone on macOS
 * @return The system timezone as a string
 */
auto getSystemTimeZoneMacOS() -> std::string;

/**
 * @brief Retrieves the list of installed updates on macOS
 * @return A vector containing the names of installed updates
 */
auto getInstalledUpdatesMacOS() -> std::vector<std::string>;

/**
 * @brief Retrieves the system language on macOS
 * @return The system language as a string
 */
auto getSystemLanguageMacOS() -> std::string;

/**
 * @brief Retrieves the system encoding on macOS
 * @return The system encoding as a string
 */
auto getSystemEncodingMacOS() -> std::string;

}  // namespace atom::system

#endif  // __APPLE__

#endif  // ATOM_SYSINFO_OS_MACOS_HPP
