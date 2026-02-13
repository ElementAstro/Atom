#ifndef ATOM_IO_FILESYSTEM_FILE_PERMISSION_CHANGE_HPP
#define ATOM_IO_FILESYSTEM_FILE_PERMISSION_CHANGE_HPP

#include <filesystem>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

/**
 * @brief Modify file permissions using permission string
 * @param filePath Filesystem path to the target file
 * @param permissions Permission string in format "rwxrwxrwx"
 * @throws std::invalid_argument If permission string format is invalid
 * @throws std::runtime_error If file doesn't exist or permission change fails
 */
void changeFilePermissions(const std::filesystem::path &filePath,
                           const atom::containers::String &permissions);

}  // namespace atom::io

#endif  // ATOM_IO_FILESYSTEM_FILE_PERMISSION_CHANGE_HPP
