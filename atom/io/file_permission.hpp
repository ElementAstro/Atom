/**
 * @file file_permission.hpp
 * @brief Defines utilities for managing and comparing file permissions.
 *
 * This file provides functions to get file permissions, get the current
 * process's permissions, compare them, and change file permissions. It
 * leverages C++20 concepts and modern C++ features for robust and type-safe
 * operations.
 */
#ifndef ATOM_IO_FILE_PERMISSION_HPP
#define ATOM_IO_FILE_PERMISSION_HPP

#include <concepts>
#include <filesystem>
#include <optional>
#include <string_view>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

/**
 * @brief Concept for types that can be converted to a filesystem path.
 *
 * This concept ensures that a type `T` is either convertible to
 * `std::filesystem::path` or `std::string_view`, which can then be used
 * to construct a `std::filesystem::path`.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path> ||
                   std::convertible_to<T, std::string_view>;

/**
 * @brief Compares the permissions of a given file with the permissions of the
 * current process.
 *
 * This function checks if the current process has at least the same permissions
 * as the specified file.
 *
 * @param filePath A string_view representing the path to the file.
 * @return An std::optional<bool> containing:
 *         - `true` if the current process has permissions greater than or equal
 * to the file's permissions.
 *         - `false` if the current process has lesser permissions than the
 * file.
 *         - `std::nullopt` if an error occurs (e.g., file not found, unable to
 * get permissions).
 * @noexcept This function is declared noexcept, meaning it will not throw
 * exceptions directly. Error conditions are indicated by the std::optional
 * return value.
 */
auto compareFileAndSelfPermissions(std::string_view filePath) noexcept
    -> std::optional<bool>;

/**
 * @brief Helper function to compare file and self permissions using PathLike
 * types.
 *
 * This template function provides a convenient way to call
 * `compareFileAndSelfPermissions` with various path-like types (e.g.,
 * `std::string`, `const char*`, `std::filesystem::path`).
 *
 * @tparam T A type satisfying the PathLike concept.
 * @param filePath The path to the file.
 * @return An std::optional<bool> as described in the primary
 * `compareFileAndSelfPermissions` function.
 * @noexcept This function is declared noexcept.
 *
 * @see compareFileAndSelfPermissions(std::string_view)
 */
template <PathLike T>
auto compareFileAndSelfPermissions(const T &filePath) noexcept
    -> std::optional<bool> {
    return compareFileAndSelfPermissions(
        std::filesystem::path(filePath).string());
}

/**
 * @brief Retrieves the permission string for a specified file.
 *
 * The permission string is typically in a format like "rwxr-xr-x".
 *
 * @param filePath A string_view representing the path to the file.
 * @return A std::string containing the file's permissions.
 *         Returns an empty string if an error occurs (e.g., file not found).
 * @noexcept This function is declared noexcept.
 */
std::string getFilePermissions(std::string_view filePath) noexcept;

/**
 * @brief Retrieves the permission string for the current running process.
 *
 * This function determines the effective permissions of the process itself.
 * The format of the permission string is similar to file permissions (e.g.,
 * "rwxr-xr-x").
 *
 * @return A std::string containing the current process's permissions.
 *         Returns an empty string if an error occurs.
 * @noexcept This function is declared noexcept.
 */
std::string getSelfPermissions() noexcept;

/**
 * @brief Changes the permissions of a file.
 *
 * This function attempts to set the file's permissions to the specified mode.
 *
 * @param filePath The `std::filesystem::path` to the file whose permissions are
 * to be changed.
 * @param permissions The new permissions string (e.g., "rwxr-xr-x" or an octal
 * string like "755"). The exact interpretation of this string might depend on
 * the underlying OS implementation.
 * @throws std::runtime_error if the permissions cannot be changed (e.g.,
 * insufficient privileges, file not found, invalid permission string).
 */
void changeFilePermissions(
    const std::filesystem::path &filePath,
    const atom::containers::String &permissions);  // Use String alias

}  // namespace atom::io

#endif  // ATOM_IO_FILE_PERMISSION_HPP