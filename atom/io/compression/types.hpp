/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Common types for compression module

**************************************************/

#ifndef ATOM_IO_COMPRESSION_TYPES_HPP
#define ATOM_IO_COMPRESSION_TYPES_HPP

#include <concepts>
#include <filesystem>
#include <ranges>
#include <thread>

#include "atom/containers/high_performance.hpp"

namespace atom::io {

namespace fs = std::filesystem;

/**
 * @brief Concept for types that can be compressed
 */
template <typename T>
concept CompressibleData =
    std::ranges::contiguous_range<T> &&
    std::is_trivially_copyable_v<std::ranges::range_value_t<T>>;

/**
 * @brief Concept for compression level values
 */
template <typename T>
concept CompressionLevel = std::integral<T> && requires(T level) {
    requires level >= -1 && level <= 9;
};

// Use type aliases from high_performance.hpp
using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

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
    int level{-1};       ///< Compression level (-1 = default, 0-9)
    int window_bits{7};  ///< Window bits for compression context (context7)
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
    int window_bits{7};  ///< Window bits for decompression context (context7)
    String password;     ///< Decryption password (if needed)
};

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

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESSION_TYPES_HPP
