/**
 * @file file_permission.hpp
 * @brief Defines utilities for managing and comparing file permissions.
 *
 * This file provides functions to get file permissions, get the current
 * process's permissions, compare them, and change file permissions. It
 * leverages C++20 concepts and modern C++ features for robust and type-safe
 * operations.
 */
#pragma once

#include <concepts>
#include <filesystem>
#include <optional>
#include <string_view>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

/**
 * @brief Concept for types that can be converted to a filesystem path
 * @tparam T The type to check for path conversion compatibility
 */
template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path> ||
                   std::convertible_to<T, std::string_view>;

/**
 * @brief Compare file permissions with current process permissions
 * @param filePath Path to the file for permission comparison
 * @return Optional boolean indicating comparison result:
 *         - true: process has equal or greater permissions than file
 *         - false: process has lesser permissions than file
 *         - nullopt: error occurred during comparison
 * @noexcept Does not throw exceptions; errors indicated by return value
 */
auto compareFileAndSelfPermissions(std::string_view filePath) noexcept
    -> std::optional<bool>;

/**
 * @brief Template wrapper for comparing file and process permissions
 * @tparam T Type satisfying PathLike concept
 * @param filePath Path-like object representing the file path
 * @return Optional boolean as described in primary function
 * @noexcept Does not throw exceptions
 */
template <PathLike T>
auto compareFileAndSelfPermissions(const T &filePath) noexcept
    -> std::optional<bool> {
    return compareFileAndSelfPermissions(
        std::filesystem::path(filePath).string());
}

/**
 * @brief Retrieve file permissions as a readable string
 * @param filePath Path to the file
 * @return Permission string in format "rwxrwxrwx" or empty string on error
 * @noexcept Does not throw exceptions; errors indicated by empty return
 */
std::string getFilePermissions(std::string_view filePath) noexcept;

/**
 * @brief Retrieve current process permissions as a readable string
 * @return Permission string in format "rwxrwxrwx" or empty string on error
 * @noexcept Does not throw exceptions; errors indicated by empty return
 */
std::string getSelfPermissions() noexcept;

/**
 * @brief Modify file permissions using permission string
 * @param filePath Filesystem path to the target file
 * @param permissions Permission string in format "rwxrwxrwx"
 * @throws std::invalid_argument If permission string format is invalid
 * @throws std::runtime_error If file doesn't exist or permission change fails
 */
void changeFilePermissions(const std::filesystem::path &filePath,
                           const atom::containers::String &permissions);

}  // namespace atom::io