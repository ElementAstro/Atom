/*
 * compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Compressor using ZLib

**************************************************/

#ifndef ATOM_IO_COMPRESS_HPP
#define ATOM_IO_COMPRESS_HPP

#include <filesystem>
#include <future>
#include <string>
#include <string_view>
#include <vector>

namespace atom::io {

// Concepts for path-like types
template <typename T>
concept PathLike = requires(T t) { std::filesystem::path(t); };

/**
 * @brief Compress a single file
 * @param file_name The name (including path) of the file to be compressed.
 * @param output_folder The folder where the compressed file will be saved.
 * @return Whether the compression is successful.
 *
 * This function compresses a single file, and the compressed file is named by
 * adding the .gz suffix to the source file name.
 *
 * @note If the file name already contains a .gz suffix, it will not be
 * compressed again.
 * @throws std::filesystem::filesystem_error if there are issues with file
 * operations
 */
auto compressFile(std::string_view file_name,
                  std::string_view output_folder) -> bool;

/**
 * @brief Decompress a single file
 * @param file_name The name (including path) of the file to be decompressed.
 * @param output_folder The folder where the decompressed file will be saved.
 * @return Whether the decompression is successful.
 *
 * This function decompresses a single compressed file, and the decompressed
 * file is named by removing the .gz suffix from the source file name.
 *
 * @note If the file name does not contain a .gz suffix, it will not be
 * decompressed.
 * @throws std::filesystem::filesystem_error if there are issues with file
 * operations
 */
auto decompressFile(std::string_view file_name,
                    std::string_view output_folder) -> bool;

/**
 * @brief Compress all files in a specified directory
 * @param folder_name The name (absolute path) of the folder to be compressed.
 * @return Whether the compression is successful.
 *
 * This function compresses all files in the specified folder, and the
 * compressed file is named by adding the .gz suffix to each source file name.
 *
 * @note The compressed files will be saved in the original directory, and files
 * in subdirectories will not be compressed.
 * @throws std::filesystem::filesystem_error if there are issues with file
 * operations
 */
auto compressFolder(const char* folder_name) -> bool;

/**
 * @brief Extract a single ZIP file
 * @param zip_file The name (including path) of the ZIP file to be extracted.
 * @param destination_folder The path where the extracted files will be saved
 * (including path).
 * @return Whether the extraction is successful.
 *
 * This function extracts a single ZIP file, and the extracted files are saved
 * in the specified path.
 *
 * @note If the specified path does not exist, the function will attempt to
 * create it.
 */
auto extractZip(std::string_view zip_file,
                std::string_view destination_folder) -> bool;

/**
 * @brief Create a ZIP file
 * @param source_folder The name (including path) of the folder to be
 * compressed.
 * @param zip_file The name (including path) of the resulting ZIP file.
 * @param compression_level Compression level (optional, default is -1, meaning
 * use default level).
 * @return Whether the creation is successful.
 *
 * This function creates a ZIP file and compresses the files in the specified
 * folder into the ZIP file.
 *
 * @note If the specified path does not exist, the function will attempt to
 * create it.
 */
auto createZip(std::string_view source_folder, std::string_view zip_file,
               int compression_level = -1) -> bool;

/**
 * @brief List files in a ZIP file
 * @param zip_file The name (including path) of the ZIP file.
 * @return A list of file names.
 *
 * This function lists the files in a ZIP file.
 *
 * @note If the specified ZIP file does not exist, the function will return an
 * empty list.
 */
auto listFilesInZip(std::string_view zip_file) -> std::vector<std::string>;

/**
 * @brief Check if a specified file exists in a ZIP file
 * @param zip_file The name (including path) of the ZIP file.
 * @param file_name The name of the file to check.
 * @return Whether the file exists.
 *
 * This function checks if a specified file exists in a ZIP file.
 *
 * @note If the specified ZIP file does not exist, the function will return
 * false.
 */
auto fileExistsInZip(std::string_view zip_file,
                     std::string_view file_name) -> bool;

/**
 * @brief Remove a specified file from a ZIP file
 * @param zip_file The name (including path) of the ZIP file.
 * @param file_name The name of the file to be removed.
 * @return Whether the removal is successful.
 *
 * This function removes a specified file from a ZIP file.
 *
 * @note If the specified ZIP file does not exist, the function will return
 * false.
 */
auto removeFileFromZip(std::string_view zip_file,
                       std::string_view file_name) -> bool;

/**
 * @brief Get the size of a file in a ZIP file
 * @param zip_file The name (including path) of the ZIP file.
 * @return The file size.
 *
 * This function gets the size of a file in a ZIP file.
 *
 * @note If the specified ZIP file does not exist, the function will return 0.
 */
auto getZipFileSize(std::string_view zip_file) -> size_t;

/**
 * @brief Compress a file slice by slice with parallel processing
 * @param inputFile The name of the file to be compressed
 * @param sliceSize The size of each slice
 * @param numThreads Number of threads to use (default: hardware concurrency)
 */
void compressFileSlice(const std::string& inputFile, size_t sliceSize,
                       size_t numThreads = std::thread::hardware_concurrency());

/**
 * @brief Decompress file slices with parallel processing
 * @param sliceFiles Vector of slice files to decompress
 * @param sliceSize Expected size of each decompressed slice
 * @param outputFile Name of the output file to reconstruct
 */
void decompressFileSlices(const std::vector<std::string>& sliceFiles,
                          size_t sliceSize, const std::string& outputFile);

/**
 * @brief Process a batch of files in parallel using coroutines
 * @param filenames Vector of filenames to process
 * @return A future that completes when all files are processed
 */
std::future<void> processFilesAsync(const std::vector<std::string>& filenames);

/**
 * @brief Create a backup of a file with optional compression
 * @param originalFile Path to the original file
 * @param backupFile Path for the backup file
 * @param compress Whether to compress the backup
 * @return Whether the backup was successful
 */
bool createBackup(const std::string& originalFile,
                  const std::string& backupFile, bool compress = false);

/**
 * @brief Restore from a backup file
 * @param backupFile Path to the backup file
 * @param originalFile Path to restore to
 * @param decompress Whether to decompress the backup
 * @return Whether the restoration was successful
 */
bool restoreBackup(const std::string& backupFile,
                   const std::string& originalFile, bool decompress = false);

/**
 * @brief Template function to compress any data that can be viewed as bytes
 * @tparam T Type of the input data container
 * @param data Input data to compress
 * @return Compressed data as vector of bytes
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::vector<unsigned char> compressData(const T& data);

/**
 * @brief Template function to decompress data
 * @tparam T Type of the input data container
 * @param compressedData Compressed input data
 * @param expectedSize Expected size of decompressed data (0 for unknown)
 * @return Decompressed data as vector of bytes
 */
template <typename T>
    requires std::ranges::contiguous_range<T>
std::vector<unsigned char> decompressData(const T& compressedData,
                                          size_t expectedSize = 0);

}  // namespace atom::io

#endif
