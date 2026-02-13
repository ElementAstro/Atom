/*
 * file_ops.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file file_ops.hpp
 * @brief File CRUD operations: copy, move, rename, remove, truncate, symlink.
 */

#ifndef ATOM_IO_CORE_FILE_OPS_HPP
#define ATOM_IO_CORE_FILE_OPS_HPP

#include <cstdint>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>

#include "atom/io/core/types.hpp"

namespace atom::io {

/**
 * @brief Copies a file from source path to destination path.
 *
 * @param src_path The source path of the file to be copied.
 * @param dst_path The destination path of the copied file.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto copyFile(const P1& src_path, const P2& dst_path) -> bool;

/**
 * @brief Moves a file from source path to destination path.
 *
 * @param src_path The source path of the file to be moved.
 * @param dst_path The destination path of the moved file.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto moveFile(const P1& src_path, const P2& dst_path) -> bool;

/**
 * @brief Renames a file with the specified old and new paths.
 *
 * @param old_path The old path of the file to be renamed.
 * @param new_path The new path of the file after renaming.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto renameFile(const P1& old_path, const P2& new_path) -> bool;

/**
 * @brief Removes a file with the specified path.
 *
 * @param path The path of the file to remove.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto removeFile(const P& path) -> bool;

/**
 * @brief Creates a symbolic link with the specified target and symlink paths.
 *
 * @param target_path The path of the target file or directory for the symlink.
 * @param symlink_path The path of the symlink to create.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P1, PathLike P2>
[[nodiscard]] auto createSymlink(const P1& target_path,
                                 const P2& symlink_path) -> bool;

/**
 * @brief Removes a symbolic link with the specified path.
 *
 * @param path The path of the symlink to remove.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto removeSymlink(const P& path) -> bool;

/**
 * @brief Returns the size of a file in bytes.
 *
 * @param path The path of the file to get the size of.
 * @return The size of the file in bytes, or 0 if the file does not exist or
 * cannot be read.
 */
template <PathLike P>
[[nodiscard]] auto fileSize(const P& path) -> std::uintmax_t;

/**
 * @brief Truncates a file to a specified size.
 *
 * @param path The path of the file to truncate.
 * @param size The size to truncate the file to.
 * @return True if the operation was successful, false otherwise.
 */
template <PathLike P>
auto truncateFile(const P& path, std::streamsize size) -> bool;

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <PathLike P1, PathLike P2>
[[nodiscard]] auto copyFile(const P1& src_path, const P2& dst_path) -> bool {
    spdlog::info("copyFile called with src_path: {}, dst_path: {}",
                 fs::path(src_path).string(), fs::path(dst_path).string());

    const auto& srcPathStr = fs::path(src_path).string();
    const auto& dstPathStr = fs::path(dst_path).string();

    if (srcPathStr.empty() || dstPathStr.empty()) {
        spdlog::error("copyFile: Invalid empty path");
        return false;
    }

    try {
        // Create destination directory if it doesn't exist
        fs::path dstDir = fs::path(dst_path).parent_path();
        if (!dstDir.empty() && !fs::exists(dstDir)) {
            std::error_code ec;
            fs::create_directories(dstDir, ec);
            if (ec) {
                spdlog::error("Failed to create destination directory {}: {}",
                              dstDir.string(), ec.message());
                return false;
            }
        }

        std::error_code ec;
        fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing,
                      ec);
        if (ec) {
            spdlog::error("Failed to copy file from {} to {}: {}", srcPathStr,
                          dstPathStr, ec.message());
            return false;
        }

        spdlog::info("File copied from {} to {}", srcPathStr, dstPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to copy file from {} to {}: {}", srcPathStr,
                      dstPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error copying file from {} to {}: {}",
                      srcPathStr, dstPathStr, e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error copying file from {} to {}", srcPathStr,
                      dstPathStr);
        return false;
    }
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto moveFile(const P1& src_path, const P2& dst_path) -> bool {
    return renameFile(src_path, dst_path);
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto renameFile(const P1& old_path, const P2& new_path) -> bool {
    spdlog::info("renameFile called with old_path: {}, new_path: {}",
                 fs::path(old_path).string(), fs::path(new_path).string());

    const auto& oldPathStr = fs::path(old_path).string();
    const auto& newPathStr = fs::path(new_path).string();

    if (oldPathStr.empty() || newPathStr.empty()) {
        spdlog::error("renameFile: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;

        // Create destination directory if needed
        fs::path newDir = fs::path(new_path).parent_path();
        if (!newDir.empty() && !fs::exists(newDir)) {
            fs::create_directories(newDir, ec);
            if (ec) {
                spdlog::error("Failed to create destination directory {}: {}",
                              newDir.string(), ec.message());
                return false;
            }
        }

        fs::rename(old_path, new_path, ec);
        if (ec) {
            spdlog::error("Failed to rename file from {} to {}: {}", oldPathStr,
                          newPathStr, ec.message());

            // Fall back to copy and delete if rename fails (e.g., across file
            // systems)
            fs::copy_file(old_path, new_path,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error("Failed to copy file from {} to {}: {}",
                              oldPathStr, newPathStr, ec.message());
                return false;
            }

            fs::remove(old_path, ec);
            if (ec) {
                spdlog::warn("Failed to remove original file {} after copy: {}",
                             oldPathStr, ec.message());
                // We still succeeded in copying, so continue
            }
        }

        spdlog::info("File renamed from {} to {}", oldPathStr, newPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to rename file from {} to {}: {}", oldPathStr,
                      newPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error renaming file from {} to {}: {}",
                      oldPathStr, newPathStr, e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error renaming file from {} to {}", oldPathStr,
                      newPathStr);
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto removeFile(const P& path) -> bool {
    spdlog::info("removeFile called with path: {}", fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        spdlog::error("removeFile: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        bool result = fs::remove(path, ec);
        if (ec) {
            spdlog::error("Failed to remove file {}: {}", pathStr,
                          ec.message());
            return false;
        }
        spdlog::info("File removed: {}", pathStr);
        return result;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to remove file {}: {}", pathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error removing file {}: {}", pathStr,
                      e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error removing file {}", pathStr);
        return false;
    }
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto createSymlink(const P1& target_path,
                                 const P2& symlink_path) -> bool {
    spdlog::info("createSymlink called with target_path: {}, symlink_path: {}",
                 fs::path(target_path).string(),
                 fs::path(symlink_path).string());

    const auto& targetPathStr = fs::path(target_path).string();
    const auto& symlinkPathStr = fs::path(symlink_path).string();

    if (targetPathStr.empty() || symlinkPathStr.empty()) {
        spdlog::error("createSymlink: Invalid empty path");
        return false;
    }

    try {
        // Create parent directory for symlink if needed
        fs::path symlinkDir = fs::path(symlink_path).parent_path();
        if (!symlinkDir.empty() && !fs::exists(symlinkDir)) {
            std::error_code ec;
            fs::create_directories(symlinkDir, ec);
            if (ec) {
                spdlog::error(
                    "Failed to create symlink parent directory {}: {}",
                    symlinkDir.string(), ec.message());
                return false;
            }
        }

        std::error_code ec;
        fs::create_symlink(target_path, symlink_path, ec);
        if (ec) {
            spdlog::error("Failed to create symlink from {} to {}: {}",
                          targetPathStr, symlinkPathStr, ec.message());
            return false;
        }

        spdlog::info("Symlink created from {} to {}", targetPathStr,
                     symlinkPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to create symlink from {} to {}: {}",
                      targetPathStr, symlinkPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error creating symlink from {} to {}: {}",
                      targetPathStr, symlinkPathStr, e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error creating symlink from {} to {}",
                      targetPathStr, symlinkPathStr);
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto removeSymlink(const P& path) -> bool {
    return removeFile(path);
}

template <PathLike P>
[[nodiscard]] auto fileSize(const P& path) -> std::uintmax_t {
    spdlog::info("fileSize called with path: {}", fs::path(path).string());
    const auto& pathStr = fs::path(path).string();

    try {
        std::error_code ec;
        std::uintmax_t size = fs::file_size(path, ec);
        if (ec) {
            spdlog::error("Failed to get file size of {}: {}", pathStr,
                          ec.message());
            return 0;
        }
        spdlog::info("File size of {}: {}", pathStr, size);
        return size;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to get file size of {}: {}", pathStr, e.what());
        return 0;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error getting file size of {}: {}", pathStr,
                      e.what());
        return 0;
    } catch (...) {
        spdlog::error("Unknown error getting file size of {}", pathStr);
        return 0;
    }
}

template <PathLike P>
auto truncateFile(const P& path, std::streamsize size) -> bool {
    spdlog::info("truncateFile called with path: {}, size: {}",
                 fs::path(path).string(), size);
    const auto& pathStr = fs::path(path).string();

    if (pathStr.empty() || size < 0) {
        spdlog::error("truncateFile: Invalid arguments");
        return false;
    }

    try {
        std::ofstream file(pathStr,
                           std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for truncation: {}", pathStr);
            return false;
        }

        file.seekp(size);
        file.put('\0');
        file.close();

        if (file.fail()) {
            spdlog::error("Failed to truncate file {}: I/O error", pathStr);
            return false;
        }

        spdlog::info("File truncated: {}", pathStr);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error truncating file {}: {}", pathStr, e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error truncating file {}", pathStr);
        return false;
    }
}

}  // namespace atom::io

#endif  // ATOM_IO_CORE_FILE_OPS_HPP
