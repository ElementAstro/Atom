/*
 * user.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_USER_HPP
#define ATOM_SYSTEM_USER_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"

namespace atom::system {
/**
 * @brief Get user groups.
 * @return User groups.
 */
ATOM_NODISCARD auto getUserGroups() -> std::vector<std::wstring>;

/**
 * @brief Get the username of the current user.
 * @return Username.
 */
ATOM_NODISCARD auto getUsername() -> std::string;

/**
 * @brief Get the hostname of the system.
 * @return Hostname.
 */
ATOM_NODISCARD auto getHostname() -> std::string;

/**
 * @brief Get the user ID of the current user.
 * @return User ID.
 */
ATOM_NODISCARD auto getUserId() -> int;

/**
 * @brief Get the group ID of the current user.
 * @return Group ID.
 */
ATOM_NODISCARD auto getGroupId() -> int;

/**
 * @brief Get the home directory of the current user.
 * @return Home directory.
 */
ATOM_NODISCARD auto getHomeDirectory() -> std::string;

/**
 * @brief Get the current working directory.
 * @return Current working directory.
 */
ATOM_NODISCARD auto getCurrentWorkingDirectory() -> std::string;

/**
 * @brief Get the login shell of the current user.
 * @return Login shell.
 */
ATOM_NODISCARD auto getLoginShell() -> std::string;

#ifdef _WIN32
/**
 * @brief Get the user profile directory (Windows only).
 * @return User profile directory.
 */
auto getUserProfileDirectory() -> std::string;
#endif

/**
 * @brief Retrieves the login name of the user.
 * @return The login name of the user.
 */
auto getLogin() -> std::string;

/**
 * @brief Check whether the current user has root/administrator privileges.
 * @return true if the current user has root/administrator privileges.
 * @return false if the current user does not have root/administrator
 * privileges.
 */
auto isRoot() -> bool;

/**
 * @brief Get the value of an environment variable.
 * @param name The name of the environment variable.
 * @return The value of the environment variable or an empty string if not
 * found.
 */
ATOM_NODISCARD auto getEnvironmentVariable(const std::string& name)
    -> std::string;

/**
 * @brief Get all environment variables.
 * @return A map containing all environment variables.
 */
ATOM_NODISCARD auto getAllEnvironmentVariables()
    -> std::unordered_map<std::string, std::string>;

/**
 * @brief Set the value of an environment variable.
 * @param name The name of the environment variable.
 * @param value The value to set.
 * @return true if the environment variable was set successfully.
 * @return false if the environment variable could not be set.
 */
auto setEnvironmentVariable(const std::string& name, const std::string& value)
    -> bool;

/**
 * @brief Get the system uptime in seconds.
 * @return System uptime in seconds.
 */
ATOM_NODISCARD auto getSystemUptime() -> uint64_t;

/**
 * @brief Get the list of logged-in users.
 * @return A vector of logged-in user names.
 */
ATOM_NODISCARD auto getLoggedInUsers() -> std::vector<std::string>;

/**
 * @brief Check if a user exists.
 * @param username The username to check.
 * @return true if the user exists.
 * @return false if the user does not exist.
 */
ATOM_NODISCARD auto userExists(const std::string& username) -> bool;
}  // namespace atom::system

#endif
