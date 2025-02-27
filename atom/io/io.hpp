/*
 * io.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-3

Description: IO

**************************************************/

#ifndef ATOM_IO_IO_HPP
#define ATOM_IO_IO_HPP

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/macro.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;

namespace atom::io {

// Concepts for path-like types
template <typename T>
concept PathLike =
    std::convertible_to<T, fs::path> || std::convertible_to<T, std::string> ||
    std::convertible_to<T, std::string_view> ||
    std::convertible_to<T, const char*>;

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
 * @brief Creates a directory with the specified path.
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
} ATOM_ALIGNAS(128);

enum class PathType { NOT_EXISTS, REGULAR_FILE, DIRECTORY, SYMLINK, OTHER };

/**
 * @brief Creates a directory with the specified path.
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
 * @brief Creates a directory with the specified path.
 *
 * @param date The directory to create.
 * @param rootDir The root directory of the directory to create.
 */
template <PathLike P1, PathLike P2>
void createDirectory(const P1& date, const P2& rootDir);

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

/**
 * @brief Recursively walks through a directory and its subdirectories, applying
 * a callback function to each file.
 *
 * This function traverses a directory and its subdirectories, calling the
 * specified callback function for each file encountered.
 *
 * @param root The root path of the directory to walk.
 * @return a json string containing the file information.
 */
template <PathLike P>
[[nodiscard]] auto jwalk(const P& root) -> std::string;

/**
 * @brief Recursively walks through a directory and its subdirectories, applying
 * a callback function to each file.
 *
 * This function traverses a directory and its subdirectories, calling the
 * specified callback function for each file encountered.
 *
 * @param root     The root path of the directory to walk.
 * @param callback The callback function to execute for each file.
 */
template <PathLike P>
void fwalk(const P& root, const std::function<void(const fs::path&)>& callback);

/**
 * @brief Convert Windows path to Linux path.
 *
 * This function converts a Windows path to a Linux path by replacing
 * backslashes with forward slashes.
 *
 * @param windows_path The Windows path to convert.
 * @return The converted Linux path.
 */
[[nodiscard]] auto convertToLinuxPath(std::string_view windows_path)
    -> std::string;

/**
 * @brief Convert Linux path to Windows path.
 *
 * This function converts a Linux path to a Windows path by replacing forward
 * slashes with backslashes.
 *
 * @param linux_path The Linux path to convert.
 * @return The converted Windows path.
 */
[[nodiscard]] auto convertToWindowsPath(std::string_view linux_path)
    -> std::string;

/**
 * @brief Normalize a path according to the platform conventions
 *
 * @param raw_path The path to normalize
 * @return Normalized path string
 */
[[nodiscard]] auto normPath(std::string_view raw_path) -> std::string;

/**
 * @brief Check if the folder name is valid.
 *
 * @param folderName The folder name to check.
 * @return True if the folder name is valid, false otherwise.
 */
[[nodiscard]] auto isFolderNameValid(std::string_view folderName) -> bool;

/**
 * @brief Check if the file name is valid.
 *
 * @param fileName The file name to check.
 * @return True if the file name is valid, false otherwise.
 */
[[nodiscard]] auto isFileNameValid(std::string_view fileName) -> bool;

/**
 * @brief Check if the folder exists.
 *
 * @param folderPath The folder path to check.
 * @return True if the folder exists, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto isFolderExists(const P& folderPath) -> bool;

/**
 * @brief Check if the file exists.
 *
 * @param filePath The file path to check.
 * @return True if the file exists, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto isFileExists(const P& filePath) -> bool;

/**
 * @brief Check if the folder is empty.
 *
 * @param folderPath The folder path to check.
 * @return True if the folder is empty, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto isFolderEmpty(const P& folderPath) -> bool;

/**
 * @brief Check if the path is an absolute path.
 *
 * @param path The path to check.
 * @return True if the path is an absolute path, false otherwise.
 */
template <PathLike P>
[[nodiscard]] auto isAbsolutePath(const P& path) -> bool;

/**
 * @brief Change the working directory.
 *
 * @param directoryPath The directory path to change to.
 * @return True if the working directory was changed successfully, false
 * otherwise.
 */
template <PathLike P>
[[nodiscard]] auto changeWorkingDirectory(const P& directoryPath) -> bool;

/**
 * @brief Get file creation and modification times
 *
 * @param filePath Path to the file
 * @return std::pair<std::string, std::string> Creation time and modification
 * time
 */
template <PathLike P>
[[nodiscard]] std::pair<std::string, std::string> getFileTimes(
    const P& filePath);

/**
 * @brief The option to check the file type.
 */
enum class FileOption { PATH, NAME };

/**
 * @brief Check the file type in the folder.
 *
 * @param folderPath The folder path to check.
 * @param fileTypes The file types to check.
 * @param fileOption The option to check the file type.
 * @return A vector of file paths.
 * @remark The file type is checked by the file extension.
 */
template <PathLike P>
[[nodiscard]] auto checkFileTypeInFolder(
    const P& folderPath, std::span<const std::string> fileTypes,
    FileOption fileOption) -> std::vector<std::string>;

/**
 * @brief Check whether the specified file exists and is executable.
 *
 * @param fileName The name of the file.
 * @param fileExt The extension of the file.
 * @return true if the file exists and is executable.
 * @return false if the file doesn't exist or isn't executable.
 */
template <PathLike P1, PathLike P2 = const char*>
auto isExecutableFile(const P1& fileName, const P2& fileExt = "") -> bool;

/**
 * @brief Get the file size.
 *
 * @param filePath The file path.
 * @return The file size.
 */
template <PathLike P>
auto getFileSize(const P& filePath) -> std::size_t;

/**
 * @brief Calculate the chunk size.
 *
 * @param fileSize The file size.
 * @param numChunks The number of chunks.
 * @return The chunk size.
 */
[[nodiscard]] constexpr auto calculateChunkSize(std::size_t fileSize,
                                                int numChunks) -> std::size_t;

/**
 * @brief Split a file into multiple parts.
 *
 * @param filePath The file path.
 * @param chunkSize The chunk size.
 * @param outputPattern The output file pattern.
 */
template <PathLike P1, PathLike P2 = const char*>
void splitFile(const P1& filePath, std::size_t chunkSize,
               const P2& outputPattern = "");

/**
 * @brief Merge multiple parts into a single file.
 *
 * @param outputFilePath The output file path.
 * @param partFiles The part files.
 */
template <PathLike P>
void mergeFiles(const P& outputFilePath,
                std::span<const std::string> partFiles);

/**
 * @brief Quickly split a file into multiple parts.
 *
 * @param filePath The file path.
 * @param numChunks The number of chunks.
 * @param outputPattern The output file pattern.
 */
template <PathLike P1, PathLike P2 = const char*>
void quickSplit(const P1& filePath, int numChunks,
                const P2& outputPattern = "");

/**
 * @brief Quickly merge multiple parts into a single file.
 *
 * @param outputFilePath The output file path.
 * @param partPattern The part file pattern.
 * @param numChunks The number of chunks.
 */
template <PathLike P1, PathLike P2>
void quickMerge(const P1& outputFilePath, const P2& partPattern, int numChunks);

/**
 * @brief Get the executable name from the path.
 *
 * @param path The path of the executable.
 * @return The executable name.
 */
[[nodiscard]] auto getExecutableNameFromPath(std::string_view path)
    -> std::string;

/**
 * @brief Get the file type
 *
 * @param path The path of the file.
 * @return The type of the file.
 */
template <PathLike P>
auto checkPathType(const P& path) -> PathType;

/**
 * @brief Count lines in a file
 *
 * @param filePath Path to the file
 * @return std::optional<int> Line count or nullopt if file couldn't be opened
 */
template <PathLike P>
auto countLinesInFile(const P& filePath) -> std::optional<int>;

/**
 * @brief Search for executable files in a directory containing search string
 *
 * @param dir Directory to search in
 * @param searchStr String to search for in filenames
 * @return std::vector<fs::path> Paths to found executable files
 */
template <PathLike P>
auto searchExecutableFiles(const P& dir,
                           std::string_view searchStr) -> std::vector<fs::path>;

/**
 * @brief Classify files in a directory by extension
 *
 * @param directory Directory to classify files in
 * @return std::unordered_map<std::string, std::vector<std::string>> Map of
 * extensions to file paths
 */
template <PathLike P>
auto classifyFiles(const P& directory)
    -> std::unordered_map<std::string, std::vector<std::string>>;

}  // namespace atom::io

#endif

/*
 * io_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-7-7

Description: IO template implementations

**************************************************/

#ifndef ATOM_IO_IO_IMPL_HPP
#define ATOM_IO_IO_IMPL_HPP

namespace atom::io {

template <PathLike P>
[[nodiscard]] auto createDirectory(const P& path) -> bool {
    LOG_F(INFO, "createDirectory called with path: {}",
          fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        LOG_F(ERROR, "createDirectory: Invalid empty path");
        return false;
    }

    try {
        bool result = fs::create_directory(path);
        LOG_F(INFO, "Directory created: {}", fs::path(path).string());
        return result;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to create directory {}: {}",
              fs::path(path).string(), e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error creating directory {}: {}",
              fs::path(path).string(), e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error creating directory {}",
              fs::path(path).string());
        return false;
    }
}

template <PathLike P, typename String>
auto createDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options) -> bool {
    LOG_F(INFO, "createDirectoriesRecursive called with basePath: {}",
          fs::path(basePath).string());

    fs::path basePathFs(basePath);
    if (!fs::exists(basePathFs)) {
        try {
            if (!options.dryRun && !fs::create_directories(basePathFs)) {
                LOG_F(ERROR, "Failed to create base directory {}",
                      basePathFs.string());
                return false;
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error creating base directory {}: {}",
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
                    LOG_F(INFO, "Directory already exists: {}",
                          fullPath.string());
                }
                continue;
            }

            if (!options.dryRun && !fs::create_directories(fullPath)) {
                LOG_F(ERROR, "Failed to create directory {}",
                      fullPath.string());
                return false;
            }

            if (options.verbose) {
                LOG_F(INFO, "Created directory: {}", fullPath.string());
            }
            options.onCreate(fullPath.string());
            if (options.delay > 0) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(options.delay));
            }
        }
        LOG_F(INFO, "createDirectoriesRecursive completed");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in createDirectoriesRecursive: {}", e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error in createDirectoriesRecursive");
        return false;
    }
}

template <PathLike P1, PathLike P2>
void createDirectory(const P1& date, const P2& rootDir) {
    LOG_F(INFO, "createDirectory called with date: {}, rootDir: {}",
          std::string(date), fs::path(rootDir).string());

    try {
        fs::path dir(rootDir);
        dir /= static_cast<std::string>(date);

        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            LOG_F(INFO, "Directory created: {}", dir.string());
        } else {
            LOG_F(INFO, "Directory already exists: {}", dir.string());
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in createDirectory: {}", e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error in createDirectory");
    }
}

template <PathLike P>
[[nodiscard]] auto removeDirectory(const P& path) -> bool {
    LOG_F(INFO, "removeDirectory called with path: {}",
          fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        LOG_F(ERROR, "removeDirectory: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        std::uintmax_t count = fs::remove_all(path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to remove directory {}: {}", pathStr,
                  ec.message());
            return false;
        }
        LOG_F(INFO, "Directory removed: {} (removed {} items)", pathStr, count);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to remove directory {}: {}", pathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error removing directory {}: {}", pathStr,
              e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error removing directory {}", pathStr);
        return false;
    }
}

template <PathLike P, typename String>
[[nodiscard]] auto removeDirectoriesRecursive(
    const P& basePath, const std::vector<String>& subdirs,
    const CreateDirectoriesOptions& options) -> bool {
    LOG_F(INFO, "removeDirectoriesRecursive called with basePath: {}",
          fs::path(basePath).string());

    fs::path basePathFs(basePath);
    if (!fs::exists(basePathFs)) {
        LOG_F(WARNING, "Base path does not exist: {}", basePathFs.string());
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
                    LOG_F(INFO, "Directory does not exist: {}",
                          fullPath.string());
                }
                continue;
            }

            try {
                if (!options.dryRun) {
                    std::error_code ec;
                    std::uintmax_t count = fs::remove_all(fullPath, ec);
                    if (ec) {
                        LOG_F(ERROR, "Failed to delete directory {}: {}",
                              fullPath.string(), ec.message());
                        success = false;
                        continue;
                    }
                    if (options.verbose) {
                        LOG_F(INFO, "Deleted directory: {} (removed {} items)",
                              fullPath.string(), count);
                    }
                } else if (options.verbose) {
                    LOG_F(INFO, "Would delete directory: {} (dry run)",
                          fullPath.string());
                }
            } catch (const fs::filesystem_error& e) {
                LOG_F(ERROR, "Failed to delete directory {}: {}",
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
        LOG_F(ERROR, "Error in removeDirectoriesRecursive: {}", e.what());
        return false;
    }

    LOG_F(INFO, "removeDirectoriesRecursive completed with status: {}",
          success);
    return success;
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto renameDirectory(const P1& old_path,
                                   const P2& new_path) -> bool {
    LOG_F(INFO, "renameDirectory called with old_path: {}, new_path: {}",
          fs::path(old_path).string(), fs::path(new_path).string());
    return moveDirectory(old_path, new_path);
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto moveDirectory(const P1& old_path,
                                 const P2& new_path) -> bool {
    LOG_F(INFO, "moveDirectory called with old_path: {}, new_path: {}",
          fs::path(old_path).string(), fs::path(new_path).string());

    const auto& oldPathStr = fs::path(old_path).string();
    const auto& newPathStr = fs::path(new_path).string();

    if (oldPathStr.empty() || newPathStr.empty()) {
        LOG_F(ERROR, "moveDirectory: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        fs::rename(old_path, new_path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to move directory from {} to {}: {}",
                  oldPathStr, newPathStr, ec.message());

            // Fall back to copy and delete if rename fails (e.g., across file
            // systems)
            fs::copy(old_path, new_path, fs::copy_options::recursive, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to copy directory from {} to {}: {}",
                      oldPathStr, newPathStr, ec.message());
                return false;
            }

            fs::remove_all(old_path, ec);
            if (ec) {
                LOG_F(WARNING,
                      "Failed to remove original directory {} after copy: {}",
                      oldPathStr, ec.message());
                // We still succeeded in copying, so return true
            }
        }

        LOG_F(INFO, "Directory moved from {} to {}", oldPathStr, newPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to move directory from {} to {}: {}", oldPathStr,
              newPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error moving directory from {} to {}: {}",
              oldPathStr, newPathStr, e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error moving directory from {} to {}", oldPathStr,
              newPathStr);
        return false;
    }
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto copyFile(const P1& src_path, const P2& dst_path) -> bool {
    LOG_F(INFO, "copyFile called with src_path: {}, dst_path: {}",
          fs::path(src_path).string(), fs::path(dst_path).string());

    const auto& srcPathStr = fs::path(src_path).string();
    const auto& dstPathStr = fs::path(dst_path).string();

    if (srcPathStr.empty() || dstPathStr.empty()) {
        LOG_F(ERROR, "copyFile: Invalid empty path");
        return false;
    }

    try {
        // Create destination directory if it doesn't exist
        fs::path dstDir = fs::path(dst_path).parent_path();
        if (!dstDir.empty() && !fs::exists(dstDir)) {
            std::error_code ec;
            fs::create_directories(dstDir, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to create destination directory {}: {}",
                      dstDir.string(), ec.message());
                return false;
            }
        }

        std::error_code ec;
        fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing,
                      ec);
        if (ec) {
            LOG_F(ERROR, "Failed to copy file from {} to {}: {}", srcPathStr,
                  dstPathStr, ec.message());
            return false;
        }

        LOG_F(INFO, "File copied from {} to {}", srcPathStr, dstPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to copy file from {} to {}: {}", srcPathStr,
              dstPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error copying file from {} to {}: {}",
              srcPathStr, dstPathStr, e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error copying file from {} to {}", srcPathStr,
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
    LOG_F(INFO, "renameFile called with old_path: {}, new_path: {}",
          fs::path(old_path).string(), fs::path(new_path).string());

    const auto& oldPathStr = fs::path(old_path).string();
    const auto& newPathStr = fs::path(new_path).string();

    if (oldPathStr.empty() || newPathStr.empty()) {
        LOG_F(ERROR, "renameFile: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;

        // Create destination directory if needed
        fs::path newDir = fs::path(new_path).parent_path();
        if (!newDir.empty() && !fs::exists(newDir)) {
            fs::create_directories(newDir, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to create destination directory {}: {}",
                      newDir.string(), ec.message());
                return false;
            }
        }

        fs::rename(old_path, new_path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to rename file from {} to {}: {}", oldPathStr,
                  newPathStr, ec.message());

            // Fall back to copy and delete if rename fails (e.g., across file
            // systems)
            fs::copy_file(old_path, new_path,
                          fs::copy_options::overwrite_existing, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to copy file from {} to {}: {}",
                      oldPathStr, newPathStr, ec.message());
                return false;
            }

            fs::remove(old_path, ec);
            if (ec) {
                LOG_F(WARNING,
                      "Failed to remove original file {} after copy: {}",
                      oldPathStr, ec.message());
                // We still succeeded in copying, so continue
            }
        }

        LOG_F(INFO, "File renamed from {} to {}", oldPathStr, newPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to rename file from {} to {}: {}", oldPathStr,
              newPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error renaming file from {} to {}: {}",
              oldPathStr, newPathStr, e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error renaming file from {} to {}", oldPathStr,
              newPathStr);
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto removeFile(const P& path) -> bool {
    LOG_F(INFO, "removeFile called with path: {}", fs::path(path).string());
    const auto& pathStr = fs::path(path).string();
    if (pathStr.empty()) {
        LOG_F(ERROR, "removeFile: Invalid empty path");
        return false;
    }

    try {
        std::error_code ec;
        bool result = fs::remove(path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to remove file {}: {}", pathStr, ec.message());
            return false;
        }
        LOG_F(INFO, "File removed: {}", pathStr);
        return result;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to remove file {}: {}", pathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error removing file {}: {}", pathStr,
              e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error removing file {}", pathStr);
        return false;
    }
}

template <PathLike P1, PathLike P2>
[[nodiscard]] auto createSymlink(const P1& target_path,
                                 const P2& symlink_path) -> bool {
    LOG_F(INFO, "createSymlink called with target_path: {}, symlink_path: {}",
          fs::path(target_path).string(), fs::path(symlink_path).string());

    const auto& targetPathStr = fs::path(target_path).string();
    const auto& symlinkPathStr = fs::path(symlink_path).string();

    if (targetPathStr.empty() || symlinkPathStr.empty()) {
        LOG_F(ERROR, "createSymlink: Invalid empty path");
        return false;
    }

    try {
        // Create parent directory for symlink if needed
        fs::path symlinkDir = fs::path(symlink_path).parent_path();
        if (!symlinkDir.empty() && !fs::exists(symlinkDir)) {
            std::error_code ec;
            fs::create_directories(symlinkDir, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to create symlink parent directory {}: {}",
                      symlinkDir.string(), ec.message());
                return false;
            }
        }

        std::error_code ec;
        fs::create_symlink(target_path, symlink_path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to create symlink from {} to {}: {}",
                  targetPathStr, symlinkPathStr, ec.message());
            return false;
        }

        LOG_F(INFO, "Symlink created from {} to {}", targetPathStr,
              symlinkPathStr);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to create symlink from {} to {}: {}",
              targetPathStr, symlinkPathStr, e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error creating symlink from {} to {}: {}",
              targetPathStr, symlinkPathStr, e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error creating symlink from {} to {}",
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
    LOG_F(INFO, "fileSize called with path: {}", fs::path(path).string());
    const auto& pathStr = fs::path(path).string();

    try {
        std::error_code ec;
        std::uintmax_t size = fs::file_size(path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to get file size of {}: {}", pathStr,
                  ec.message());
            return 0;
        }
        LOG_F(INFO, "File size of {}: {}", pathStr, size);
        return size;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to get file size of {}: {}", pathStr, e.what());
        return 0;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error getting file size of {}: {}", pathStr,
              e.what());
        return 0;
    } catch (...) {
        LOG_F(ERROR, "Unknown error getting file size of {}", pathStr);
        return 0;
    }
}

template <PathLike P>
auto truncateFile(const P& path, std::streamsize size) -> bool {
    LOG_F(INFO, "truncateFile called with path: {}, size: {}",
          fs::path(path).string(), size);
    const auto& pathStr = fs::path(path).string();

    if (pathStr.empty() || size < 0) {
        LOG_F(ERROR, "truncateFile: Invalid arguments");
        return false;
    }

    try {
        std::ofstream file(pathStr,
                           std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open file for truncation: {}", pathStr);
            return false;
        }

        file.seekp(size);
        file.put('\0');
        file.close();

        if (file.fail()) {
            LOG_F(ERROR, "Failed to truncate file {}: I/O error", pathStr);
            return false;
        }

        LOG_F(INFO, "File truncated: {}", pathStr);
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error truncating file {}: {}", pathStr, e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error truncating file {}", pathStr);
        return false;
    }
}

// Function to walk through directories and apply a callback
inline void walk(const fs::path& root, bool recursive,
                 const std::function<void(const fs::path&)>& callback) {
    LOG_F(INFO, "walk called with root: {}, recursive: {}", root.string(),
          recursive);

    try {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error traversing directory {}: {}", root.string(),
                      ec.message());
                continue;
            }

            callback(entry.path());

            if (recursive && fs::is_directory(entry, ec)) {
                if (!ec) {
                    walk(entry.path(), recursive, callback);
                } else {
                    LOG_F(ERROR, "Error checking if {} is directory: {}",
                          entry.path().string(), ec.message());
                }
            }
        }

        LOG_F(INFO, "walk completed for root: {}", root.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error walking directory {}: {}", root.string(), e.what());
    }
}

// Helper function to build JSON structure
inline auto buildJsonStructure(const fs::path& root,
                               bool recursive) -> nlohmann::json {
    LOG_F(INFO, "buildJsonStructure called with root: {}, recursive: {}",
          root.string(), recursive);

    nlohmann::json folder = {{"path", root.generic_string()},
                             {"directories", nlohmann::json::array()},
                             {"files", nlohmann::json::array()}};

    try {
        walk(root, recursive, [&](const fs::path& entry) {
            std::error_code ec;
            if (fs::is_directory(entry, ec)) {
                if (!ec) {
                    folder["directories"].push_back(
                        buildJsonStructure(entry, recursive));
                }
            } else if (!ec) {
                folder["files"].push_back(entry.generic_string());
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error building JSON structure for {}: {}", root.string(),
              e.what());
    }

    LOG_F(INFO, "buildJsonStructure completed for root: {}", root.string());
    return folder;
}

template <PathLike P>
[[nodiscard]] auto jwalk(const P& root) -> std::string {
    LOG_F(INFO, "jwalk called with root: {}", fs::path(root).string());
    fs::path rootPath(root);

    try {
        if (!isFolderExists(rootPath)) {
            LOG_F(WARNING, "Folder does not exist: {}", rootPath.string());
            return "";
        }

        std::string result = buildJsonStructure(rootPath, true).dump();
        LOG_F(INFO, "jwalk completed for root: {}", rootPath.string());
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in jwalk for {}: {}", rootPath.string(), e.what());
        return "";
    } catch (...) {
        LOG_F(ERROR, "Unknown error in jwalk for {}", rootPath.string());
        return "";
    }
}

template <PathLike P>
void fwalk(const P& root,
           const std::function<void(const fs::path&)>& callback) {
    LOG_F(INFO, "fwalk called with root: {}", fs::path(root).string());

    try {
        fs::path rootPath(root);
        walk(rootPath, true, callback);
        LOG_F(INFO, "fwalk completed for root: {}", rootPath.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in fwalk for {}: {}", fs::path(root).string(),
              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error in fwalk for {}", fs::path(root).string());
    }
}

template <PathLike P>
[[nodiscard]] auto isFolderExists(const P& folderPath) -> bool {
    LOG_F(INFO, "isFolderExists called with folderPath: {}",
          fs::path(folderPath).string());

    try {
        fs::path path(folderPath);
        std::error_code ec;
        bool result = fs::exists(path, ec) && fs::is_directory(path, ec);
        if (ec) {
            LOG_F(ERROR, "Error checking if folder exists {}: {}",
                  path.string(), ec.message());
            return false;
        }

        LOG_F(INFO, "isFolderExists returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in isFolderExists for {}: {}",
              fs::path(folderPath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isFileExists(const P& filePath) -> bool {
    LOG_F(INFO, "isFileExists called with filePath: {}",
          fs::path(filePath).string());

    try {
        fs::path path(filePath);
        std::error_code ec;
        bool result = fs::exists(path, ec) && fs::is_regular_file(path, ec);
        if (ec) {
            LOG_F(ERROR, "Error checking if file exists {}: {}", path.string(),
                  ec.message());
            return false;
        }

        LOG_F(INFO, "isFileExists returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in isFileExists for {}: {}",
              fs::path(filePath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isFolderEmpty(const P& folderPath) -> bool {
    LOG_F(INFO, "isFolderEmpty called with folderPath: {}",
          fs::path(folderPath).string());

    try {
        fs::path path(folderPath);
        if (!isFolderExists(path)) {
            LOG_F(WARNING, "Folder does not exist: {}", path.string());
            return false;
        }

        std::error_code ec;
        bool result = fs::is_empty(path, ec);
        if (ec) {
            LOG_F(ERROR, "Error checking if folder is empty {}: {}",
                  path.string(), ec.message());
            return false;
        }

        LOG_F(INFO, "isFolderEmpty returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in isFolderEmpty for {}: {}",
              fs::path(folderPath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isAbsolutePath(const P& path) -> bool {
    LOG_F(INFO, "isAbsolutePath called with path: {}", fs::path(path).string());

    try {
        bool result = fs::path(path).is_absolute();
        LOG_F(INFO, "isAbsolutePath returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in isAbsolutePath for {}: {}",
              fs::path(path).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto changeWorkingDirectory(const P& directoryPath) -> bool {
    LOG_F(INFO, "changeWorkingDirectory called with directoryPath: {}",
          fs::path(directoryPath).string());

    try {
        fs::path path(directoryPath);
        if (!isFolderExists(path)) {
            LOG_F(ERROR, "Directory does not exist: {}", path.string());
            return false;
        }

        std::error_code ec;
        fs::current_path(path, ec);
        if (ec) {
            LOG_F(ERROR, "Failed to change working directory to {}: {}",
                  path.string(), ec.message());
            return false;
        }

        LOG_F(INFO, "Changed working directory to: {}", path.string());
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_F(ERROR, "Failed to change working directory to {}: {}",
              fs::path(directoryPath).string(), e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error changing working directory to {}: {}",
              fs::path(directoryPath).string(), e.what());
        return false;
    } catch (...) {
        LOG_F(ERROR, "Unknown error changing working directory to {}",
              fs::path(directoryPath).string());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] std::pair<std::string, std::string> getFileTimes(
    const P& filePath) {
    LOG_F(INFO, "getFileTimes called with filePath: {}",
          fs::path(filePath).string());
    std::pair<std::string, std::string> fileTimes;

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            LOG_F(ERROR, "File does not exist: {}", path.string());
            return fileTimes;
        }

        std::error_code ec;
        auto writeTime = fs::last_write_time(path, ec);
        if (ec) {
            LOG_F(ERROR, "Error getting last write time for {}: {}",
                  path.string(), ec.message());
            return fileTimes;
        }

        // Get file creation time - C++20 still doesn't have a standard way to
        // get creation time This implementation may need platform-specific code
        // for complete accuracy

#if defined(_WIN32)
        // Windows implementation
        // This is platform-specific and would need to use Windows API
        // Placeholder for now
        fileTimes.first = "Creation time not available in portable C++";
#else
        // Unix/Linux implementation
        // This is platform-specific and would need to use stat
        // Placeholder for now
        fileTimes.first = "Creation time not available in portable C++";
#endif

        // Convert last_write_time to string
        // C++20 provides better formatting, using std::format when available
#if __cpp_lib_format >= 202106L
        fileTimes.second = std::format(
            "{:%Y-%m-%d %H:%M:%S}", std::chrono::file_clock::to_sys(writeTime));
#else
        // Fallback for C++20 without std::format
        auto systemTime = std::chrono::file_clock::to_sys(writeTime);
        auto timeT = std::chrono::system_clock::to_time_t(systemTime);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        fileTimes.second = ss.str();
#endif

        LOG_F(INFO, "getFileTimes returning: modification time: {}",
              fileTimes.second);
        return fileTimes;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting file times for {}: {}",
              fs::path(filePath).string(), e.what());
        return fileTimes;
    }
}

template <PathLike P>
[[nodiscard]] auto checkFileTypeInFolder(
    const P& folderPath, std::span<const std::string> fileTypes,
    FileOption fileOption) -> std::vector<std::string> {
    LOG_F(INFO, "checkFileTypeInFolder called with folderPath: {}",
          fs::path(folderPath).string());

    std::vector<std::string> files;

    try {
        fs::path path(folderPath);
        if (!isFolderExists(path)) {
            LOG_F(ERROR, "Folder does not exist: {}", path.string());
            return files;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error iterating directory {}: {}", path.string(),
                      ec.message());
                continue;
            }

            if (entry.is_regular_file(ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error checking if {} is regular file: {}",
                          entry.path().string(), ec.message());
                    continue;
                }

                auto extension = entry.path().extension().string();
                if (std::ranges::find(fileTypes, extension) !=
                    fileTypes.end()) {
                    files.push_back(fileOption == FileOption::PATH
                                        ? entry.path().string()
                                        : entry.path().filename().string());
                }
            }
        }
    } catch (const fs::filesystem_error& ex) {
        LOG_F(ERROR, "Failed to check files in folder {}: {}",
              fs::path(folderPath).string(), ex.what());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Unexpected error checking files in folder {}: {}",
              fs::path(folderPath).string(), e.what());
    }

    LOG_F(INFO, "checkFileTypeInFolder returning {} files", files.size());
    return files;
}

template <PathLike P1, PathLike P2>
auto isExecutableFile(const P1& fileName, const P2& fileExt) -> bool {
    LOG_F(INFO, "isExecutableFile called with fileName: {}, fileExt: {}",
          fs::path(fileName).string(), std::string(fileExt));

    try {
#ifdef _WIN32
        fs::path filePath = fs::path(fileName).string() + std::string(fileExt);
#else
        fs::path filePath = fileName;
#endif

        LOG_F(INFO, "Checking file '{}'.", filePath.string());
        std::error_code ec;

        // Check if file exists and is regular
        if (!fs::exists(filePath, ec) || ec) {
            LOG_F(WARNING, "The file '{}' does not exist: {}",
                  filePath.string(), ec ? ec.message() : "");
            return false;
        }

        if (!fs::is_regular_file(filePath, ec) || ec) {
            LOG_F(WARNING, "The path '{}' is not a regular file: {}",
                  filePath.string(), ec ? ec.message() : "");
            return false;
        }

#ifndef _WIN32
        // On Unix-like systems, check execute permissions
        fs::perms p = fs::status(filePath, ec).permissions();
        if (ec) {
            LOG_F(WARNING, "Error getting permissions for '{}': {}",
                  filePath.string(), ec.message());
            return false;
        }

        if ((p & fs::perms::owner_exec) == fs::perms::none) {
            LOG_F(WARNING, "The file '{}' is not executable.",
                  filePath.string());
            return false;
        }
#endif

        LOG_F(INFO, "The file '{}' exists and is executable.",
              filePath.string());
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking if file is executable {}: {}",
              fs::path(fileName).string(), e.what());
        return false;
    }
}

template <PathLike P>
auto getFileSize(const P& filePath) -> std::size_t {
    LOG_F(INFO, "getFileSize called with filePath: {}",
          fs::path(filePath).string());

    try {
        fs::path path(filePath);
        std::error_code ec;
        std::size_t size = fs::file_size(path, ec);
        if (ec) {
            LOG_F(ERROR, "Error getting file size for {}: {}", path.string(),
                  ec.message());
            return 0;
        }

        LOG_F(INFO, "getFileSize returning: {}", size);
        return size;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error getting file size for {}: {}",
              fs::path(filePath).string(), e.what());
        return 0;
    }
}

[[nodiscard]] constexpr auto calculateChunkSize(std::size_t fileSize,
                                                int numChunks) -> std::size_t {
    // Use std::max to ensure we don't divide by zero
    return fileSize / std::max(1, numChunks) +
           (fileSize % std::max(1, numChunks) != 0);
}

template <PathLike P1, PathLike P2>
void splitFile(const P1& filePath, std::size_t chunkSize,
               const P2& outputPattern) {
    LOG_F(
        INFO,
        "splitFile called with filePath: {}, chunkSize: {}, outputPattern: {}",
        fs::path(filePath).string(), chunkSize, std::string(outputPattern));

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            LOG_F(ERROR, "File does not exist: {}", path.string());
            return;
        }

        std::ifstream inputFile(path, std::ios::binary);
        if (!inputFile) {
            LOG_F(ERROR, "Failed to open file: {}", path.string());
            return;
        }

        std::size_t fileSize = getFileSize(path);
        if (fileSize == 0) {
            LOG_F(ERROR, "File is empty or couldn't determine size: {}",
                  path.string());
            return;
        }

        // Use a buffer with smart pointer for automatic cleanup
        auto buffer = std::make_unique<char[]>(chunkSize);
        int partNumber = 0;

        // Process file in chunks
        std::size_t remainingSize = fileSize;
        while (remainingSize > 0) {
            std::ostringstream partFileName;
            std::string outputBase = std::string(outputPattern).empty()
                                         ? path.string()
                                         : std::string(outputPattern);
            partFileName << outputBase << ".part" << partNumber;

            std::ofstream outputFile(partFileName.str(), std::ios::binary);
            if (!outputFile) {
                LOG_F(ERROR, "Failed to create part file: {}",
                      partFileName.str());
                return;
            }

            std::size_t bytesToRead = std::min(chunkSize, remainingSize);
            inputFile.read(buffer.get(), bytesToRead);
            if (inputFile.fail() && !inputFile.eof()) {
                LOG_F(ERROR, "Error reading from file: {}", path.string());
                return;
            }

            outputFile.write(buffer.get(), inputFile.gcount());

            remainingSize -= bytesToRead;
            ++partNumber;
        }

        LOG_F(INFO, "File split into {} parts", partNumber);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error splitting file {}: {}", fs::path(filePath).string(),
              e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error splitting file {}",
              fs::path(filePath).string());
    }
}

template <PathLike P>
void mergeFiles(const P& outputFilePath,
                std::span<const std::string> partFiles) {
    LOG_F(INFO, "mergeFiles called with outputFilePath: {}, partFiles size: {}",
          fs::path(outputFilePath).string(), partFiles.size());

    try {
        fs::path outPath(outputFilePath);

        // Create parent directory if it doesn't exist
        fs::path outDir = outPath.parent_path();
        if (!outDir.empty()) {
            std::error_code ec;
            fs::create_directories(outDir, ec);
            if (ec) {
                LOG_F(ERROR, "Failed to create output directory {}: {}",
                      outDir.string(), ec.message());
                return;
            }
        }

        std::ofstream outputFile(outPath, std::ios::binary);
        if (!outputFile) {
            LOG_F(ERROR, "Failed to create output file: {}", outPath.string());
            return;
        }

        // Use a 64KB buffer for efficient file I/O
        constexpr std::size_t bufferSize = 65536;
        auto buffer = std::make_unique<char[]>(bufferSize);

        // Process each part file
        for (const auto& partFile : partFiles) {
            std::ifstream inputFile(partFile, std::ios::binary);
            if (!inputFile) {
                LOG_F(ERROR, "Failed to open part file: {}", partFile);
                return;
            }

            while (inputFile) {
                inputFile.read(buffer.get(), bufferSize);
                std::streamsize bytesRead = inputFile.gcount();
                if (bytesRead > 0) {
                    outputFile.write(buffer.get(), bytesRead);
                    if (outputFile.fail()) {
                        LOG_F(ERROR, "Error writing to output file: {}",
                              outPath.string());
                        return;
                    }
                }
            }
        }

        LOG_F(INFO, "Files merged successfully into {}", outPath.string());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error merging files to {}: {}",
              fs::path(outputFilePath).string(), e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error merging files to {}",
              fs::path(outputFilePath).string());
    }
}

template <PathLike P1, PathLike P2>
void quickSplit(const P1& filePath, int numChunks, const P2& outputPattern) {
    LOG_F(
        INFO,
        "quickSplit called with filePath: {}, numChunks: {}, outputPattern: {}",
        fs::path(filePath).string(), numChunks, std::string(outputPattern));

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            LOG_F(ERROR, "File does not exist: {}", path.string());
            return;
        }

        std::size_t fileSize = getFileSize(path);
        if (fileSize == 0) {
            LOG_F(ERROR, "File is empty or couldn't determine size: {}",
                  path.string());
            return;
        }

        std::size_t chunkSize = calculateChunkSize(fileSize, numChunks);
        LOG_F(INFO, "Calculated chunk size: {} bytes for {} chunks", chunkSize,
              numChunks);

        splitFile(path, chunkSize, outputPattern);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in quickSplit for {}: {}",
              fs::path(filePath).string(), e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error in quickSplit for {}",
              fs::path(filePath).string());
    }
}

template <PathLike P1, PathLike P2>
void quickMerge(const P1& outputFilePath, const P2& partPattern,
                int numChunks) {
    LOG_F(INFO,
          "quickMerge called with outputFilePath: {}, partPattern: {}, "
          "numChunks: {}",
          fs::path(outputFilePath).string(), std::string(partPattern),
          numChunks);

    try {
        if (numChunks <= 0) {
            LOG_F(ERROR, "Invalid number of chunks: {}", numChunks);
            return;
        }

        std::vector<std::string> partFiles;
        partFiles.reserve(numChunks);

        for (int i = 0; i < numChunks; ++i) {
            std::ostringstream partFileName;
            partFileName << std::string(partPattern) << ".part" << i;
            partFiles.push_back(partFileName.str());
        }

        mergeFiles(outputFilePath, partFiles);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in quickMerge for {}: {}",
              fs::path(outputFilePath).string(), e.what());
    } catch (...) {
        LOG_F(ERROR, "Unknown error in quickMerge for {}",
              fs::path(outputFilePath).string());
    }
}

template <PathLike P>
auto checkPathType(const P& path) -> PathType {
    LOG_F(INFO, "checkPathType called with path: {}", fs::path(path).string());

    try {
        fs::path fsPath(path);
        std::error_code ec;

        if (!fs::exists(fsPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error checking if path exists {}: {}",
                      fsPath.string(), ec.message());
            }
            return PathType::NOT_EXISTS;
        }

        if (fs::is_regular_file(fsPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error checking if path is regular file {}: {}",
                      fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::REGULAR_FILE;
        }

        if (fs::is_directory(fsPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error checking if path is directory {}: {}",
                      fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::DIRECTORY;
        }

        if (fs::is_symlink(fsPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error checking if path is symlink {}: {}",
                      fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::SYMLINK;
        }

        return PathType::OTHER;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error in checkPathType for {}: {}",
              fs::path(path).string(), e.what());
        return PathType::OTHER;
    }
}

template <PathLike P>
auto countLinesInFile(const P& filePath) -> std::optional<int> {
    LOG_F(INFO, "countLinesInFile called with filePath: {}",
          fs::path(filePath).string());

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            LOG_F(ERROR, "File does not exist: {}", path.string());
            return std::nullopt;
        }

        if (!fs::is_regular_file(path)) {
            LOG_F(ERROR, "Path is not a regular file: {}", path.string());
            return std::nullopt;
        }

        std::ifstream file(path);
        if (!file) {
            LOG_F(ERROR, "Failed to open file: {}", path.string());
            return std::nullopt;
        }

        int lineCount = 0;
        std::string line;

        // Count lines using std::getline
        // This is more portable than alternatives like GNU extensions
        while (std::getline(file, line)) {
            ++lineCount;
        }

        if (file.bad()) {
            LOG_F(ERROR, "Error reading file: {}", path.string());
            return std::nullopt;
        }

        LOG_F(INFO, "File {} has {} lines", path.string(), lineCount);
        return lineCount;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error counting lines in {}: {}",
              fs::path(filePath).string(), e.what());
        return std::nullopt;
    }
}

template <PathLike P>
auto searchExecutableFiles(const P& dir, std::string_view searchStr)
    -> std::vector<fs::path> {
    LOG_F(INFO, "searchExecutableFiles called with dir: {}, searchStr: {}",
          fs::path(dir).string(), std::string(searchStr));

    std::vector<fs::path> matchedFiles;

    try {
        fs::path dirPath(dir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            LOG_F(ERROR, "Directory does not exist or is not a directory: {}",
                  dirPath.string());
            return matchedFiles;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error iterating directory {}: {}",
                      dirPath.string(), ec.message());
                continue;
            }

            // Check if the entry is a regular file
            if (entry.is_regular_file(ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error checking if {} is a regular file: {}",
                          entry.path().string(), ec.message());
                    continue;
                }

                // Check if the file is executable
                if (isExecutableFile(entry.path(), "")) {
                    // Check if the filename contains the search string
                    const auto& fileName = entry.path().filename().string();
                    if (fileName.find(searchStr) != std::string::npos) {
                        matchedFiles.push_back(entry.path());
                        LOG_F(INFO, "Found matching executable file: {}",
                              entry.path().string());
                    }
                }
            }
        }

        LOG_F(INFO, "Found {} matching executable files", matchedFiles.size());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error searching for executable files in {}: {}",
              fs::path(dir).string(), e.what());
    }

    return matchedFiles;
}

template <PathLike P>
auto classifyFiles(const P& directory)
    -> std::unordered_map<std::string, std::vector<std::string>> {
    LOG_F(INFO, "classifyFiles called with directory: {}",
          fs::path(directory).string());

    std::unordered_map<std::string, std::vector<std::string>> fileMap;

    try {
        fs::path dirPath(directory);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            LOG_F(ERROR, "Directory does not exist or is not a directory: {}",
                  dirPath.string());
            return fileMap;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                LOG_F(ERROR, "Error iterating directory {}: {}",
                      dirPath.string(), ec.message());
                continue;
            }

            if (entry.is_regular_file(ec)) {
                if (ec) {
                    LOG_F(ERROR, "Error checking if {} is a regular file: {}",
                          entry.path().string(), ec.message());
                    continue;
                }

                std::string extension = entry.path().extension().string();
                if (extension.empty()) {
                    extension = "<no extension>";
                }

                fileMap[extension].push_back(entry.path().string());
            }
        }

        // Use modern C++20 features to report on the classification
        LOG_F(INFO, "Classified files into {} categories:", fileMap.size());
        for (const auto& [ext, files] : fileMap) {
            LOG_F(INFO, "  - {} files with extension '{}'", files.size(), ext);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error classifying files in {}: {}",
              fs::path(directory).string(), e.what());
    }

    return fileMap;
}

}  // namespace atom::io

#endif  // ATOM_IO_IO_IMPL_HPP
