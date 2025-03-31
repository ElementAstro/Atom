/*
 * os.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - OS Information

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_OS_HPP
#define ATOM_SYSTEM_MODULE_OS_HPP

#include <string>
#include <vector>
#include <chrono>

#include "atom/macro.hpp"

namespace atom::system {
/**
 * @brief Represents information about the operating system.
 */
struct OperatingSystemInfo {
    std::string osName;        /**< The name of the operating system. */
    std::string osVersion;     /**< The version of the operating system. */
    std::string kernelVersion; /**< The version of the kernel. */
    std::string architecture;  /**< The architecture of the operating system. */
    std::string
        compiler; /**< The compiler used to compile the operating system. */
    std::string computerName; /**< The name of the computer. */

    // 新增字段
    std::string bootTime;      /**< System boot time */
    std::string installDate;   /**< OS installation date */
    std::string lastUpdate;    /**< Last system update time */
    std::string timeZone;      /**< System timezone */
    std::string charSet;       /**< System character set */
    bool isServer;             /**< Whether the OS is server version */
    std::vector<std::string> installedUpdates; /**< List of installed updates */

    OperatingSystemInfo() = default;

    std::string toJson() const;

    // 新增格式化输出方法
    std::string toDetailedString() const;
    std::string toJsonString() const;
} ATOM_ALIGNAS(128);

/**
 * @brief Retrieves the information about the operating system.
 * @return The `OperatingSystemInfo` struct containing the operating system
 * information.
 */
OperatingSystemInfo getOperatingSystemInfo();

/**
 * @brief Checks if the operating system is running in a Windows Subsystem for
 * Linux (WSL) environment.
 * @return `true` if the operating system is running in a WSL environment,
 * `false` otherwise.
 */
auto isWsl() -> bool;

/**
 * @brief Retrieves the system uptime.
 * @return The system uptime as a duration in seconds.
 */
auto getSystemUptime() -> std::chrono::seconds;

/**
 * @brief Retrieves the last boot time of the system.
 * @return The last boot time as a string.
 */
auto getLastBootTime() -> std::string;

/**
 * @brief Retrieves the system timezone.
 * @return The system timezone as a string.
 */
auto getSystemTimeZone() -> std::string;

/**
 * @brief Retrieves the list of installed updates.
 * @return A vector containing the names of installed updates.
 */
auto getInstalledUpdates() -> std::vector<std::string>;

/**
 * @brief Checks for available updates.
 * @return A vector containing the names of available updates.
 */
auto checkForUpdates() -> std::vector<std::string>;

/**
 * @brief Retrieves the system language.
 * @return The system language as a string.
 */
auto getSystemLanguage() -> std::string;

/**
 * @brief Retrieves the system encoding.
 * @return The system encoding as a string.
 */
auto getSystemEncoding() -> std::string;

/**
 * @brief Checks if the operating system is a server edition.
 * @return `true` if the operating system is a server edition, `false`
 * otherwise.
 */
auto isServerEdition() -> bool;

}  // namespace atom::system

#endif
