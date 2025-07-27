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
#include <functional>
#include <chrono>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>

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

// Progress callback type
using ProgressCallback = std::function<void(size_t bytes_processed, size_t total_bytes, double percentage)>;

// Completion callback type
using CompletionCallback = std::function<void(const struct CompressionResult& result)>;

/**
 * @brief Enhanced compression status and result struct
 */
struct CompressionResult {
    bool success{false};            ///< Whether the compression was successful
    String error_message;           ///< Error message if compression failed
    size_t original_size{0};        ///< Size of original data
    size_t compressed_size{0};      ///< Size after compression
    double compression_ratio{0.0};  ///< Compression ratio achieved

    // Enhanced statistics
    std::chrono::milliseconds processing_time{0};  ///< Time taken for operation
    size_t files_processed{0};      ///< Number of files processed
    double throughput_mbps{0.0};    ///< Processing throughput in MB/s
    String algorithm_used;          ///< Compression algorithm used
    int compression_level{-1};      ///< Actual compression level used

    // Integrity information
    uint32_t crc32_checksum{0};     ///< CRC32 checksum of original data
    bool integrity_verified{false}; ///< Whether integrity was verified

    // Memory usage statistics
    size_t peak_memory_usage{0};    ///< Peak memory usage during operation
    size_t buffer_size_used{0};     ///< Buffer size used for operation

    /**
     * @brief Calculates and updates compression ratio
     */
    void updateCompressionRatio() {
        if (original_size > 0) {
            compression_ratio = static_cast<double>(compressed_size) / original_size;
        }
    }

    /**
     * @brief Calculates and updates throughput
     */
    void updateThroughput() {
        if (processing_time.count() > 0) {
            double seconds = processing_time.count() / 1000.0;
            double mb_processed = static_cast<double>(original_size) / (1024 * 1024);
            throughput_mbps = mb_processed / seconds;
        }
    }
};

/**
 * @brief Enhanced compression options
 */
struct CompressionOptions {
    int level{-1};       ///< Compression level (-1 = default, 0-9)
    int window_bits{7};  ///< Window bits for compression context (context7)
    size_t chunk_size{16384};  ///< Processing chunk size
    bool use_parallel{true};   ///< Whether to use parallel processing
    size_t num_threads{
        std::thread::hardware_concurrency()};  ///< Number of parallel threads
    bool create_backup{false};                 ///< Whether to create a backup
    String password;  ///< Encryption password (optional)

    // Enhanced options
    bool enable_progress_reporting{false};     ///< Enable progress callbacks
    bool enable_statistics{true};              ///< Enable detailed statistics
    bool verify_integrity{true};               ///< Verify data integrity
    bool use_memory_mapping{false};            ///< Use memory mapping for large files
    size_t memory_mapping_threshold{100 * 1024 * 1024}; ///< 100MB threshold

    // Performance tuning
    bool use_dictionary{false};                ///< Use compression dictionary
    String dictionary_data;                    ///< Custom dictionary data
    bool optimize_for_speed{false};            ///< Optimize for speed over ratio
    size_t buffer_pool_size{10};               ///< Number of buffers to pool

    // Advanced options
    std::chrono::milliseconds timeout{30000};  ///< Operation timeout (30s)
    bool enable_cancellation{true};            ///< Allow operation cancellation
    String compression_profile{"balanced"};    ///< Compression profile (fast/balanced/best)

    // Callbacks
    ProgressCallback progress_callback;        ///< Progress reporting callback
    CompletionCallback completion_callback;    ///< Completion callback

    /**
     * @brief Creates a fast compression profile
     */
    static CompressionOptions createFastProfile() {
        CompressionOptions options;
        options.level = 1;
        options.chunk_size = 32768;
        options.optimize_for_speed = true;
        options.compression_profile = "fast";
        return options;
    }

    /**
     * @brief Creates a balanced compression profile
     */
    static CompressionOptions createBalancedProfile() {
        CompressionOptions options;
        options.level = 6;
        options.chunk_size = 16384;
        options.compression_profile = "balanced";
        return options;
    }

    /**
     * @brief Creates a best compression profile
     */
    static CompressionOptions createBestProfile() {
        CompressionOptions options;
        options.level = 9;
        options.chunk_size = 8192;
        options.optimize_for_speed = false;
        options.compression_profile = "best";
        return options;
    }
};

/**
 * @brief Enhanced decompression options
 */
struct DecompressionOptions {
    size_t chunk_size{16384};  ///< Processing chunk size
    bool use_parallel{true};   ///< Whether to use parallel processing
    size_t num_threads{
        std::thread::hardware_concurrency()};  ///< Number of parallel threads
    bool verify_checksum{true};                ///< Whether to verify checksum
    int window_bits{7};  ///< Window bits for decompression context (context7)
    String password;     ///< Decryption password (if needed)

    // Enhanced options
    bool enable_progress_reporting{false};     ///< Enable progress callbacks
    bool enable_statistics{true};              ///< Enable detailed statistics
    bool verify_integrity{true};               ///< Verify data integrity after decompression
    bool use_memory_mapping{false};            ///< Use memory mapping for large files
    size_t memory_mapping_threshold{100 * 1024 * 1024}; ///< 100MB threshold

    // Performance tuning
    bool optimize_for_speed{false};            ///< Optimize for speed over memory usage
    size_t buffer_pool_size{10};               ///< Number of buffers to pool
    bool preserve_timestamps{true};            ///< Preserve original file timestamps
    bool preserve_permissions{true};           ///< Preserve original file permissions

    // Advanced options
    std::chrono::milliseconds timeout{30000};  ///< Operation timeout (30s)
    bool enable_cancellation{true};            ///< Allow operation cancellation
    bool validate_archive_structure{true};     ///< Validate archive structure before extraction

    // Callbacks
    ProgressCallback progress_callback;        ///< Progress reporting callback
    CompletionCallback completion_callback;    ///< Completion callback

    /**
     * @brief Creates a fast decompression profile
     */
    static DecompressionOptions createFastProfile() {
        DecompressionOptions options;
        options.chunk_size = 32768;
        options.optimize_for_speed = true;
        options.verify_checksum = false;  // Skip for speed
        return options;
    }

    /**
     * @brief Creates a secure decompression profile
     */
    static DecompressionOptions createSecureProfile() {
        DecompressionOptions options;
        options.verify_checksum = true;
        options.verify_integrity = true;
        options.validate_archive_structure = true;
        return options;
    }
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
 * @brief Compresses a single file with progress reporting
 * @param file_path Path of the file to compress
 * @param output_folder Output folder
 * @param progress_callback Progress callback function
 * @param options Compression options
 * @return Compression result
 */
CompressionResult compressFileWithProgress(
    std::string_view file_path, std::string_view output_folder,
    ProgressCallback progress_callback,
    const CompressionOptions& options = CompressionOptions{});

/**
 * @brief Compresses a single file asynchronously
 * @param file_path Path of the file to compress
 * @param output_folder Output folder
 * @param completion_callback Completion callback function
 * @param options Compression options
 * @return Future containing compression result
 */
std::future<CompressionResult> compressFileAsync(
    std::string_view file_path, std::string_view output_folder,
    CompletionCallback completion_callback = nullptr,
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
 * @brief Decompresses a single file with progress reporting
 * @param file_path Path of the file to decompress
 * @param output_folder Output folder
 * @param progress_callback Progress callback function
 * @param options Decompression options
 * @return Operation result
 */
CompressionResult decompressFileWithProgress(
    std::string_view file_path, std::string_view output_folder,
    ProgressCallback progress_callback,
    const DecompressionOptions& options = DecompressionOptions{});

/**
 * @brief Decompresses a single file asynchronously
 * @param file_path Path of the file to decompress
 * @param output_folder Output folder
 * @param completion_callback Completion callback function
 * @param options Decompression options
 * @return Future containing operation result
 */
std::future<CompressionResult> decompressFileAsync(
    std::string_view file_path, std::string_view output_folder,
    CompletionCallback completion_callback = nullptr,
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

/**
 * @brief Compression statistics and monitoring
 */
class CompressionStats {
public:
    static CompressionStats& getInstance();

    void recordOperation(const CompressionResult& result);
    void reset();

    size_t getTotalOperations() const;
    size_t getSuccessfulOperations() const;
    size_t getFailedOperations() const;
    double getAverageCompressionRatio() const;
    double getAverageThroughput() const;

private:
    CompressionStats() = default;
    mutable std::mutex mutex_;
    size_t total_operations_{0};
    size_t successful_operations_{0};
    size_t failed_operations_{0};
    double total_compression_ratio_{0.0};
    double total_throughput_{0.0};
};

/**
 * @brief Buffer pool for efficient memory management
 */
class CompressionBufferPool {
public:
    static CompressionBufferPool& getInstance();

    Vector<unsigned char> getBuffer(size_t size);
    void returnBuffer(Vector<unsigned char>&& buffer);
    void clear();

private:
    CompressionBufferPool() = default;
    std::mutex mutex_;
    std::unordered_map<size_t, Vector<Vector<unsigned char>>> pools_;
};

/**
 * @brief Compression format detection utility
 */
enum class CompressionFormat {
    UNKNOWN,
    GZIP,
    ZLIB,
    ZIP,
    BZIP2,
    XZ
};

class CompressionFormatDetector {
public:
    static CompressionFormat detectFormat(std::string_view file_path);
    static CompressionFormat detectFormat(const Vector<unsigned char>& data);
    static String getFormatName(CompressionFormat format);
    static Vector<String> getSupportedExtensions(CompressionFormat format);

private:
    static bool isGzipFormat(const Vector<unsigned char>& header);
    static bool isZlibFormat(const Vector<unsigned char>& header);
    static bool isZipFormat(const Vector<unsigned char>& header);
};

/**
 * @brief Utility functions for compression operations
 */
namespace utils {

/**
 * @brief Estimates compression ratio for given data
 */
double estimateCompressionRatio(const Vector<unsigned char>& data,
                               const CompressionOptions& options = {});

/**
 * @brief Calculates optimal chunk size for given file size
 */
size_t getOptimalChunkSize(size_t file_size);

/**
 * @brief Validates compression options
 */
bool validateCompressionOptions(const CompressionOptions& options);

/**
 * @brief Validates decompression options
 */
bool validateDecompressionOptions(const DecompressionOptions& options);

/**
 * @brief Creates optimal compression options for given file size
 */
CompressionOptions createOptimalOptions(size_t file_size, const String& profile = "balanced");

} // namespace utils

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESS_HPP
