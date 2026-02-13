/*
 * utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-31

Description: Internal utility classes and functions for compression module

**************************************************/

#ifndef ATOM_IO_COMPRESSION_UTILS_HPP
#define ATOM_IO_COMPRESSION_UTILS_HPP

#include <zlib.h>

#include <atomic>
#include <string>

namespace atom::io::detail {

constexpr size_t DEFAULT_CHUNK_SIZE = 16384;

// Helper function to calculate compression ratio
inline double calculateCompressionRatio(size_t compressed_size,
                                        size_t original_size) {
    if (original_size > 0) {
        return static_cast<double>(compressed_size) /
               static_cast<double>(original_size);
    }
    return 0.0;
}

// Helper function to get compression ratio percentage for display
inline double getCompressionPercentage(double compression_ratio) {
    return (compression_ratio > 0.0) ? (1.0 - compression_ratio) * 100.0 : 0.0;
}

class ZStreamGuard {
    z_stream stream_;
    bool initialized_{false};
    bool is_inflate_{false};

public:
    ZStreamGuard() noexcept {
        stream_.zalloc = Z_NULL;
        stream_.zfree = Z_NULL;
        stream_.opaque = Z_NULL;
    }

    ~ZStreamGuard() {
        if (initialized_) {
            if (is_inflate_) {
                inflateEnd(&stream_);
            } else {
                deflateEnd(&stream_);
            }
        }
    }

    // Initialize for compression
    bool initDeflate(int level, int windowBits = 7) {
        int ret = deflateInit2(&stream_, level, Z_DEFLATED, windowBits, 8,
                               Z_DEFAULT_STRATEGY);
        if (ret == Z_OK) {
            initialized_ = true;
            is_inflate_ = false;
            return true;
        }
        return false;
    }

    // Initialize for decompression
    bool initInflate(int windowBits = 7) {
        int ret = inflateInit2(&stream_, windowBits);
        if (ret == Z_OK) {
            initialized_ = true;
            is_inflate_ = true;
            return true;
        }
        return false;
    }

    // End the stream explicitly if needed before destruction
    void endStream() {
        if (initialized_) {
            if (is_inflate_) {
                inflateEnd(&stream_);
            } else {
                deflateEnd(&stream_);
            }
            initialized_ = false;
        }
    }

    z_stream* get() { return &stream_; }
    const z_stream* get() const { return &stream_; }
};

// Error handling helper function
// Return std::string as it's simple and doesn't need high performance here
inline std::string getZlibErrorMessage(int error_code) {
    switch (error_code) {
        case Z_ERRNO:
            return "File operation error";
        case Z_STREAM_ERROR:
            return "Stream state inconsistent";
        case Z_DATA_ERROR:
            return "Input data corrupted";
        case Z_MEM_ERROR:
            return "Out of memory";
        case Z_BUF_ERROR:
            return "Buffer error";
        case Z_VERSION_ERROR:
            return "zlib version incompatible";
        default:
            // Use zError if available and error_code is valid zlib error
            if (error_code < 0) {
                const char* msg = zError(error_code);
                if (msg)
                    return msg;
            }
            return "Unknown error";
    }
}

// Progress info (no changes needed)
struct ProgressInfo {
    std::atomic<size_t> bytes_processed{0};
    std::atomic<size_t> total_bytes{0};
    std::atomic<bool> cancelled{false};
};

}  // namespace atom::io::detail

#endif  // ATOM_IO_COMPRESSION_UTILS_HPP
