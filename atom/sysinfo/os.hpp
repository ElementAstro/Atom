/**
 * @file os.hpp
 * @brief Operating System Information Module
 * 
 * This file contains definitions for retrieving comprehensive operating system
 * information across different platforms including Windows, Linux, and macOS.
 * 
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_MODULE_OS_HPP
#define ATOM_SYSTEM_MODULE_OS_HPP

#include <string>
#include <vector>
#include <chrono>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @struct OperatingSystemInfo
 * @brief Comprehensive information about the operating system
 * 
 * Contains detailed information about the operating system including
 * version details, architecture, boot time, and system configuration.
 */
struct OperatingSystemInfo {
    std::string osName;        /**< The name of the operating system */
    std::string osVersion;     /**< The version of the operating system */
    std::string kernelVersion; /**< The version of the kernel */
    std::string architecture;  /**< The architecture of the operating system */
    std::string compiler;      /**< The compiler used to compile the operating system */
    std::string computerName;  /**< The name of the computer */
    std::string bootTime;      /**< System boot time */
    std::string installDate;   /**< OS installation date */
    std::string lastUpdate;    /**< Last system update time */
    std::string timeZone;      /**< System timezone */
    std::string charSet;       /**< System character set */
    bool isServer;             /**< Whether the OS is server version */
    std::vector<std::string> installedUpdates; /**< List of installed updates */

    OperatingSystemInfo() = default;

    /**
     * @brief Converts the OS information to JSON format
     * @return JSON string representation of the OS information
     */
    std::string toJson() const;

    /**
     * @brief Converts the OS information to detailed string format
     * @return Detailed string representation of the OS information
     */
    std::string toDetailedString() const;

    /**
     * @brief Converts the OS information to JSON string format
     * @return JSON string representation of the OS information
     */
    std::string toJsonString() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Retrieves comprehensive information about the operating system
 * 
 * Queries the operating system for detailed information including name,
 * version, kernel details, architecture, and other system properties.
 * 
 * @return OperatingSystemInfo struct containing the operating system information
 */
OperatingSystemInfo getOperatingSystemInfo();

/**
 * @brief Checks if the operating system is running in a Windows Subsystem for Linux (WSL) environment
 * 
 * Detects whether the current environment is running under WSL by examining
 * system files and environment indicators.
 * 
 * @return true if the operating system is running in a WSL environment, false otherwise
 */
auto isWsl() -> bool;

/**
 * @brief Retrieves the system uptime
 * 
 * Calculates the duration since the system was last booted.
 * 
 * @return The system uptime as a duration in seconds
 */
auto getSystemUptime() -> std::chrono::seconds;

/**
 * @brief Retrieves the last boot time of the system
 * 
 * Determines when the system was last started by calculating the boot time
 * based on current time and uptime.
 * 
 * @return The last boot time as a formatted string
 */
auto getLastBootTime() -> std::string;

/**
 * @brief Retrieves the system timezone
 * 
 * Gets the current timezone configuration of the system.
 * 
 * @return The system timezone as a string
 */
auto getSystemTimeZone() -> std::string;

/**
 * @brief Retrieves the list of installed updates
 * 
 * Queries the system for a list of installed updates, patches, or packages
 * depending on the operating system.
 * 
 * @return A vector containing the names of installed updates
 */
auto getInstalledUpdates() -> std::vector<std::string>;

/**
 * @brief Checks for available updates
 * 
 * Queries the system or update repositories for available updates
 * that can be installed.
 * 
 * @return A vector containing the names of available updates
 */
auto checkForUpdates() -> std::vector<std::string>;

/**
 * @brief Retrieves the system language
 * 
 * Gets the primary language configured for the system.
 * 
 * @return The system language as a string
 */
auto getSystemLanguage() -> std::string;

/**
 * @brief Retrieves the system encoding
 * 
 * Gets the character encoding used by the system.
 * 
 * @return The system encoding as a string
 */
auto getSystemEncoding() -> std::string;

/**
 * @brief Checks if the operating system is a server edition
 * 
 * Determines whether the current OS installation is a server variant
 * or desktop/workstation variant.
 * 
 * @return true if the operating system is a server edition, false otherwise
 */
auto isServerEdition() -> bool;

}  // namespace atom::system

#endif
