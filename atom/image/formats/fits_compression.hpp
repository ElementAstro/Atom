/**
 * @file fits_compression.hpp
 * @brief FITS tile compression algorithms support
 *
 * This file provides implementations of FITS standard compression
 * algorithms for tile-compressed images:
 * - Rice: Lossless compression for integer data
 * - GZIP: General purpose compression
 * - HCOMPRESS: Lossy/lossless compression for astronomical images
 * - PLIO: Pixel list compression for masks
 *
 * @copyright Copyright (C) 2023-2025
 */

#ifndef ATOM_IMAGE_FITS_COMPRESSION_HPP
#define ATOM_IMAGE_FITS_COMPRESSION_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace atom::image::fits {

/**
 * @enum TileCompressionType
 * @brief FITS tile compression algorithms
 */
enum class TileCompressionType {
    NONE,    ///< No compression
    RICE_1,  ///< Rice compression
    RICE_ONE = RICE_1,
    GZIP_1,       ///< Standard GZIP
    GZIP_2,       ///< GZIP with byte shuffling
    HCOMPRESS_1,  ///< H-compress
    PLIO_1,       ///< Pixel list I/O
    NOCOMPRESS    ///< Uncompressed tiles (for overhead)
};

/**
 * @struct CompressionParams
 * @brief Parameters for compression algorithms
 */
struct CompressionParams {
    TileCompressionType algorithm = TileCompressionType::RICE_1;

    // Tile dimensions
    std::vector<int64_t> tileSize;

    // Rice parameters
    int riceBlockSize = 32;  ///< Rice block size (RICE_BLOCKSIZE)
    int riceBytePix = 4;     ///< Bytes per pixel for Rice

    // GZIP parameters
    int gzipLevel = 6;  ///< GZIP compression level (1-9)

    // HCompress parameters
    double hcompScale = 0.0;  ///< HCompress scale factor (0 = lossless)
    int hcompSmooth = 0;      ///< HCompress smoothing

    // Quantization (for floating point)
    bool quantize = false;               ///< Enable quantization
    int quantizLevel = 16;               ///< Quantization level (bits)
    double quantizMethod = 0.0;          ///< Quantization method parameter
    std::string zquantiz = "NO_DITHER";  ///< Quantization algorithm

    // Null value handling
    int64_t blankValue = 0;  ///< Blank/null value
    bool hasBlank = false;   ///< Whether blank value is used

    // Original data info
    int bitpix = 0;                     ///< Original BITPIX
    std::vector<int64_t> originalDims;  ///< Original dimensions
};

/**
 * @struct CompressionStats
 * @brief Statistics from compression/decompression
 */
struct CompressionStats {
    size_t originalSize = 0;         ///< Original data size in bytes
    size_t compressedSize = 0;       ///< Compressed data size in bytes
    double ratio = 1.0;              ///< Compression ratio
    double compressionTime = 0.0;    ///< Time to compress (ms)
    double decompressionTime = 0.0;  ///< Time to decompress (ms)
    int tilesCompressed = 0;         ///< Number of tiles compressed
};

/**
 * @class RiceCompressor
 * @brief Rice compression algorithm implementation
 *
 * Rice compression is optimal for integer data with low entropy
 * (differences between adjacent pixels are small).
 */
class RiceCompressor {
public:
    /**
     * @brief Compress data using Rice algorithm
     * @param input Input data
     * @param blockSize Block size (default 32)
     * @return Compressed data
     */
    template <typename T>
    [[nodiscard]] std::vector<uint8_t> compress(std::span<const T> input,
                                                int blockSize = 32) const;

    /**
     * @brief Decompress Rice-compressed data
     * @param input Compressed data
     * @param outputSize Expected output size in elements
     * @param blockSize Block size used during compression
     * @return Decompressed data
     */
    template <typename T>
    [[nodiscard]] std::vector<T> decompress(std::span<const uint8_t> input,
                                            size_t outputSize,
                                            int blockSize = 32) const;

    /**
     * @brief Compress with byte stream
     * @param input Input bytes
     * @param bytesPerPixel Bytes per pixel (1, 2, 4, or 8)
     * @param blockSize Block size
     * @return Compressed data
     */
    [[nodiscard]] std::vector<uint8_t> compressBytes(
        std::span<const uint8_t> input, int bytesPerPixel,
        int blockSize = 32) const;

    /**
     * @brief Decompress to byte stream
     * @param input Compressed data
     * @param outputSize Expected output size in bytes
     * @param bytesPerPixel Bytes per pixel
     * @param blockSize Block size
     * @return Decompressed bytes
     */
    [[nodiscard]] std::vector<uint8_t> decompressBytes(
        std::span<const uint8_t> input, size_t outputSize, int bytesPerPixel,
        int blockSize = 32) const;

private:
    /**
     * @brief Encode a block of differences
     */
    void encodeBlock(const std::vector<int32_t>& diffs, int fs,
                     std::vector<uint8_t>& output) const;

    /**
     * @brief Decode a block of differences
     */
    void decodeBlock(const uint8_t*& input, const uint8_t* end, int fs,
                     int blockSize, std::vector<int32_t>& diffs) const;

    /**
     * @brief Calculate optimal FS parameter
     */
    [[nodiscard]] int calculateFS(const std::vector<int32_t>& diffs) const;
};

/**
 * @class GZIPCompressor
 * @brief GZIP compression wrapper
 *
 * Uses zlib for GZIP compression. GZIP_2 variant shuffles bytes
 * before compression for potentially better compression of
 * floating-point data.
 */
class GZIPCompressor {
public:
    /**
     * @brief Construct with compression level
     * @param level Compression level (1-9, default 6)
     */
    explicit GZIPCompressor(int level = 6);

    /**
     * @brief Compress data using GZIP
     * @param input Input data
     * @return Compressed data
     */
    [[nodiscard]] std::vector<uint8_t> compress(
        std::span<const uint8_t> input) const;

    /**
     * @brief Decompress GZIP data
     * @param input Compressed data
     * @param expectedSize Expected output size (0 = auto)
     * @return Decompressed data
     */
    [[nodiscard]] std::vector<uint8_t> decompress(
        std::span<const uint8_t> input, size_t expectedSize = 0) const;

    /**
     * @brief Compress with byte shuffling (GZIP_2)
     * @param input Input data
     * @param bytesPerPixel Bytes per pixel
     * @return Compressed data
     */
    [[nodiscard]] std::vector<uint8_t> compressShuffled(
        std::span<const uint8_t> input, int bytesPerPixel) const;

    /**
     * @brief Decompress with byte unshuffling (GZIP_2)
     * @param input Compressed data
     * @param bytesPerPixel Bytes per pixel
     * @param expectedSize Expected output size
     * @return Decompressed data
     */
    [[nodiscard]] std::vector<uint8_t> decompressShuffled(
        std::span<const uint8_t> input, int bytesPerPixel,
        size_t expectedSize) const;

    /**
     * @brief Set compression level
     * @param level Compression level (1-9)
     */
    void setLevel(int level) noexcept { level_ = level; }

    /**
     * @brief Get compression level
     * @return Current level
     */
    [[nodiscard]] int getLevel() const noexcept { return level_; }

private:
    int level_;

    /**
     * @brief Shuffle bytes for better compression
     */
    [[nodiscard]] std::vector<uint8_t> shuffleBytes(
        std::span<const uint8_t> input, int bytesPerPixel) const;

    /**
     * @brief Unshuffle bytes after decompression
     */
    [[nodiscard]] std::vector<uint8_t> unshuffleBytes(
        std::span<const uint8_t> input, int bytesPerPixel) const;
};

/**
 * @class HCompressor
 * @brief H-compress algorithm implementation
 *
 * H-compress is specifically designed for astronomical images.
 * It can be lossless (scale=0) or lossy (scale>0).
 */
class HCompressor {
public:
    /**
     * @brief Construct with scale factor
     * @param scale Scale factor (0 = lossless)
     * @param smooth Smoothing parameter
     */
    explicit HCompressor(double scale = 0.0, int smooth = 0);

    /**
     * @brief Compress 2D image
     * @param input Input image data
     * @param width Image width
     * @param height Image height
     * @return Compressed data
     */
    template <typename T>
    [[nodiscard]] std::vector<uint8_t> compress(std::span<const T> input,
                                                int width, int height) const;

    /**
     * @brief Decompress to 2D image
     * @param input Compressed data
     * @param width Image width
     * @param height Image height
     * @return Decompressed image data
     */
    template <typename T>
    [[nodiscard]] std::vector<T> decompress(std::span<const uint8_t> input,
                                            int width, int height) const;

    /**
     * @brief Set scale factor
     * @param scale Scale factor (0 = lossless)
     */
    void setScale(double scale) noexcept { scale_ = scale; }

    /**
     * @brief Set smoothing parameter
     * @param smooth Smoothing value
     */
    void setSmooth(int smooth) noexcept { smooth_ = smooth; }

    /**
     * @brief Check if lossless mode
     * @return True if scale is 0
     */
    [[nodiscard]] bool isLossless() const noexcept { return scale_ == 0.0; }

private:
    double scale_;
    int smooth_;

    /**
     * @brief Apply H-transform
     */
    void htrans(std::vector<int64_t>& data, int nx, int ny) const;

    /**
     * @brief Inverse H-transform
     */
    void htransInverse(std::vector<int64_t>& data, int nx, int ny) const;

    /**
     * @brief Digitize the transformed data
     */
    void digitize(std::vector<int64_t>& data, double scale) const;

    /**
     * @brief Undigitize the data
     */
    void undigitize(std::vector<int64_t>& data, double scale) const;
};

/**
 * @class PLIOCompressor
 * @brief Pixel List I/O compression for masks
 *
 * PLIO is optimal for binary masks or data with large contiguous
 * regions of the same value.
 */
class PLIOCompressor {
public:
    /**
     * @brief Compress mask data using PLIO
     * @param input Input mask data
     * @return Compressed data
     */
    template <typename T>
    [[nodiscard]] std::vector<int16_t> compress(std::span<const T> input) const;

    /**
     * @brief Decompress PLIO data
     * @param input Compressed data
     * @param outputSize Expected output size
     * @return Decompressed mask data
     */
    template <typename T>
    [[nodiscard]] std::vector<T> decompress(std::span<const int16_t> input,
                                            size_t outputSize) const;
};

/**
 * @class TileCompressor
 * @brief High-level tile compression manager
 *
 * Manages compression and decompression of tiled FITS images,
 * selecting appropriate algorithms based on parameters.
 */
class TileCompressor {
public:
    /**
     * @brief Default constructor
     */
    TileCompressor() = default;

    /**
     * @brief Construct with parameters
     * @param params Compression parameters
     */
    explicit TileCompressor(const CompressionParams& params);

    /**
     * @brief Compress a single tile
     * @param tileData Tile data
     * @return Compressed tile data
     */
    [[nodiscard]] std::vector<uint8_t> compressTile(
        std::span<const uint8_t> tileData) const;

    /**
     * @brief Decompress a single tile
     * @param compressedData Compressed tile data
     * @param expectedSize Expected decompressed size
     * @return Decompressed tile data
     */
    [[nodiscard]] std::vector<uint8_t> decompressTile(
        std::span<const uint8_t> compressedData, size_t expectedSize) const;

    /**
     * @brief Compress entire image as tiles
     * @param imageData Full image data
     * @param width Image width
     * @param height Image height
     * @param bytesPerPixel Bytes per pixel
     * @param progressCallback Progress callback
     * @return Vector of compressed tiles
     */
    [[nodiscard]] std::vector<std::vector<uint8_t>> compressImage(
        std::span<const uint8_t> imageData, int width, int height,
        int bytesPerPixel,
        std::function<void(float)> progressCallback = nullptr) const;

    /**
     * @brief Decompress tiled image
     * @param tiles Compressed tiles
     * @param width Image width
     * @param height Image height
     * @param bytesPerPixel Bytes per pixel
     * @param progressCallback Progress callback
     * @return Full decompressed image
     */
    [[nodiscard]] std::vector<uint8_t> decompressImage(
        const std::vector<std::vector<uint8_t>>& tiles, int width, int height,
        int bytesPerPixel,
        std::function<void(float)> progressCallback = nullptr) const;

    /**
     * @brief Get compression statistics
     * @return Statistics from last operation
     */
    [[nodiscard]] const CompressionStats& getStats() const noexcept {
        return stats_;
    }

    /**
     * @brief Set compression parameters
     * @param params Parameters
     */
    void setParams(const CompressionParams& params);

    /**
     * @brief Get current parameters
     * @return Current parameters
     */
    [[nodiscard]] const CompressionParams& getParams() const noexcept {
        return params_;
    }

    /**
     * @brief Get default tile size for image dimensions
     * @param width Image width
     * @param height Image height
     * @return Recommended tile size
     */
    [[nodiscard]] static std::pair<int, int> getDefaultTileSize(int width,
                                                                int height);

private:
    CompressionParams params_;
    mutable CompressionStats stats_;

    RiceCompressor rice_;
    GZIPCompressor gzip_;
    HCompressor hcompress_;
    PLIOCompressor plio_;

    /**
     * @brief Get bytes per pixel from BITPIX
     */
    [[nodiscard]] int getBytesPerPixel() const noexcept;

    /**
     * @brief Calculate tile dimensions
     */
    [[nodiscard]] std::pair<int, int> getTileDimensions() const;
};

/**
 * @brief Get compression type from string
 * @param name Algorithm name (e.g., "RICE_1")
 * @return Compression type
 */
[[nodiscard]] TileCompressionType compressionFromString(
    const std::string& name);

/**
 * @brief Convert compression type to string
 * @param type Compression type
 * @return Algorithm name
 */
[[nodiscard]] std::string compressionToString(TileCompressionType type);

/**
 * @brief Check if compression type is lossless
 * @param type Compression type
 * @param scale Scale factor (for HCOMPRESS)
 * @return True if lossless
 */
[[nodiscard]] bool isLossless(TileCompressionType type, double scale = 0.0);

}  // namespace atom::image::fits

#endif  // ATOM_IMAGE_FITS_COMPRESSION_HPP
