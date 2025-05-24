#ifndef ATOM_IO_FILE_INFO_HPP
#define ATOM_IO_FILE_INFO_HPP

#include <filesystem>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::io {
namespace fs = std::filesystem;

using atom::containers::String;

/**
 * @brief Structure to store detailed file information.
 */
struct FileInfo {
    String filePath;          ///< Absolute path of the file.
    String fileName;          ///< Name of the file.
    String extension;         ///< File extension.
    std::uintmax_t fileSize;  ///< Size of the file in bytes.
    String fileType;      ///< Type of the file (e.g., Regular file, Directory).
    String creationTime;  ///< Creation timestamp.
    String lastModifiedTime;  ///< Last modification timestamp.
    String lastAccessTime;    ///< Last access timestamp.
    String permissions;       ///< File permissions (e.g., rwxr-xr-x).
    bool isHidden;            ///< Indicates if the file is hidden.
#ifdef _WIN32
    String owner;  ///< Owner of the file (Windows only).
#else
    String owner;          ///< Owner of the file (Linux only).
    String group;          ///< Group of the file (Linux only).
    String symlinkTarget;  ///< Target of the symbolic link, if applicable.
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

}  // namespace atom::io

#endif  // ATOM_IO_FILE_INFO_HPP
