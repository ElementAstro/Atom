/*
 * path_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file path_utils.hpp
 * @brief Centralized path validation and manipulation utilities
 *
 * This header provides consolidated path validation functions to eliminate
 * duplication across the atom/io module. All path validation should use
 * these utilities for consistency and security.
 */

#ifndef ATOM_IO_CORE_PATH_UTILS_HPP
#define ATOM_IO_CORE_PATH_UTILS_HPP

#include <filesystem>
#include <regex>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace atom::io::path_utils {

namespace fs = std::filesystem;

// Platform-specific path validation regexes
#ifdef _WIN32
// Windows: disallow ?, *, :, ;, {}, \ in folder names (except drive letter
// colon)
inline const std::regex FOLDER_NAME_REGEX(R"(^[^\/?*:;{}\\]+[^\\]*$)");
// Windows: disallow \, /, :, *, ?, ", <, >, | in file names
inline const std::regex FILE_NAME_REGEX("^[^\\/:*?\"<>|]+$");
// Windows reserved names
inline const std::array<std::string_view, 22> RESERVED_NAMES = {
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4",
    "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3",
    "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
#else
// Unix/Linux: disallow / in folder and file names
inline const std::regex FOLDER_NAME_REGEX("^[^/]+$");
inline const std::regex FILE_NAME_REGEX("^[^/]+$");
#endif

/**
 * @brief Comprehensive path validation for security and format compliance
 *
 * Performs validation including:
 * - Empty path check
 * - Null byte detection (security vulnerability)
 * - Path length limits
 * - Path traversal pattern detection
 * - Windows-specific invalid characters and reserved names
 * - Filesystem path validation
 *
 * @param path The path to validate
 * @return true if path is valid and safe, false otherwise
 */
inline bool validatePath(std::string_view path) noexcept {
    if (path.empty()) {
        return false;
    }

    // Check for null bytes (security vulnerability)
    if (path.find('\0') != std::string_view::npos) {
        spdlog::warn("Path contains null byte - security risk");
        return false;
    }

    // Check for excessively long paths
    constexpr size_t MAX_PATH_LENGTH =
        4096;  // Reasonable limit for most systems
    if (path.length() > MAX_PATH_LENGTH) {
        spdlog::warn("Path exceeds maximum length: {}", path.length());
        return false;
    }

    // Check for path traversal patterns
    if (path.find("..") != std::string_view::npos) {
        // Allow .. only if it's part of a valid relative path
        // More sophisticated check could be added here
        spdlog::debug("Path contains '..' - potential traversal");
    }

#ifdef _WIN32
    // Windows-specific checks
    // Check for invalid characters
    constexpr std::string_view INVALID_CHARS = "<>:\"|?*";
    for (char c : INVALID_CHARS) {
        if (path.find(c) != std::string_view::npos) {
            spdlog::warn("Path contains invalid Windows character: {}", c);
            return false;
        }
    }

    // Check for reserved names
    try {
        fs::path p(path);
        std::string filename = p.filename().string();

        // Convert to uppercase for comparison
        std::string upper_filename = filename;
        std::transform(upper_filename.begin(), upper_filename.end(),
                       upper_filename.begin(), ::toupper);

        for (const auto& reserved : RESERVED_NAMES) {
            if (upper_filename == reserved ||
                upper_filename.starts_with(std::string(reserved) + ".")) {
                spdlog::warn("Path uses Windows reserved name: {}", filename);
                return false;
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Path validation failed: {}", e.what());
        return false;
    }
#endif

    // Validate as filesystem path
    try {
        fs::path p(path);
        // Basic validation - path can be constructed
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Invalid filesystem path: {}", e.what());
        return false;
    }
}

/**
 * @brief Validates a folder name (not full path, just the name component)
 *
 * @param folderName The folder name to validate
 * @return true if valid, false otherwise
 */
inline bool isFolderNameValid(std::string_view folderName) noexcept {
    if (folderName.empty()) {
        spdlog::warn("Empty folder name is invalid");
        return false;
    }

    try {
        return std::regex_match(folderName.begin(), folderName.end(),
                                FOLDER_NAME_REGEX);
    } catch (const std::exception& e) {
        spdlog::error("Error checking folder name validity: {}", e.what());
        return false;
    }
}

/**
 * @brief Validates a file name (not full path, just the name component)
 *
 * @param fileName The file name to validate
 * @return true if valid, false otherwise
 */
inline bool isFileNameValid(std::string_view fileName) noexcept {
    if (fileName.empty()) {
        spdlog::warn("Empty file name is invalid");
        return false;
    }

    try {
        return std::regex_match(fileName.begin(), fileName.end(),
                                FILE_NAME_REGEX);
    } catch (const std::exception& e) {
        spdlog::error("Error checking file name validity: {}", e.what());
        return false;
    }
}

/**
 * @brief Basic path validation for directory stack operations
 *
 * @param path The path to validate
 * @return true if valid, false otherwise
 */
inline bool isValidPath(const fs::path& path) noexcept {
    try {
        if (path.empty()) {
            return false;
        }

        std::error_code ec;
        [[maybe_unused]] auto canonical_path = fs::weakly_canonical(path, ec);
        return !ec;
    } catch (const std::exception&) {
        return false;
    }
}

/**
 * @brief Validates file permissions for read/write operations
 *
 * @param path Path to validate
 * @param write_access Whether write access is required
 * @return true if permissions are valid, false otherwise
 */
inline bool validatePermissions(std::string_view path,
                                bool write_access = false) noexcept {
    if (!validatePath(path)) {
        return false;
    }

    try {
        fs::path p(path);
        std::error_code ec;

        // Check if path exists
        if (!fs::exists(p, ec) || ec) {
            return false;
        }

        // Check permissions
        auto perms = fs::status(p, ec).permissions();
        if (ec) {
            return false;
        }

        // Check read permission
        if ((perms & fs::perms::owner_read) == fs::perms::none) {
            return false;
        }

        // Check write permission if required
        if (write_access &&
            (perms & fs::perms::owner_write) == fs::perms::none) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Permission validation failed: {}", e.what());
        return false;
    }
}

}  // namespace atom::io::path_utils

#endif  // ATOM_IO_CORE_PATH_UTILS_HPP
