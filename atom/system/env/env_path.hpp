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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;

/**
 * @brief PATH operation result enumeration
 */
enum class PathOperationResult {
    SUCCESS,
    ALREADY_EXISTS,
    NOT_FOUND,
    INVALID_PATH,
    PERMISSION_DENIED,
    SYSTEM_ERROR
};

/**
 * @brief PATH cache entry
 */
struct PathCacheEntry {
    Vector<String> paths;
    std::chrono::steady_clock::time_point timestamp;
    bool isValid;

    PathCacheEntry() : isValid(false) {}
    PathCacheEntry(const Vector<String>& p)
        : paths(p), timestamp(std::chrono::steady_clock::now()), isValid(true) {}
};

/**
 * @brief PATH environment variable management
 */
class EnvPath {
public:
    /**
     * @brief Adds a path to the PATH environment variable
     * @param path Path to add
     * @param prepend Whether to add to the beginning (default adds to end)
     * @return Result of the operation
     */
    static auto addToPath(const String& path, bool prepend = false) -> PathOperationResult;

    /**
     * @brief Adds multiple paths to the PATH environment variable
     * @param paths Vector of paths to add
     * @param prepend Whether to add to the beginning
     * @return Number of successfully added paths
     */
    static auto addMultiplePaths(const Vector<String>& paths, bool prepend = false) -> size_t;

    /**
     * @brief Removes a path from the PATH environment variable
     * @param path Path to remove
     * @return Result of the operation
     */
    static auto removeFromPath(const String& path) -> PathOperationResult;

    /**
     * @brief Removes multiple paths from the PATH environment variable
     * @param paths Vector of paths to remove
     * @return Number of successfully removed paths
     */
    static auto removeMultiplePaths(const Vector<String>& paths) -> size_t;

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
     * @return Number of entries removed during cleanup
     */
    static auto cleanupPath() -> size_t;

    // ========== NEW ENHANCED FEATURES ==========

    /**
     * @brief Enables or disables PATH caching
     * @param enabled Whether to enable caching
     * @param ttl_seconds Cache time-to-live in seconds (default: 60)
     */
    static void setCachingEnabled(bool enabled, int ttl_seconds = 60);

    /**
     * @brief Clears the PATH cache
     */
    static void clearCache();

    /**
     * @brief Gets PATH cache statistics
     * @return Map containing cache statistics
     */
    static auto getCacheStats() -> HashMap<String, size_t>;

    /**
     * @brief Finds executable files in PATH
     * @param executable Name of executable to find
     * @return Vector of full paths to the executable
     */
    ATOM_NODISCARD static auto findExecutable(const String& executable) -> Vector<String>;

    /**
     * @brief Finds all executables in PATH matching a pattern
     * @param pattern Pattern to match (supports wildcards)
     * @return Vector of executable paths
     */
    ATOM_NODISCARD static auto findExecutablesMatching(const String& pattern) -> Vector<String>;

    /**
     * @brief Gets PATH statistics
     * @return Map containing PATH statistics (total entries, valid entries, etc.)
     */
    ATOM_NODISCARD static auto getPathStats() -> HashMap<String, size_t>;

    /**
     * @brief Backs up current PATH
     * @param name Backup name identifier
     * @return True if backup was successful
     */
    static auto backupPath(const String& name) -> bool;

    /**
     * @brief Restores PATH from backup
     * @param name Backup name identifier
     * @return True if restore was successful
     */
    static auto restorePath(const String& name) -> bool;

    /**
     * @brief Lists available PATH backups
     * @return Vector of backup names
     */
    static auto listPathBackups() -> Vector<String>;

    /**
     * @brief Optimizes PATH by reordering entries based on usage frequency
     * @return True if optimization was successful
     */
    static auto optimizePath() -> bool;

    /**
     * @brief Validates all paths in PATH environment variable
     * @return Vector of invalid paths found
     */
    ATOM_NODISCARD static auto validateAllPaths() -> Vector<String>;

    /**
     * @brief Gets the first valid path containing an executable
     * @param executable Name of executable to find
     * @return First path containing the executable, or empty string if not found
     */
    ATOM_NODISCARD static auto getFirstPathContaining(const String& executable) -> String;

private:
    // Caching system
    static PathCacheEntry sPathCache;
    static std::mutex sCacheMutex;
    static std::atomic<bool> sCachingEnabled;
    static std::atomic<int> sCacheTtlSeconds;
    static std::atomic<size_t> sCacheHits;
    static std::atomic<size_t> sCacheMisses;

    // Backup system
    static HashMap<String, String> sPathBackups;
    static std::mutex sBackupMutex;

    // Helper methods
    static auto getCachedPaths() -> std::optional<Vector<String>>;
    static void setCachedPaths(const Vector<String>& paths);
    static auto isCacheValid() -> bool;
    static auto isExecutableFile(const std::filesystem::path& filePath) -> bool;
    static auto matchesPattern(const String& filename, const String& pattern) -> bool;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_PATH_HPP
