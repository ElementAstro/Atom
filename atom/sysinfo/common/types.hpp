/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-4

Description: Common types and definitions for sysinfo modules

**************************************************/

#ifndef ATOM_SYSINFO_COMMON_TYPES_HPP
#define ATOM_SYSINFO_COMMON_TYPES_HPP

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace atom::sysinfo::common {

/**
 * @brief Common error types for all sysinfo modules
 */
enum class SysInfoError {
    SUCCESS,
    ACCESS_DENIED,
    NOT_SUPPORTED,
    INVALID_DATA,
    SYSTEM_ERROR,
    TIMEOUT,
    UNKNOWN_ERROR
};

/**
 * @brief Convert error to string
 */
[[nodiscard]] auto errorToString(SysInfoError error) -> std::string;

/**
 * @brief Common result type template
 */
template<typename T>
using Result = std::variant<T, SysInfoError>;

/**
 * @brief Platform detection
 */
enum class Platform {
    WINDOWS,
    LINUX,
    MACOS,
    FREEBSD,
    UNKNOWN
};

/**
 * @brief Get current platform
 */
[[nodiscard]] auto getCurrentPlatform() -> Platform;

/**
 * @brief Convert platform to string
 */
[[nodiscard]] auto platformToString(Platform platform) -> std::string;

/**
 * @brief Common timestamp type
 */
using Timestamp = std::chrono::system_clock::time_point;

/**
 * @brief Get current timestamp
 */
[[nodiscard]] auto getCurrentTimestamp() -> Timestamp;

/**
 * @brief Convert timestamp to string
 */
[[nodiscard]] auto timestampToString(const Timestamp& timestamp) -> std::string;

} // namespace atom::sysinfo::common

#endif // ATOM_SYSINFO_COMMON_TYPES_HPP
