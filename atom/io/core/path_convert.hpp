/*
 * path_convert.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file path_convert.hpp
 * @brief Path conversion, normalization, and name validation declarations.
 */

#ifndef ATOM_IO_CORE_PATH_CONVERT_HPP
#define ATOM_IO_CORE_PATH_CONVERT_HPP

#include <string>
#include <string_view>

namespace atom::io {

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
 * @brief Get the executable name from the path.
 *
 * @param path The path of the executable.
 * @return The executable name.
 */
[[nodiscard]] auto getExecutableNameFromPath(std::string_view path)
    -> std::string;

}  // namespace atom::io

#endif  // ATOM_IO_CORE_PATH_CONVERT_HPP
