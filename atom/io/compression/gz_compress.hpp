/*
 * gz_compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: GZ file compression and decompression using ZLib

**************************************************/

#ifndef ATOM_IO_COMPRESSION_GZ_COMPRESS_HPP
#define ATOM_IO_COMPRESSION_GZ_COMPRESS_HPP

#include <string_view>

#include "types.hpp"

namespace atom::io {

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

}  // namespace atom::io

#endif  // ATOM_IO_COMPRESSION_GZ_COMPRESS_HPP
