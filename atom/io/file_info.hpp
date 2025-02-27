// FILE: file_info.cpp

#ifndef FILE_INFO_HPP
#define FILE_INFO_HPP

#include <ctime>
#include <filesystem>
#include <string>

#include "atom/macro.hpp"

namespace atom::io {
namespace fs = std::filesystem;

/**
 * @brief Structure to store detailed file information.
 */
struct FileInfo {
    std::string filePath;     ///< Absolute path of the file.
    std::string fileName;     ///< Name of the file.
    std::string extension;    ///< File extension.
    std::uintmax_t fileSize;  ///< Size of the file in bytes.
    std::string
        fileType;  ///< Type of the file (e.g., Regular file, Directory).
    std::string creationTime;      ///< Creation timestamp.
    std::string lastModifiedTime;  ///< Last modification timestamp.
    std::string lastAccessTime;    ///< Last access timestamp.
    std::string permissions;       ///< File permissions (e.g., rwxr-xr-x).
    bool isHidden;                 ///< Indicates if the file is hidden.
#ifdef _WIN32
    std::string owner;  ///< Owner of the file (Windows only).
#else
    std::string owner;          ///< Owner of the file (Linux only).
    std::string group;          ///< Group of the file (Linux only).
    std::string symlinkTarget;  ///< Target of the symbolic link, if applicable.
#endif
} ATOM_ALIGNAS(128);

/**
 * @brief Retrieves detailed information about a file.
 *
 * @param filePath The path to the file.
 * @return FileInfo structure containing the file's information.
 * @throws std::runtime_error if the file does not exist or cannot be accessed.
 */
FileInfo getFileInfo(const fs::path& filePath);

/**
 * @brief Prints the file information to the console.
 *
 * @param info The FileInfo structure containing file details.
 */
void printFileInfo(const FileInfo& info);

/**
 * @brief Changes the permissions of a file.
 *
 * @param filePath The path to the file.
 * @param permissions The new permissions string (e.g., "rwxr-xr-x").
 * @throws std::runtime_error if the permissions cannot be changed.
 */
void changeFilePermissions(const fs::path& filePath,
                           const std::string& permissions);

/**
 * @brief Renames a file.
 *
 * @param oldPath The current path of the file.
 * @param newPath The new path/name for the file.
 * @throws std::runtime_error if the file cannot be renamed.
 */
void renameFile(const fs::path& oldPath, const fs::path& newPath);

/**
 * @brief Deletes a file.
 *
 * @param filePath The path to the file.
 * @throws std::runtime_error if the file cannot be deleted.
 */
void deleteFile(const fs::path& filePath);

// Updated to incorporate C++20 features, robust exception handling, input
// validation, and modern C++ best practices

}  // namespace atom::io

#endif  // FILE_INFO_HPP
