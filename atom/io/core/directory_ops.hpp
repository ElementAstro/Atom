/*
 * directory_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file directory_ops.hpp
 * @brief Directory CRUD operations: create, remove, rename, move.
 */

#ifndef ATOM_IO_CORE_DIRECTORY_OPS_HPP
#define ATOM_IO_CORE_DIRECTORY_OPS_HPP

#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

#include "atom/io/core/types.hpp"

namespace atom::io {

/**
 * @brief Creates a directory with the specified path.
 *
 * @param path The path of the directory to create.
 * @return True if the operation was successful, false otherwise.
 * @throws fs::filesystem_error if there's a filesystem error
 */
template <PathLike P>
[[nodiscard]] auto createDirectory(const P& path) -> bool;

/**
 * @brief Creates directories recursively with the specified base path and
 * subdirectories.
 *
 * @param basePath The base path of the directory to create.
 * @param subdirs The subdirectories to create.
 * @param options The options for creating the directory.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P, typename String = std::string>
auto createDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options = {}) -> bool;

/**
 * @brief Creates a directory with date-based path under root directory.
 *
 * @param date The date-based directory name to create.
 * @param rootDir The root directory of the directory to create.
 */
template <PathLike P1, PathLike P2>
void createDateDirectory(const P1& date, const P2& rootDir);

/**
 * @brief Removes an empty directory with the specified path.
 *
 * @param path The path of the directory to remove.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto removeDirectory(const P& path) -> bool;

/**
 * @brief Removes a directory with the specified path.
 *
 * @param basePath The base path of the directory to remove.
 * @param subdirs The subdirectories to remove.
 * @param options The options for removing the directory.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P, typename String = std::string>
[[nodiscard]] auto removeDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options = {}) -> bool;

/**
 * @brief Renames a directory with the specified old and new paths.
 *
 * @param old_path The old path of the directory to be renamed.
 * @param new_path The new path of the directory after renaming.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto renameDirectory(const P1& old_path,
                                   const P2& new_path) -> bool;

/**
 * @brief Moves a directory from one path to another.
 *
 * @param old_path The old path of the directory to be moved.
 * @param new_path The new path of the directory after moving.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto moveDirectory(const P1& old_path,
                                 const P2& new_path) -> bool;

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <PathLike P>
[[nodiscard]] auto createDirectory(const P& path) -> bool {
    spdlog::info("createDirectory called with path: {}",
                 fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        spdlog::error("createDirectory: Invalid empty path");
        return false;
    }

    try {
        bool result = fs::create_directory(path);
        spdlog::info("Directory created: {}", fs::path(path).string());
        return result;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to create directory {}: {}",
                      fs::path(path).string(), e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error creating directory {}: {}",
                      fs::path(path).string(), e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error creating directory {}",
                      fs::path(path).string());
        return false;
    }
}

template <PathLike P, typename String>
auto createDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options) -> bool {
    spdlog::info("createDirectoriesRecursive called with basePath: {}",
                 fs::path(basePath).string());

    fs::path basePathFs(basePath);
    if (!fs::exists(basePathFs)) {
        try {
            if (!options.dryRun && !fs::create_directories(basePathFs)) {
                spdlog::error("Failed to create base directory {}",
                              basePathFs.string());
                return false;
            }
        } catch (const std::exception& e) {
            spdlog::error("Error creating base directory {}: {}",
                          basePathFs.string(), e.what());
            return false;
        }
    }

    try {
        for (const auto& subdir :
             subdirs | std::views::filter(
                           [&](const auto& s) { return options.filter(s); })) {
            auto fullPath = basePathFs / subdir;
            if (fs::exists(fullPath) && fs::is_directory(fullPath)) {
                if (options.verbose) {
                    spdlog::info("Directory already exists: {}",
                                 fullPath.string());
                }
                continue;
            }

            if (!options.dryRun && !fs::create_directories(fullPath)) {
                spdlog::error("Failed to create directory {}",
                              fullPath.string());
                return false;
            }

            if (options.verbose) {
                spdlog::info("Created directory: {}", fullPath.string());
            }
            options.onCreate(fullPath.string());
            if (options.delay > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(options.delay));
            }
        }
        spdlog::info("createDirectoriesRecursive completed");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error in createDirectoriesRecursive: {}", e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error in createDirectoriesRecursive");
        return false;
    }
}

template <PathLike P1, PathLike P2>
void createDateDirectory(const P1& date, const P2& rootDir) {
    spdlog::info("createDateDirectory called with date: {}, rootDir: {}",
                 std::string(date), fs::path(rootDir).string());

    try {
        fs::path dir(rootDir);
        dir /= static_cast<std::string>(date);

        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            spdlog::info("Directory created: {}", dir.string());
        } else {
            spdlog::info("Directory already exists: {}", dir.string());
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in createDateDirectory: {}", e.what());
    } catch (...) {
        spdlog::error("Unknown error in createDateDirectory");
    }
}

template <PathLike P>
[[nodiscard]] auto removeDirectory(const P& path) -> bool {
    spdlog::info("removeDirectory called with path: {}",
                 fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        spdlog::error("removeDirectory: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        std::uintmax_t count = fs::remove_all(path, ec);
        if (ec) {
            spdlog::error("Failed to remove directory {}: {}", pathStr,
                          ec.message());
            return false;
        }
        spdlog::info("Directory removed: {} (removed {} items)", pathStr,
                     count);
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to remove directory {}: {}", pathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error removing directory {}: {}", pathStr,
                      e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error removing directory {}", pathStr);
        return false;
    }
}

template <PathLike P, typename String>
[[nodiscard]] auto removeDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options) -> bool {
    spdlog::info("removeDirectoriesRecursive called with basePath: {}",
                 fs::path(basePath).string());

    fs::path basePathFs(basePath);
    if (!fs::exists(basePathFs)) {
        spdlog::warn("Base path does not exist: {}", basePathFs.string());
        return false;
    }

    bool success = true;
    try {
        for (const auto& subdir :
             subdirs | std::views::filter(
                           [&](const auto& s) { return options.filter(s); })) {
            auto fullPath = basePathFs / subdir;
            if (!fs::exists(fullPath)) {
                if (options.verbose) {
                    spdlog::info("Directory does not exist: {}",
                                 fullPath.string());
                }
                continue;
            }

            try {
                if (!options.dryRun) {
                    std::error_code ec;
                    std::uintmax_t count = fs::remove_all(fullPath, ec);
                    if (ec) {
                        spdlog::error("Failed to delete directory {}: {}",
                                      fullPath.string(), ec.message());
                        success = false;
                        continue;
                    }
                    if (options.verbose) {
                        spdlog::info("Deleted directory: {} (removed {} items)",
                                     fullPath.string(), count);
                    }
                } else if (options.verbose) {
                    spdlog::info("Would delete directory: {} (dry run)",
                                 fullPath.string());
                }
            } catch (const fs::filesystem_error& e) {
                spdlog::error("Failed to delete directory {}: {}",
                              fullPath.string(), e.what());
                success = false;
                continue;
            }

            options.onDelete(fullPath.string());
            if (options.delay > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(options.delay));
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in removeDirectoriesRecursive: {}", e.what());
        return false;
    }

    spdlog::info("removeDirectoriesRecursive completed with status: {}",
                 success);
    return success;
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto renameDirectory(const P1& old_path,
                                   const P2& new_path) -> bool {
    spdlog::info("renameDirectory called with old_path: {}, new_path: {}",
                 fs::path(old_path).string(), fs::path(new_path).string());
    return moveDirectory(old_path, new_path);
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto moveDirectory(const P1& old_path,
                                 const P2& new_path) -> bool {
    spdlog::info("moveDirectory called with old_path: {}, new_path: {}",
                 fs::path(old_path).string(), fs::path(new_path).string());

    const auto& oldPathStr = fs::path(old_path).string();
    const auto& newPathStr = fs::path(new_path).string();

    if (oldPathStr.empty() || newPathStr.empty()) {
        spdlog::error("moveDirectory: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        fs::rename(old_path, new_path, ec);
        if (ec) {
            spdlog::error("Failed to move directory from {} to {}: {}",
                          oldPathStr, newPathStr, ec.message());

            // Fall back to copy and delete if rename fails (e.g., across file
            // systems)
            fs::copy(old_path, new_path, fs::copy_options::recursive, ec);
            if (ec) {
                spdlog::error("Failed to copy directory from {} to {}: {}",
                              oldPathStr, newPathStr, ec.message());
                return false;
            }

            fs::remove_all(old_path, ec);
            if (ec) {
                spdlog::warn(
                    "Failed to remove original directory {} after copy: {}",
                    oldPathStr, ec.message());
                // We still succeeded in copying, so return true
            }
        }

        spdlog::info("Directory moved from {} to {}", oldPathStr, newPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to move directory from {} to {}: {}", oldPathStr,
                      newPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error moving directory from {} to {}: {}",
                      oldPathStr, newPathStr, e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error moving directory from {} to {}",
                      oldPathStr, newPathStr);
        return false;
    }
}

}  // namespace atom::io

#endif  // ATOM_IO_CORE_DIRECTORY_OPS_HPP
