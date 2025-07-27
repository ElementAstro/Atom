/**
 * @file common.hpp
 * @brief Common utilities and definitions for OS information module
 *
 * This file contains common utilities and helper functions used across
 * different platform implementations of the OS information module.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_OS_COMMON_HPP
#define ATOM_SYSINFO_OS_COMMON_HPP

#include <string>
#include <utility>
#include <vector>

namespace atom::system {

/**
 * @brief Parses a configuration file for OS information
 * @param filePath Path to the file to parse
 * @return Pair containing OS name and version
 */
auto parseFile(const std::string& filePath) -> std::pair<std::string, std::string>;

/**
 * @brief Checks if the operating system is running in a Windows Subsystem for
 * Linux (WSL) environment
 *
 * Detects whether the current environment is running under WSL by examining
 * system files and environment indicators.
 *
 * @return true if the operating system is running in a WSL environment, false
 * otherwise
 */
auto isWsl() -> bool;

/**
 * @brief Checks for available updates
 *
 * Queries the system or update repositories for available updates
 * that can be installed.
 *
 * @return A vector containing the names of available updates
 */
auto checkForUpdates() -> std::vector<std::string>;

}  // namespace atom::system

#endif  // ATOM_SYSINFO_OS_COMMON_HPP
