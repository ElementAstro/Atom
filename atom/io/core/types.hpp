/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Shared types, concepts, and enumerations for the atom::io module.
 */

#ifndef ATOM_IO_CORE_TYPES_HPP
#define ATOM_IO_CORE_TYPES_HPP

#include <concepts>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::io {

namespace fs = std::filesystem;

using atom::containers::String;
using atom::containers::Vector;

/**
 * @brief Concept for types convertible to a filesystem path.
 */
template <typename T>
concept PathLike =
    std::convertible_to<T, fs::path> || std::convertible_to<T, std::string> ||
    std::convertible_to<T, std::string_view> ||
    std::convertible_to<T, const char*>;

/**
 * @brief Alias for PathLike, used in async I/O interfaces.
 */
template <typename T>
concept PathString = PathLike<T>;

/**
 * @brief Options for recursive directory creation/deletion.
 */
struct CreateDirectoriesOptions {
    bool verbose = true;
    bool dryRun = false;
    int delay = 0;
    std::function<bool(std::string_view)> filter = [](std::string_view) {
        return true;
    };
    std::function<void(std::string_view)> onCreate = [](std::string_view) {};
    std::function<void(std::string_view)> onDelete = [](std::string_view) {};
};

enum class PathType { NOT_EXISTS, REGULAR_FILE, DIRECTORY, SYMLINK, OTHER };

/**
 * @brief The option to check the file type.
 */
enum class FileOption { PATH, NAME };

}  // namespace atom::io

#endif  // ATOM_IO_CORE_TYPES_HPP
