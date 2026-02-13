/*
 * backup.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Backup, restore, and async file processing operations

**************************************************/

#ifndef ATOM_IO_COMPRESSION_BACKUP_HPP
#define ATOM_IO_COMPRESSION_BACKUP_HPP

#include <future>
#include <string_view>

#include "types.hpp"

namespace atom::io {

/**
 * @brief Processes multiple files asynchronously
 * @param file_paths List of file paths
 * @param options Compression options
 * @return Future containing vector of compression results
 */
std::future<Vector<CompressionResult>> processFilesAsync(
    const Vector<String>& file_paths,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Creates a file backup (optional compression)
 * @param source_path Source file path
 * @param backup_path Backup file path
 * @param compress Whether to compress the backup
 * @param options Compression options
 * @return Operation result
 */
CompressionResult createBackup(
    std::string_view source_path, std::string_view backup_path,
    bool compress = false,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Restores a file from backup
 * @param backup_path Backup file path
 * @param restore_path Restore file path
 * @param compressed Whether the backup is compressed
 * @param options Decompression options
 * @return Operation result
 */
CompressionResult restoreFromBackup(
    std::string_view backup_path, std::string_view restore_path,
    bool compressed = false,
    const DecompressionOptions& options = DecompressionOptions{});

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESSION_BACKUP_HPP
