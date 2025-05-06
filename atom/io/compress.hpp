/*
 * compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib and MiniZip-ng

**************************************************/

#ifndef ATOM_IO_COMPRESS_HPP
#define ATOM_IO_COMPRESS_HPP

#include <future>
#include <optional>
#include <ranges>
#include <string_view>
#include <thread>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

// Use type aliases from high_performance.hpp
using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

/// @brief Forward declaration of compression options struct
struct CompressionOptions;
/// @brief Forward declaration of decompression options struct
struct DecompressionOptions;

/**
 * @brief Compression status and result struct
 */
struct CompressionResult {
    bool success{false};            ///< Whether the compression was successful
    String error_message;           ///< Error message if compression failed
    size_t original_size{0};        ///< Size of original data
    size_t compressed_size{0};      ///< Size after compression
    double compression_ratio{0.0};  ///< Compression ratio achieved
};

/**
 * @brief Basic compression options
 */
struct CompressionOptions {
    int level{-1};             ///< Compression level (-1 = default, 0-9)
    size_t chunk_size{16384};  ///< Processing chunk size
    bool use_parallel{true};   ///< Whether to use parallel processing
    size_t num_threads{
        std::thread::hardware_concurrency()};  ///< Number of parallel threads
    bool create_backup{false};                 ///< Whether to create a backup
    String password;  ///< Encryption password (optional)
};

/**
 * @brief Basic decompression options
 */
struct DecompressionOptions {
    size_t chunk_size{16384};  ///< Processing chunk size
    bool use_parallel{true};   ///< Whether to use parallel processing
    size_t num_threads{
        std::thread::hardware_concurrency()};  ///< Number of parallel threads
    bool verify_checksum{true};                ///< Whether to verify checksum
    String password;  ///< Decryption password (if needed)
};

/**
 * @brief Compresses a single file
 * @param file_path Path of the file to compress
 * @param output_folder Output folder
 * @param options Compression options
 * @return Compression result
 */
CompressionResult compressFile(
    std::string_view file_path, std::string_view output_folder,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Decompresses a single file
 * @param file_path Path of the file to decompress
 * @param output_folder Output folder
 * @param options Decompression options
 * @return Operation result
 */
CompressionResult decompressFile(
    std::string_view file_path, std::string_view output_folder,
    const DecompressionOptions& options = DecompressionOptions{});

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
 * @brief ZIP file information struct
 */
struct ZipFileInfo {
    String name;             ///< File name
    size_t size;             ///< Uncompressed size
    size_t compressed_size;  ///< Compressed size
    String datetime;         ///< Date and time information
    bool is_directory;       ///< Whether the entry is a directory
    bool is_encrypted;       ///< Whether the entry is encrypted
    uint32_t crc;            ///< CRC checksum
};

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

/**
 * @brief Compresses a large file in slices
 * @param file_path Path of the file to compress
 * @param slice_size Size of each slice in bytes
 * @param options Compression options
 * @return Operation result
 */
CompressionResult compressFileInSlices(
    std::string_view file_path, size_t slice_size,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Merges compressed slices
 * @param slice_files List of slice file paths
 * @param output_path Output file path
 * @param options Decompression options
 * @return Operation result
 */
CompressionResult mergeCompressedSlices(
    const Vector<String>& slice_files, std::string_view output_path,
    const DecompressionOptions& options = DecompressionOptions{});

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

/**
 * @brief Generic data compression template
 * @tparam T Input data type (must be a contiguous range)
 * @param data Data to compress
 * @param options Compression options
 * @return Pair containing compression result and compressed data
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, Vector<unsigned char>> compressData(
    const T& data, const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Generic data decompression template
 * @tparam T Input data type (must be a contiguous range)
 * @param compressed_data Compressed data
 * @param expected_size Expected decompressed size (optional)
 * @param options Decompression options
 * @return Pair containing decompression result and decompressed data
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::pair<CompressionResult, Vector<unsigned char>> decompressData(
    const T& compressed_data, size_t expected_size = 0,
    const DecompressionOptions& options = DecompressionOptions{});

/// @cond TEMPLATE_IMPL
// Explicit template instantiation declarations
extern template std::pair<CompressionResult, Vector<unsigned char>>
compressData<Vector<unsigned char>>(const Vector<unsigned char>&,
                                    const CompressionOptions&);

extern template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<Vector<unsigned char>>(const Vector<unsigned char>&, size_t,
                                      const DecompressionOptions&);
/// @endcond

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESS_HPP
