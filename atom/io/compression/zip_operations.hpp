/*
 * zip_operations.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: ZIP file operations using MiniZip-ng

**************************************************/

#ifndef ATOM_IO_COMPRESSION_ZIP_OPERATIONS_HPP
#define ATOM_IO_COMPRESSION_ZIP_OPERATIONS_HPP

#include <optional>
#include <string_view>

#include "types.hpp"

namespace atom::io {

/**
 * @brief Compresses an entire folder
 * @param folder_path Path of the folder to compress
 * @param output_path Output file path
 * @param options Compression options
 * @return Compression result
 */
CompressionResult compressFolder(
    std::string_view folder_path, std::string_view output_path,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Extracts a ZIP file
 * @param zip_path Path of the ZIP file
 * @param output_folder Output folder
 * @param options Decompression options
 * @return Operation result
 */
CompressionResult extractZip(
    std::string_view zip_path, std::string_view output_folder,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief Creates a ZIP file
 * @param source_path Source folder or file path
 * @param zip_path Target ZIP file path
 * @param options Compression options
 * @return Operation result
 */
CompressionResult createZip(
    std::string_view source_path, std::string_view zip_path,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Lists the contents of a ZIP file
 * @param zip_path Path of the ZIP file
 * @return List of file information
 */
Vector<ZipFileInfo> listZipContents(std::string_view zip_path);

/**
 * @brief Checks if a file exists in the ZIP archive
 * @param zip_path Path of the ZIP file
 * @param file_path Path of the file to check
 * @return True if the file exists, false otherwise
 */
bool fileExistsInZip(std::string_view zip_path, std::string_view file_path);

/**
 * @brief Removes a specified file from the ZIP archive
 * @param zip_path Path of the ZIP file
 * @param file_path Path of the file to remove
 * @return Operation result
 */
CompressionResult removeFromZip(std::string_view zip_path,
                                std::string_view file_path);

/**
 * @brief Gets the size of the ZIP file
 * @param zip_path Path of the ZIP file
 * @return File size in bytes
 */
std::optional<size_t> getZipSize(std::string_view zip_path);

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESSION_ZIP_OPERATIONS_HPP
