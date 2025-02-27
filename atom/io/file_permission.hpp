#ifndef ATOM_IO_FILE_PERMISSION_HPP
#define ATOM_IO_FILE_PERMISSION_HPP

#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

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

}  // namespace atom::io

#endif