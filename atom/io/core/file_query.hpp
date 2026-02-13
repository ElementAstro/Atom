/*
 * file_query.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file file_query.hpp
 * @brief File and directory existence checks, info queries, and classification.
 */

#ifndef ATOM_IO_CORE_FILE_QUERY_HPP
#define ATOM_IO_CORE_FILE_QUERY_HPP

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#include <spdlog/spdlog.h>

#include "atom/io/core/types.hpp"

namespace atom::io {

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
 * @deprecated Use fileSize() from file_ops.hpp instead.
 * @param filePath The file path.
 * @return The file size.
 */
template <PathLike P>
[[deprecated("Use fileSize() from atom/io/core/file_ops.hpp instead")]]
auto getFileSize(const P& filePath) -> std::size_t;

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

// ---------------------------------------------------------------------------
// Template implementations
// ---------------------------------------------------------------------------

template <PathLike P>
[[nodiscard]] auto isFolderExists(const P& folderPath) -> bool {
    spdlog::info("isFolderExists called with folderPath: {}",
                 fs::path(folderPath).string());

    try {
        fs::path path(folderPath);
        std::error_code ec;
        bool result = fs::exists(path, ec) && fs::is_directory(path, ec);
        if (ec) {
            spdlog::error("Error checking if folder exists {}: {}",
                          path.string(), ec.message());
            return false;
        }

        spdlog::info("isFolderExists returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in isFolderExists for {}: {}",
                      fs::path(folderPath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isFileExists(const P& filePath) -> bool {
    spdlog::info("isFileExists called with filePath: {}",
                 fs::path(filePath).string());

    try {
        fs::path path(filePath);
        std::error_code ec;
        bool result = fs::exists(path, ec) && fs::is_regular_file(path, ec);
        if (ec) {
            spdlog::error("Error checking if file exists {}: {}", path.string(),
                          ec.message());
            return false;
        }

        spdlog::info("isFileExists returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in isFileExists for {}: {}",
                      fs::path(filePath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isFolderEmpty(const P& folderPath) -> bool {
    spdlog::info("isFolderEmpty called with folderPath: {}",
                 fs::path(folderPath).string());

    try {
        fs::path path(folderPath);
        if (!isFolderExists(path)) {
            spdlog::warn("Folder does not exist: {}", path.string());
            return false;
        }

        std::error_code ec;
        bool result = fs::is_empty(path, ec);
        if (ec) {
            spdlog::error("Error checking if folder is empty {}: {}",
                          path.string(), ec.message());
            return false;
        }

        spdlog::info("isFolderEmpty returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in isFolderEmpty for {}: {}",
                      fs::path(folderPath).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto isAbsolutePath(const P& path) -> bool {
    spdlog::info("isAbsolutePath called with path: {}",
                 fs::path(path).string());

    try {
        bool result = fs::path(path).is_absolute();
        spdlog::info("isAbsolutePath returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in isAbsolutePath for {}: {}",
                      fs::path(path).string(), e.what());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] auto changeWorkingDirectory(const P& directoryPath) -> bool {
    spdlog::info("changeWorkingDirectory called with directoryPath: {}",
                 fs::path(directoryPath).string());

    try {
        fs::path path(directoryPath);
        if (!isFolderExists(path)) {
            spdlog::error("Directory does not exist: {}", path.string());
            return false;
        }

        std::error_code ec;
        fs::current_path(path, ec);
        if (ec) {
            spdlog::error("Failed to change working directory to {}: {}",
                          path.string(), ec.message());
            return false;
        }

        spdlog::info("Changed working directory to: {}", path.string());
        return true;
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Failed to change working directory to {}: {}",
                      fs::path(directoryPath).string(), e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error changing working directory to {}: {}",
                      fs::path(directoryPath).string(), e.what());
        return false;
    } catch (...) {
        spdlog::error("Unknown error changing working directory to {}",
                      fs::path(directoryPath).string());
        return false;
    }
}

template <PathLike P>
[[nodiscard]] std::pair<std::string, std::string> getFileTimes(
    const P& filePath) {
    spdlog::info("getFileTimes called with filePath: {}",
                 fs::path(filePath).string());
    std::pair<std::string, std::string> fileTimes;

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            spdlog::error("File does not exist: {}", path.string());
            return fileTimes;
        }

        std::error_code ec;
        auto writeTime = fs::last_write_time(path, ec);
        if (ec) {
            spdlog::error("Error getting last write time for {}: {}",
                          path.string(), ec.message());
            return fileTimes;
        }

        // Get file creation time using platform-specific APIs
#if defined(_WIN32)
        // Windows implementation using GetFileAttributesExW
        try {
            WIN32_FILE_ATTRIBUTE_DATA fileInfo;
            if (GetFileAttributesExW(path.wstring().c_str(),
                                     GetFileExInfoStandard, &fileInfo)) {
                SYSTEMTIME sysTime;
                FILETIME creationTime = fileInfo.ftCreationTime;

                if (FileTimeToSystemTime(&creationTime, &sysTime)) {
                    std::array<char, 100> buffer{};
                    int written =
                        snprintf(buffer.data(), buffer.size(),
                                 "%04d-%02d-%02d %02d:%02d:%02d", sysTime.wYear,
                                 sysTime.wMonth, sysTime.wDay, sysTime.wHour,
                                 sysTime.wMinute, sysTime.wSecond);
                    if (written > 0 &&
                        static_cast<size_t>(written) < buffer.size()) {
                        fileTimes.first = std::string(buffer.data());
                    } else {
                        fileTimes.first = "Unavailable";
                    }
                } else {
                    fileTimes.first = "Unavailable";
                }
            } else {
                spdlog::warn("Failed to get Windows file attributes for: {}",
                             path.string());
                fileTimes.first = "Unavailable";
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception while getting Windows creation time: {}",
                          e.what());
            fileTimes.first = "Unavailable";
        }
#else
        // Unix/Linux implementation using stat
        try {
            struct stat fileStat;
            if (stat(path.string().c_str(), &fileStat) == 0) {
#ifdef __APPLE__
                // macOS has birth time
                auto time_val = fileStat.st_birthtimespec.tv_sec;
#elif defined(__linux__)
                // Linux uses ctime (change time, closest to creation)
                auto time_val = fileStat.st_ctim.tv_sec;
#else
                // Fallback for other Unix systems
                auto time_val = fileStat.st_ctime;
#endif
                std::string timeStr = std::ctime(&time_val);
                if (!timeStr.empty() && timeStr.back() == '\n') {
                    timeStr.pop_back();
                }
                fileTimes.first = timeStr;
            } else {
                spdlog::warn("Failed to get file stat for: {}", path.string());
                fileTimes.first = "Unavailable";
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception while getting Unix creation time: {}",
                          e.what());
            fileTimes.first = "Unavailable";
        }
#endif

        // Convert last_write_time to string
        // Convert file_clock to system_clock in a C++20-portable way
        auto systemTime =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                writeTime - fs::file_time_type::clock::now() +
                std::chrono::system_clock::now());
#if __cpp_lib_format >= 202106L
        fileTimes.second = std::format("{:%Y-%m-%d %H:%M:%S}", systemTime);
#else
        // Fallback for environments without std::format
        auto timeT = std::chrono::system_clock::to_time_t(systemTime);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
        fileTimes.second = ss.str();
#endif

        spdlog::info("getFileTimes returning: modification time: {}",
                     fileTimes.second);
        return fileTimes;
    } catch (const std::exception& e) {
        spdlog::error("Error getting file times for {}: {}",
                      fs::path(filePath).string(), e.what());
        return fileTimes;
    }
}

template <PathLike P>
[[nodiscard]] auto checkFileTypeInFolder(
    const P& folderPath, std::span<const std::string> fileTypes,
    FileOption fileOption) -> std::vector<std::string> {
    spdlog::info("checkFileTypeInFolder called with folderPath: {}",
                 fs::path(folderPath).string());

    std::vector<std::string> files;

    try {
        fs::path path(folderPath);
        if (!isFolderExists(path)) {
            spdlog::error("Folder does not exist: {}", path.string());
            return files;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(path, ec)) {
            if (ec) {
                spdlog::error("Error iterating directory {}: {}", path.string(),
                              ec.message());
                continue;
            }

            if (entry.is_regular_file(ec)) {
                if (ec) {
                    spdlog::error("Error checking if {} is regular file: {}",
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
        spdlog::error("Failed to check files in folder {}: {}",
                      fs::path(folderPath).string(), ex.what());
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error checking files in folder {}: {}",
                      fs::path(folderPath).string(), e.what());
    }

    spdlog::info("checkFileTypeInFolder returning {} files", files.size());
    return files;
}

template <PathLike P1, PathLike P2>
auto isExecutableFile(const P1& fileName, const P2& fileExt) -> bool {
    spdlog::info("isExecutableFile called with fileName: {}, fileExt: {}",
                 fs::path(fileName).string(), std::string(fileExt));

    try {
#ifdef _WIN32
        fs::path filePath = fs::path(fileName).string() + std::string(fileExt);
#else
        fs::path filePath = fileName;
#endif

        spdlog::info("Checking file '{}'.", filePath.string());
        std::error_code ec;

        // Check if file exists and is regular
        if (!fs::exists(filePath, ec) || ec) {
            spdlog::warn("The file '{}' does not exist: {}", filePath.string(),
                         ec ? ec.message() : "");
            return false;
        }

        if (!fs::is_regular_file(filePath, ec) || ec) {
            spdlog::warn("The path '{}' is not a regular file: {}",
                         filePath.string(), ec ? ec.message() : "");
            return false;
        }

#ifndef _WIN32
        // On Unix-like systems, check execute permissions
        fs::perms p = fs::status(filePath, ec).permissions();
        if (ec) {
            spdlog::warn("Error getting permissions for '{}': {}",
                         filePath.string(), ec.message());
            return false;
        }

        if ((p & fs::perms::owner_exec) == fs::perms::none) {
            spdlog::warn("The file '{}' is not executable.", filePath.string());
            return false;
        }
#endif

        spdlog::info("The file '{}' exists and is executable.",
                     filePath.string());
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error checking if file is executable {}: {}",
                      fs::path(fileName).string(), e.what());
        return false;
    }
}

template <PathLike P>
auto getFileSize(const P& filePath) -> std::size_t {
    spdlog::info("getFileSize called with filePath: {}",
                 fs::path(filePath).string());

    try {
        fs::path path(filePath);
        std::error_code ec;
        std::size_t size = fs::file_size(path, ec);
        if (ec) {
            spdlog::error("Error getting file size for {}: {}", path.string(),
                          ec.message());
            return 0;
        }

        spdlog::info("getFileSize returning: {}", size);
        return size;
    } catch (const std::exception& e) {
        spdlog::error("Error getting file size for {}: {}",
                      fs::path(filePath).string(), e.what());
        return 0;
    }
}

template <PathLike P>
auto checkPathType(const P& path) -> PathType {
    spdlog::info("checkPathType called with path: {}", fs::path(path).string());

    try {
        fs::path fsPath(path);
        std::error_code ec;

        if (!fs::exists(fsPath, ec)) {
            if (ec) {
                spdlog::error("Error checking if path exists {}: {}",
                              fsPath.string(), ec.message());
            }
            return PathType::NOT_EXISTS;
        }

        if (fs::is_regular_file(fsPath, ec)) {
            if (ec) {
                spdlog::error("Error checking if path is regular file {}: {}",
                              fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::REGULAR_FILE;
        }

        if (fs::is_directory(fsPath, ec)) {
            if (ec) {
                spdlog::error("Error checking if path is directory {}: {}",
                              fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::DIRECTORY;
        }

        if (fs::is_symlink(fsPath, ec)) {
            if (ec) {
                spdlog::error("Error checking if path is symlink {}: {}",
                              fsPath.string(), ec.message());
                return PathType::OTHER;
            }
            return PathType::SYMLINK;
        }

        return PathType::OTHER;
    } catch (const std::exception& e) {
        spdlog::error("Error in checkPathType for {}: {}",
                      fs::path(path).string(), e.what());
        return PathType::OTHER;
    }
}

template <PathLike P>
auto countLinesInFile(const P& filePath) -> std::optional<int> {
    spdlog::info("countLinesInFile called with filePath: {}",
                 fs::path(filePath).string());

    try {
        fs::path path(filePath);
        if (!fs::exists(path)) {
            spdlog::error("File does not exist: {}", path.string());
            return std::nullopt;
        }

        if (!fs::is_regular_file(path)) {
            spdlog::error("Path is not a regular file: {}", path.string());
            return std::nullopt;
        }

        std::ifstream file(path);
        if (!file) {
            spdlog::error("Failed to open file: {}", path.string());
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
            spdlog::error("Error reading file: {}", path.string());
            return std::nullopt;
        }

        spdlog::info("File {} has {} lines", path.string(), lineCount);
        return lineCount;
    } catch (const std::exception& e) {
        spdlog::error("Error counting lines in {}: {}",
                      fs::path(filePath).string(), e.what());
        return std::nullopt;
    }
}

template <PathLike P>
auto searchExecutableFiles(const P& dir, std::string_view searchStr)
    -> std::vector<fs::path> {
    spdlog::info("searchExecutableFiles called with dir: {}, searchStr: {}",
                 fs::path(dir).string(), std::string(searchStr));

    std::vector<fs::path> matchedFiles;

    try {
        fs::path dirPath(dir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            spdlog::error("Directory does not exist or is not a directory: {}",
                          dirPath.string());
            return matchedFiles;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                spdlog::error("Error iterating directory {}: {}",
                              dirPath.string(), ec.message());
                continue;
            }

            // Check if the entry is a regular file
            if (entry.is_regular_file(ec)) {
                if (ec) {
                    spdlog::error("Error checking if {} is a regular file: {}",
                                  entry.path().string(), ec.message());
                    continue;
                }

                // Check if the file is executable
                if (isExecutableFile(entry.path(), "")) {
                    // Check if the filename contains the search string
                    const auto& fileName = entry.path().filename().string();
                    if (fileName.find(searchStr) != std::string::npos) {
                        matchedFiles.push_back(entry.path());
                        spdlog::info("Found matching executable file: {}",
                                     entry.path().string());
                    }
                }
            }
        }

        spdlog::info("Found {} matching executable files", matchedFiles.size());
    } catch (const std::exception& e) {
        spdlog::error("Error searching for executable files in {}: {}",
                      fs::path(dir).string(), e.what());
    }

    return matchedFiles;
}

template <PathLike P>
auto classifyFiles(const P& directory)
    -> std::unordered_map<std::string, std::vector<std::string>> {
    spdlog::info("classifyFiles called with directory: {}",
                 fs::path(directory).string());

    std::unordered_map<std::string, std::vector<std::string>> fileMap;

    try {
        fs::path dirPath(directory);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            spdlog::error("Directory does not exist or is not a directory: {}",
                          dirPath.string());
            return fileMap;
        }

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                spdlog::error("Error iterating directory {}: {}",
                              dirPath.string(), ec.message());
                continue;
            }

            if (entry.is_regular_file(ec)) {
                if (ec) {
                    spdlog::error("Error checking if {} is a regular file: {}",
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
        spdlog::info("Classified files into {} categories:", fileMap.size());
        for (const auto& [ext, files] : fileMap) {
            spdlog::info("  - {} files with extension '{}'", files.size(), ext);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error classifying files in {}: {}",
                      fs::path(directory).string(), e.what());
    }

    return fileMap;
}

}  // namespace atom::io

#endif  // ATOM_IO_CORE_FILE_QUERY_HPP
