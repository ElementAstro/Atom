/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: Common utility functions for sysinfo modules

**************************************************/

#ifndef ATOM_SYSINFO_COMMON_UTILS_HPP
#define ATOM_SYSINFO_COMMON_UTILS_HPP

#include <string>
#include <vector>
#include <optional>

namespace atom::sysinfo::common {

/**
 * @brief String utilities
 */
namespace string_utils {

/**
 * @brief Trim whitespace from string
 */
[[nodiscard]] auto trim(const std::string& str) -> std::string;

/**
 * @brief Split string by delimiter
 */
[[nodiscard]] auto split(const std::string& str, char delimiter) -> std::vector<std::string>;

/**
 * @brief Join strings with delimiter
 */
[[nodiscard]] auto join(const std::vector<std::string>& strings, const std::string& delimiter) -> std::string;

/**
 * @brief Convert string to lowercase
 */
[[nodiscard]] auto toLower(const std::string& str) -> std::string;

/**
 * @brief Convert string to uppercase
 */
[[nodiscard]] auto toUpper(const std::string& str) -> std::string;

/**
 * @brief Check if string starts with prefix
 */
[[nodiscard]] auto startsWith(const std::string& str, const std::string& prefix) -> bool;

/**
 * @brief Check if string ends with suffix
 */
[[nodiscard]] auto endsWith(const std::string& str, const std::string& suffix) -> bool;

} // namespace string_utils

/**
 * @brief File system utilities
 */
namespace fs_utils {

/**
 * @brief Check if file exists
 */
[[nodiscard]] auto fileExists(const std::string& path) -> bool;

/**
 * @brief Read entire file content
 */
[[nodiscard]] auto readFile(const std::string& path) -> std::optional<std::string>;

/**
 * @brief Read first line of file
 */
[[nodiscard]] auto readFirstLine(const std::string& path) -> std::optional<std::string>;

/**
 * @brief List files in directory
 */
[[nodiscard]] auto listFiles(const std::string& directory) -> std::vector<std::string>;

} // namespace fs_utils

/**
 * @brief System utilities
 */
namespace sys_utils {

/**
 * @brief Execute command and get output
 */
[[nodiscard]] auto executeCommand(const std::string& command) -> std::optional<std::string>;

/**
 * @brief Get environment variable
 */
[[nodiscard]] auto getEnvironmentVariable(const std::string& name) -> std::optional<std::string>;

/**
 * @brief Check if running as administrator/root
 */
[[nodiscard]] auto isElevated() -> bool;

} // namespace sys_utils

} // namespace atom::sysinfo::common

#endif // ATOM_SYSINFO_COMMON_UTILS_HPP
