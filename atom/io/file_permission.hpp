#ifndef ATOM_IO_FILE_PERMISSION_HPP
#define ATOM_IO_FILE_PERMISSION_HPP

#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

// Concept for filesystem path types
template <typename T>
concept PathLike = std::convertible_to<T, std::filesystem::path> ||
                   std::convertible_to<T, std::string_view>;

// Core permission comparison function
auto compareFileAndSelfPermissions(const std::string &filePath) noexcept
    -> std::optional<bool>;

// Helper functions with C++20 features
template <PathLike T>
auto compareFileAndSelfPermissions(const T &filePath) noexcept
    -> std::optional<bool> {
    return compareFileAndSelfPermissions(
        std::filesystem::path(filePath).string());
}

// Get file permissions string
std::string getFilePermissions(const std::string &filePath) noexcept;

// Get self permissions string
std::string getSelfPermissions() noexcept;

/**
 * @brief Changes the permissions of a file.
 *
 * @param filePath The path to the file.
 * @param permissions The new permissions string (e.g., "rwxr-xr-x").
 * @throws std::runtime_error if the permissions cannot be changed.
 */
void changeFilePermissions(
    const std::filesystem::path &filePath,
    const atom::containers::String &permissions);  // Use String alias

}  // namespace atom::io

#endif