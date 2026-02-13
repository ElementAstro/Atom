/*
 * data_compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Generic data compression and decompression templates

**************************************************/

#ifndef ATOM_IO_COMPRESSION_DATA_COMPRESS_HPP
#define ATOM_IO_COMPRESSION_DATA_COMPRESS_HPP

#include <ranges>
#include <utility>

#include "types.hpp"

namespace atom::io {

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

#endif  // ATOM_IO_COMPRESSION_DATA_COMPRESS_HPP
