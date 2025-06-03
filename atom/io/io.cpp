/*
 * io.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "io.hpp"

#include <algorithm>
#include <filesystem>
#include <regex>
#include <string_view>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"
#include "atom/type/json.hpp"

#ifdef __linux
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

#ifdef _WIN32
#include <windows.h>
const std::regex FOLDER_NAME_REGEX(R"(^[^\/?*:;{}\\]+[^\\]*$)");
const std::regex FILE_NAME_REGEX("^[^\\/:*?\"<>|]+$");
#else
const std::regex FOLDER_NAME_REGEX("^[^/]+$");
const std::regex FILE_NAME_REGEX("^[^/]+$");
#endif

namespace atom::io {

// These non-templated functions are kept in the .cpp file
auto convertToLinuxPath(std::string_view windows_path) -> std::string {
    spdlog::info("convertToLinuxPath called with windows_path: {}",
                 windows_path);

    try {
        std::string linuxPath(windows_path);
        std::ranges::replace(linuxPath, '\\', '/');

        // Convert drive letter to lowercase (e.g. C: -> c:)
        if (linuxPath.length() >= 2 && linuxPath[1] == ':') {
            linuxPath[0] = std::tolower(linuxPath[0]);
        }

        spdlog::info("Converted to Linux path: {}", linuxPath);
        return linuxPath;
    } catch (const std::exception& e) {
        spdlog::error("Error converting to Linux path: {}", e.what());
        return std::string(windows_path);
    }
}

auto convertToWindowsPath(std::string_view linux_path) -> std::string {
    spdlog::info("convertToWindowsPath called with linux_path: {}", linux_path);

    try {
        std::string windowsPath(linux_path);
        std::ranges::replace(windowsPath, '/', '\\');

        // Convert drive letter to uppercase (e.g. c: -> C:)
        if (windowsPath.length() >= 2 && std::islower(windowsPath[0]) &&
            windowsPath[1] == ':') {
            windowsPath[0] = std::toupper(windowsPath[0]);
        }

        spdlog::info("Converted to Windows path: {}", windowsPath);
        return windowsPath;
    } catch (const std::exception& e) {
        spdlog::error("Error converting to Windows path: {}", e.what());
        return std::string(linux_path);
    }
}

auto normPath(std::string_view raw_path) -> std::string {
    spdlog::info("normPath called with raw_path: {}", raw_path);

    try {
        // Normalize path separators first
        std::string path(raw_path);
        char preferred_separator = fs::path::preferred_separator;

        if (preferred_separator == '/') {
            std::ranges::replace(path, '\\', '/');
        } else {
            std::ranges::replace(path, '/', '\\');
        }

        fs::path normalized;
        fs::path input_path(path);

        // Handle absolute paths specially
        bool is_absolute = input_path.is_absolute();

        for (const auto& part : input_path) {
            std::string part_str = part.string();

            if (part_str == ".") {
                // Skip current directory markers
                continue;
            } else if (part_str == "..") {
                // Go up one level if not at root already
                if (!normalized.empty() && normalized.filename() != "..") {
                    normalized = normalized.parent_path();
                } else if (!is_absolute) {
                    // Can't go up further if at root of absolute path
                    normalized /= part;
                }
            } else {
                normalized /= part;
            }
        }

        std::string result = normalized.string();
        if (result.empty() && is_absolute) {
            // Return root path for absolute paths that normalize to empty
            result = preferred_separator == '/' ? "/" : "C:\\";
        }

        spdlog::info("Normalized path: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error normalizing path: {}", e.what());
        return std::string(raw_path);
    }
}

auto isFolderNameValid(std::string_view folderName) -> bool {
    spdlog::info("isFolderNameValid called with folderName: {}", folderName);

    try {
        if (folderName.empty()) {
            spdlog::warn("Empty folder name is invalid");
            return false;
        }

        bool result = std::regex_match(folderName.begin(), folderName.end(),
                                       FOLDER_NAME_REGEX);
        spdlog::info("isFolderNameValid returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error checking folder name validity: {}", e.what());
        return false;
    }
}

auto isFileNameValid(std::string_view fileName) -> bool {
    spdlog::info("isFileNameValid called with fileName: {}", fileName);

    try {
        if (fileName.empty()) {
            spdlog::warn("Empty file name is invalid");
            return false;
        }

        bool result =
            std::regex_match(fileName.begin(), fileName.end(), FILE_NAME_REGEX);
        spdlog::info("isFileNameValid returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Error checking file name validity: {}", e.what());
        return false;
    }
}

auto getExecutableNameFromPath(std::string_view path) -> std::string {
    spdlog::info("getExecutableNameFromPath called with path: {}", path);

    if (path.empty()) {
        spdlog::error("The provided path is empty");
        THROW_INVALID_ARGUMENT("The provided path is empty");
    }

    try {
        // Platform-independent path separator detection
        const std::string path_separators =
#ifdef _WIN32
            "/\\";
#else
            "/";
#endif

        size_t lastSlashPos = path.find_last_of(path_separators);

        if (lastSlashPos == std::string_view::npos) {
            // No path separator, treat the whole string as filename
            if (path.find('.') == std::string_view::npos) {
                spdlog::error(
                    "The provided path does not contain a valid file name "
                    "with extension");
                THROW_INVALID_ARGUMENT(
                    "The provided path does not contain a valid file name with "
                    "extension");
            }
            spdlog::info("Returning path as file name: {}", path);
            return std::string(path);
        }

        std::string fileName(path.substr(lastSlashPos + 1));
        spdlog::info("Extracted file name: {}", fileName);

        if (fileName.empty()) {
            spdlog::error(
                "The provided path ends with a separator and contains no "
                "file name");
            THROW_INVALID_ARGUMENT(
                "The provided path ends with a separator and contains no file "
                "name");
        }

        size_t dotPos = fileName.find_last_of('.');
        if (dotPos == std::string::npos) {
            spdlog::error("The file name does not contain an extension");
            THROW_INVALID_ARGUMENT(
                "The file name does not contain an extension");
        }

        spdlog::info("Returning file name: {}", fileName);
        return fileName;
    } catch (const atom::error::Exception& e) {
        // Pass through our custom exceptions
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Unexpected error in getExecutableNameFromPath: {}",
                      e.what());
        THROW_RUNTIME_ERROR(std::string("Error extracting executable name: ") +
                            e.what());
    }
}

}  // namespace atom::io
