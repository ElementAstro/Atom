#ifndef ATOM_IO_FILESYSTEM_FILE_OPS_HPP
#define ATOM_IO_FILESYSTEM_FILE_OPS_HPP

#include "file_info.hpp"
#include "atom/io/core/file_ops.hpp"

namespace atom::io {

/**
 * @brief Prints the file information to the console.
 *
 * @param info The FileInfo structure containing file details.
 */
void printFileInfo(const FileInfo& info);

/**
 * @brief Deletes a file.
 * @deprecated Use removeFile() from atom/io/core/file_ops.hpp instead.
 * @param filePath The path to the file to delete.
 * @throws std::runtime_error if the file cannot be deleted.
 */
[[deprecated("Use removeFile() from atom/io/core/file_ops.hpp instead")]]
void deleteFile(const fs::path& filePath);

}  // namespace atom::io

#endif  // ATOM_IO_FILESYSTEM_FILE_OPS_HPP
