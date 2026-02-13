/*
 * slice_compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Slice-based file compression and merging

**************************************************/

#ifndef ATOM_IO_COMPRESSION_SLICE_COMPRESS_HPP
#define ATOM_IO_COMPRESSION_SLICE_COMPRESS_HPP

#include <string_view>

#include "types.hpp"

namespace atom::io {

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

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESSION_SLICE_COMPRESS_HPP
