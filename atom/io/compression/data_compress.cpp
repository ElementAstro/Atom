/*
 * data_compress.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Generic data compression and decompression templates

**************************************************/

#include "data_compress.hpp"

#include <zlib.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <span>
#include <string>

#include <spdlog/spdlog.h>

#include "utils.hpp"

namespace atom::io {

// Use type aliases from high_performance.hpp within the implementation
using atom::containers::String;
template <typename T>
using Vector = atom::containers::Vector<T>;

// --- Template Implementations ---

// Generic data compression template
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             (!std::is_same_v<
                 std::remove_cvref_t<std::ranges::range_value_t<T>>,
                 wchar_t>)  // Exclude wide char ranges for now
std::pair<CompressionResult, Vector<unsigned char>> compressData(
    const T& data, const CompressionOptions& options) {
    std::pair<CompressionResult, Vector<unsigned char>> result_pair;
    auto& [compression_result, compressed_data] =
        result_pair;  // Use structured binding

    try {
        // Get data pointer and size using std::ranges::data and
        // std::ranges::size
        const auto* data_ptr = std::ranges::data(data);
        size_t data_size =
            std::ranges::size(data) *
            sizeof(std::ranges::range_value_t<T>);  // Size in bytes

        if (data_size == 0) {
            compression_result.error_message = "Empty input data";
            return result_pair;
        }

        compression_result.original_size = data_size;

        // Estimate compressed size using zlib's compressBound
        uLong compressed_bound = compressBound(data_size);
        compressed_data.resize(
            compressed_bound);  // Resize Vector<unsigned char>

        // Use advanced deflate with specified window_bits instead of simple
        // compress2
        z_stream zs{};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = static_cast<uInt>(data_size);
        zs.next_in =
            const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data_ptr));
        zs.avail_out = static_cast<uInt>(compressed_bound);
        zs.next_out = reinterpret_cast<Bytef*>(compressed_data.data());

        // Initialize deflate with window_bits from options
        int ret = deflateInit2(&zs, options.level, Z_DEFLATED,
                               options.window_bits, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK) {
            compression_result.error_message =
                detail::getZlibErrorMessage(ret);
            return result_pair;
        }

        // Use RAII for zstream cleanup
        std::unique_ptr<z_stream, decltype(&deflateEnd)> deflate_guard(
            &zs, deflateEnd);

        // Perform compression in one step
        ret = deflate(&zs, Z_FINISH);

        if (ret != Z_STREAM_END) {
            compression_result.error_message =
                String("Compression failed: ") +
                detail::getZlibErrorMessage(ret);
            return result_pair;
        }

        // Use actual bytes written
        uLongf actual_compressed_size = zs.total_out;

        if (ret != Z_OK) {
            compression_result.error_message =
                detail::getZlibErrorMessage(ret);  // Use helper
            compressed_data.clear();               // Clear data on error
            return result_pair;
        }

        // Resize buffer to actual compressed size
        compressed_data.resize(actual_compressed_size);
        compression_result.compressed_size = actual_compressed_size;
        compression_result.compression_ratio =
            detail::calculateCompressionRatio(actual_compressed_size,
                                             compression_result.original_size);

        compression_result.success = true;

        spdlog::info(
            "Successfully compressed {} bytes to {} bytes (ratio: {:.2f}%)",
            compression_result.original_size, actual_compressed_size,
            detail::getCompressionPercentage(
                compression_result.compression_ratio));

    } catch (const std::exception& e) {
        compression_result.error_message =
            String("Exception during data compression: ") + e.what();
        spdlog::error("{}", compression_result.error_message.c_str());
        compressed_data.clear();  // Ensure data is cleared on exception
    }

    return result_pair;
}

// Generic data decompression template
template <typename T>
    requires std::ranges::contiguous_range<T> &&
             (!std::is_same_v<
                 std::remove_cvref_t<std::ranges::range_value_t<T>>, wchar_t>)
std::pair<CompressionResult, Vector<unsigned char>> decompressData(
    const T& compressed_data_range, size_t expected_size,
    [[maybe_unused]] const DecompressionOptions&
        options) {  // Mark options as potentially unused

    std::pair<CompressionResult, Vector<unsigned char>> result_pair;
    auto& [compression_result, decompressed_data] = result_pair;

    try {
        const auto* compressed_data_ptr =
            std::ranges::data(compressed_data_range);
        size_t compressed_data_size = std::ranges::size(compressed_data_range) *
                                      sizeof(std::ranges::range_value_t<T>);

        if (compressed_data_size == 0) {
            compression_result.error_message = "Empty compressed data";
            return result_pair;
        }

        compression_result.compressed_size = compressed_data_size;

        // Optimized buffer size estimation
        // For small inputs, allocate a minimum buffer
        // For larger inputs with known expected size, use that
        // For larger inputs with unknown size, use a multiplier based on
        // compression type detection
        size_t buffer_size = 0;
        if (expected_size > 0) {
            // If we know the expected size, allocate exactly that
            buffer_size = expected_size;
        } else {
            // Try to detect compression type from header bytes for better
            // buffer estimation
            if (compressed_data_size >= 2) {
                const unsigned char* header =
                    reinterpret_cast<const unsigned char*>(compressed_data_ptr);

                // Check for gzip magic signature (0x1F, 0x8B)
                if (header[0] == 0x1F && header[1] == 0x8B) {
                    // Gzip typically has 2:1 to 10:1 compression ratio
                    buffer_size = compressed_data_size * 5;
                }
                // Check for zlib header (first byte bits 0-3 is 8 for deflate,
                // bits 4-7 for window size)
                else if ((header[0] & 0x0F) == 0x08) {
                    // Zlib typically has similar compression ratio to gzip
                    buffer_size = compressed_data_size * 5;
                } else {
                    // Unknown format, use conservative 4:1 ratio
                    buffer_size = compressed_data_size * 4;
                }
            } else {
                // Very small input, allocate a modest buffer
                buffer_size = 4096;
            }
        }

        // Ensure minimum buffer size
        if (buffer_size < 1024) {
            buffer_size = 1024;
        }

        decompressed_data.resize(buffer_size);

        // Use z_stream for more control, especially for potential resizing
        z_stream zs = {};
        zs.zalloc = Z_NULL;
        zs.zfree = Z_NULL;
        zs.opaque = Z_NULL;
        zs.avail_in = static_cast<uInt>(compressed_data_size);
        // Need const_cast because zlib API is not const-correct
        zs.next_in = const_cast<Bytef*>(
            reinterpret_cast<const Bytef*>(compressed_data_ptr));

        // Initialize for decompression (inflate)
        // Use window_bits from options (context7 = 7)
        // For gzip/zlib auto-detection, add 32 (15+32)
        // For raw deflate with no header, use negative value (-15)
        int windowBits = options.window_bits;

        // Auto-detect based on header bytes if possible
        if (compressed_data_size >= 2) {
            const unsigned char* header =
                reinterpret_cast<const unsigned char*>(compressed_data_ptr);
            // Check for gzip magic signature (0x1F, 0x8B)
            if (header[0] == 0x1F && header[1] == 0x8B) {
                // Need at least 15 or add 16 for gzip
                windowBits = std::max(15, abs(windowBits)) + 16;
            }
            // Check for zlib header
            else if ((header[0] & 0x0F) == 0x08) {
                // Use absolute value to ensure positive window bits for zlib
                windowBits = std::max(8, abs(windowBits));
            }
            // If not recognized, use as-is (for raw deflate)
        }

        int ret = inflateInit2(&zs, windowBits);
        if (ret != Z_OK) {
            compression_result.error_message =
                detail::getZlibErrorMessage(ret);
            return result_pair;
        }
        // Guard for inflateEnd
        std::unique_ptr<z_stream, decltype(&inflateEnd)> inflate_guard(
            &zs, inflateEnd);

        // Decompression loop to handle buffer resizing
        int inflate_ret = Z_OK;
        do {
            zs.avail_out =
                static_cast<uInt>(decompressed_data.size() - zs.total_out);
            zs.next_out = reinterpret_cast<Bytef*>(decompressed_data.data() +
                                                   zs.total_out);

            if (zs.avail_out == 0) {
                // Buffer is full, resize it with an optimized growth strategy
                size_t old_size = decompressed_data.size();

                // Smart growth strategy:
                // - For small buffers (<64KB): double the size
                // - For medium buffers (64KB-1MB): grow by 50%
                // - For large buffers (>1MB): grow by 25% or a fixed chunk
                // (1MB), whichever is larger
                size_t new_size;
                if (old_size < 65536) {
                    new_size = old_size * 2;
                } else if (old_size < 1048576) {
                    new_size = old_size + (old_size / 2);
                } else {
                    size_t increment = std::max(old_size / 4, size_t(1048576));
                    new_size = old_size + increment;
                }

                // Check for overflow
                if (new_size <= old_size) {
                    compression_result.error_message =
                        "Decompression buffer size overflow";
                    return result_pair;  // inflate_guard handles cleanup
                }

                // Allocate new buffer
                try {
                    decompressed_data.resize(new_size);
                } catch (const std::bad_alloc&) {
                    compression_result.error_message =
                        "Memory allocation failed during decompression";
                    return result_pair;
                }

                // Update stream pointers after resize
                zs.avail_out =
                    static_cast<uInt>(decompressed_data.size() - zs.total_out);
                zs.next_out = reinterpret_cast<Bytef*>(
                    decompressed_data.data() + zs.total_out);
            }

            inflate_ret = inflate(&zs, Z_NO_FLUSH);

            if (inflate_ret == Z_STREAM_ERROR) {
                compression_result.error_message = "Decompression stream error";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_NEED_DICT) {
                compression_result.error_message =
                    "Decompression needs dictionary (not supported)";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_DATA_ERROR) {
                compression_result.error_message =
                    "Decompression data error (input corrupted?)";
                return result_pair;  // inflate_guard handles cleanup
            }
            if (inflate_ret == Z_MEM_ERROR) {
                compression_result.error_message = "Decompression memory error";
                return result_pair;  // inflate_guard handles cleanup
            }

        } while (inflate_ret != Z_STREAM_END &&
                 zs.avail_in >
                     0);  // Continue if input remains and not finished

        // Check if decompression finished successfully
        if (inflate_ret != Z_STREAM_END) {
            // It might be Z_OK if the buffer was exactly the right size on the
            // last call Or Z_BUF_ERROR if output buffer was full but input
            // wasn't exhausted (should have resized) Or some other error
            // occurred. Check if all input was consumed. If not, it's likely an
            // error or truncated input.
            if (zs.avail_in != 0) {
                compression_result.error_message =
                    String(
                        "Decompression failed, stream did not end correctly "
                        "and input remains. Ret: ") +
                    String(std::to_string(inflate_ret));
                return result_pair;  // inflate_guard handles cleanup
            }
            // If input is consumed but stream end wasn't reached, it might be
            // ok if the buffer was just right, but often indicates truncated
            // data if the original size wasn't known. Let's consider it
            // successful if input is consumed and no error occurred.
            spdlog::warn(
                "Decompression finished with code {} (Z_STREAM_END is {}), but "
                "all input consumed.",
                inflate_ret, Z_STREAM_END);
        }

        // Resize to actual decompressed size
        size_t actual_decompressed_size = zs.total_out;
        decompressed_data.resize(actual_decompressed_size);

        compression_result.original_size = actual_decompressed_size;
        compression_result.compression_ratio =
            detail::calculateCompressionRatio(
                compression_result.compressed_size, actual_decompressed_size);

        compression_result.success = true;

        spdlog::info(
            "Successfully decompressed {} bytes to {} bytes (ratio: {:.2f}%)",
            compression_result.compressed_size, actual_decompressed_size,
            detail::getCompressionPercentage(
                compression_result.compression_ratio));

    } catch (const std::exception& e) {
        compression_result.error_message =
            String("Exception during data decompression: ") + e.what();
        spdlog::error("{}", compression_result.error_message.c_str());
        decompressed_data.clear();
    }

    return result_pair;
}

// Explicit template instantiations using Vector<unsigned char>
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<Vector<unsigned char>>(const Vector<unsigned char>&,
                                    const CompressionOptions&);
// Add instantiations for other types if needed, e.g., Vector<char>, String
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<Vector<char>>(const Vector<char>&, const CompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<String>(const String&, const CompressionOptions&);
// Instantiation for std::span might require C++20
#if __cplusplus >= 202002L
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, const CompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
compressData<std::span<const char>>(const std::span<const char>&,
                                    const CompressionOptions&);
#endif

template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<Vector<unsigned char>>(const Vector<unsigned char>&, size_t,
                                      const DecompressionOptions&);
// Add instantiations for other types if needed
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<Vector<char>>(const Vector<char>&, size_t,
                             const DecompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<String>(const String&, size_t, const DecompressionOptions&);
#if __cplusplus >= 202002L
// #include <span> // Already included above
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<std::span<const unsigned char>>(
    const std::span<const unsigned char>&, size_t, const DecompressionOptions&);
template std::pair<CompressionResult, Vector<unsigned char>>
decompressData<std::span<const char>>(const std::span<const char>&, size_t,
                                      const DecompressionOptions&);
#endif

}  // namespace atom::io
