/*
 * env_path.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: PATH environment variable management

**************************************************/

#ifndef ATOM_SYSTEM_ENV_PATH_HPP
#define ATOM_SYSTEM_ENV_PATH_HPP

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief PATH environment variable management
 */
class EnvPath {
public:
    /**
     * @brief Adds a path to the PATH environment variable
     * @param path Path to add
     * @param prepend Whether to add to the beginning (default adds to end)
     * @return True if successfully added, otherwise false
     */
    static auto addToPath(const String& path, bool prepend = false) -> bool;

    /**
     * @brief Removes a path from the PATH environment variable
     * @param path Path to remove
     * @return True if successfully removed, otherwise false
     */
    static auto removeFromPath(const String& path) -> bool;

    /**
     * @brief Checks if a path is in the PATH environment variable
     * @param path Path to check
     * @return True if in PATH, otherwise false
     */
    ATOM_NODISCARD static auto isInPath(const String& path) -> bool;

    /**
     * @brief Gets all paths in the PATH environment variable
     * @return Vector containing all paths
     */
    ATOM_NODISCARD static auto getPathEntries() -> Vector<String>;

    /**
     * @brief Gets the PATH separator character for the current platform
     * @return Path separator character (';' on Windows, ':' on Unix-like)
     */
    ATOM_NODISCARD static auto getPathSeparator() -> char;

    /**
     * @brief Splits a PATH string into individual paths
     * @param pathStr The PATH string to split
     * @return Vector of individual paths
     */
    ATOM_NODISCARD static auto splitPathString(const String& pathStr) -> Vector<String>;

    /**
     * @brief Joins individual paths into a PATH string
     * @param paths Vector of paths to join
     * @return Joined PATH string
     */
    ATOM_NODISCARD static auto joinPathString(const Vector<String>& paths) -> String;

    /**
     * @brief Normalizes a path (removes duplicates, cleans up separators)
     * @param path The path to normalize
     * @return Normalized path
     */
    ATOM_NODISCARD static auto normalizePath(const String& path) -> String;

    /**
     * @brief Removes duplicate paths from the PATH environment variable
     * @return True if duplicates were removed, otherwise false
     */
    static auto removeDuplicatesFromPath() -> bool;

    /**
     * @brief Validates that a path exists and is accessible
     * @param path The path to validate
     * @return True if the path is valid and accessible, otherwise false
     */
    ATOM_NODISCARD static auto isValidPath(const String& path) -> bool;

    /**
     * @brief Cleans up the PATH by removing invalid and duplicate entries
     * @return True if cleanup was successful, otherwise false
     */
    static auto cleanupPath() -> bool;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_PATH_HPP
