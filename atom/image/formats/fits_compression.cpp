/**
 * @file fits_compression.cpp
 * @brief Implementation of FITS tile compression algorithms
 *
 * @copyright Copyright (C) 2023-2025
 */

#include "fits_compression.hpp"

#include <zlib.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>

namespace atom::image::fits {

namespace {

// Rice compression constants
constexpr int RICE_MAX_FS = 32;
constexpr int RICE_DEFAULT_BLOCK = 32;

}  // anonymous namespace

// RiceCompressor implementation

template <typename T>
std::vector<uint8_t> RiceCompressor::compress(std::span<const T> input,
                                              int blockSize) const {
    if (input.empty()) {
        return {};
    }

    // Calculate differences
    std::vector<int32_t> diffs(input.size());
    diffs[0] = static_cast<int32_t>(input[0]);
    for (size_t i = 1; i < input.size(); ++i) {
        diffs[i] =
            static_cast<int32_t>(input[i]) - static_cast<int32_t>(input[i - 1]);
    }

    std::vector<uint8_t> output;
    output.reserve(input.size() * sizeof(T));

    // Store first value (full precision)
    int32_t first = diffs[0];
    output.push_back(static_cast<uint8_t>((first >> 24) & 0xFF));
    output.push_back(static_cast<uint8_t>((first >> 16) & 0xFF));
    output.push_back(static_cast<uint8_t>((first >> 8) & 0xFF));
    output.push_back(static_cast<uint8_t>(first & 0xFF));

    // Compress blocks
    size_t pos = 1;
    while (pos < diffs.size()) {
        size_t blockEnd = std::min(pos + blockSize, diffs.size());
        std::vector<int32_t> block(diffs.begin() + pos,
                                   diffs.begin() + blockEnd);

        int fs = calculateFS(block);
        encodeBlock(block, fs, output);

        pos = blockEnd;
    }

    return output;
}

template <typename T>
std::vector<T> RiceCompressor::decompress(std::span<const uint8_t> input,
                                          size_t outputSize,
                                          int blockSize) const {
    if (input.empty() || outputSize == 0) {
        return {};
    }

    std::vector<T> output(outputSize);
    std::vector<int32_t> diffs;
    diffs.reserve(outputSize);

    // Read first value
    if (input.size() < 4) {
        throw std::runtime_error("Invalid Rice compressed data");
    }

    int32_t first = (static_cast<int32_t>(input[0]) << 24) |
                    (static_cast<int32_t>(input[1]) << 16) |
                    (static_cast<int32_t>(input[2]) << 8) |
                    static_cast<int32_t>(input[3]);
    diffs.push_back(first);

    const uint8_t* ptr = input.data() + 4;
    const uint8_t* end = input.data() + input.size();

    // Decode blocks
    while (diffs.size() < outputSize && ptr < end) {
        int fs = *ptr & 0x1F;
        ++ptr;

        decodeBlock(ptr, end, fs, blockSize, diffs);
    }

    // Reconstruct from differences
    output[0] = static_cast<T>(diffs[0]);
    for (size_t i = 1; i < outputSize && i < diffs.size(); ++i) {
        output[i] =
            static_cast<T>(static_cast<int32_t>(output[i - 1]) + diffs[i]);
    }

    return output;
}

std::vector<uint8_t> RiceCompressor::compressBytes(
    std::span<const uint8_t> input, int bytesPerPixel, int blockSize) const {
    switch (bytesPerPixel) {
        case 1:
            return compress<uint8_t>(
                std::span<const uint8_t>(input.data(), input.size()),
                blockSize);
        case 2: {
            std::vector<int16_t> data(input.size() / 2);
            std::memcpy(data.data(), input.data(), input.size());
            return compress<int16_t>(data, blockSize);
        }
        case 4: {
            std::vector<int32_t> data(input.size() / 4);
            std::memcpy(data.data(), input.data(), input.size());
            return compress<int32_t>(data, blockSize);
        }
        default:
            throw std::runtime_error("Unsupported bytes per pixel for Rice");
    }
}

std::vector<uint8_t> RiceCompressor::decompressBytes(
    std::span<const uint8_t> input, size_t outputSize, int bytesPerPixel,
    int blockSize) const {
    switch (bytesPerPixel) {
        case 1: {
            auto data = decompress<uint8_t>(input, outputSize, blockSize);
            return data;
        }
        case 2: {
            auto data = decompress<int16_t>(input, outputSize / 2, blockSize);
            std::vector<uint8_t> result(outputSize);
            std::memcpy(result.data(), data.data(), outputSize);
            return result;
        }
        case 4: {
            auto data = decompress<int32_t>(input, outputSize / 4, blockSize);
            std::vector<uint8_t> result(outputSize);
            std::memcpy(result.data(), data.data(), outputSize);
            return result;
        }
        default:
            throw std::runtime_error("Unsupported bytes per pixel for Rice");
    }
}

void RiceCompressor::encodeBlock(const std::vector<int32_t>& diffs, int fs,
                                 std::vector<uint8_t>& output) const {
    // Store FS in first byte
    output.push_back(static_cast<uint8_t>(fs & 0x1F));

    if (fs == 0) {
        // All zeros - just store length
        return;
    }

    // Rice encode each difference
    uint32_t bitBuffer = 0;
    int bitCount = 0;

    for (int32_t diff : diffs) {
        // Map signed to unsigned (zigzag encoding)
        uint32_t udiff = (diff >= 0) ? (2 * diff) : (-2 * diff - 1);

        // Quotient and remainder
        uint32_t q = udiff >> fs;
        uint32_t r = udiff & ((1u << fs) - 1);

        // Output q 1-bits followed by a 0-bit
        while (q > 0) {
            bitBuffer = (bitBuffer << 1) | 1;
            ++bitCount;
            if (bitCount >= 8) {
                output.push_back(
                    static_cast<uint8_t>(bitBuffer >> (bitCount - 8)));
                bitCount -= 8;
                bitBuffer &= (1u << bitCount) - 1;
            }
            --q;
        }

        // Zero bit
        bitBuffer <<= 1;
        ++bitCount;
        if (bitCount >= 8) {
            output.push_back(static_cast<uint8_t>(bitBuffer >> (bitCount - 8)));
            bitCount -= 8;
            bitBuffer &= (1u << bitCount) - 1;
        }

        // Remainder bits
        bitBuffer = (bitBuffer << fs) | r;
        bitCount += fs;
        while (bitCount >= 8) {
            output.push_back(static_cast<uint8_t>(bitBuffer >> (bitCount - 8)));
            bitCount -= 8;
            bitBuffer &= (1u << bitCount) - 1;
        }
    }

    // Flush remaining bits
    if (bitCount > 0) {
        output.push_back(static_cast<uint8_t>(bitBuffer << (8 - bitCount)));
    }
}

void RiceCompressor::decodeBlock(const uint8_t*& input, const uint8_t* end,
                                 int fs, int blockSize,
                                 std::vector<int32_t>& diffs) const {
    if (fs == 0) {
        // All zeros
        for (int i = 0; i < blockSize && diffs.size() < diffs.capacity(); ++i) {
            diffs.push_back(0);
        }
        return;
    }

    uint32_t bitBuffer = 0;
    int bitCount = 0;
    int decoded = 0;

    while (decoded < blockSize && input < end) {
        // Read more bits if needed
        while (bitCount < 32 && input < end) {
            bitBuffer = (bitBuffer << 8) | *input++;
            bitCount += 8;
        }

        // Count quotient (1-bits until 0)
        uint32_t q = 0;
        while (bitCount > 0) {
            if ((bitBuffer >> (bitCount - 1)) & 1) {
                ++q;
                --bitCount;
            } else {
                --bitCount;  // Skip the 0
                break;
            }
        }

        // Read remainder
        if (bitCount < fs) {
            // Need more bits
            while (bitCount < fs && input < end) {
                bitBuffer = (bitBuffer << 8) | *input++;
                bitCount += 8;
            }
        }

        uint32_t r = 0;
        if (fs > 0 && bitCount >= fs) {
            r = (bitBuffer >> (bitCount - fs)) & ((1u << fs) - 1);
            bitCount -= fs;
        }

        // Reconstruct value
        uint32_t udiff = (q << fs) | r;

        // Reverse zigzag
        int32_t diff = (udiff & 1) ? -static_cast<int32_t>((udiff + 1) / 2)
                                   : static_cast<int32_t>(udiff / 2);
        diffs.push_back(diff);
        ++decoded;
    }
}

int RiceCompressor::calculateFS(const std::vector<int32_t>& diffs) const {
    if (diffs.empty()) {
        return 0;
    }

    // Calculate mean absolute difference
    double sum = 0.0;
    for (int32_t diff : diffs) {
        sum += std::abs(diff);
    }
    double mean = sum / diffs.size();

    if (mean < 1.0) {
        return 0;
    }

    // Optimal FS is approximately log2(mean)
    int fs = static_cast<int>(std::ceil(std::log2(mean + 1)));
    return std::clamp(fs, 1, RICE_MAX_FS);
}

// Explicit template instantiations for Rice
template std::vector<uint8_t> RiceCompressor::compress<uint8_t>(
    std::span<const uint8_t>, int) const;
template std::vector<uint8_t> RiceCompressor::compress<int16_t>(
    std::span<const int16_t>, int) const;
template std::vector<uint8_t> RiceCompressor::compress<int32_t>(
    std::span<const int32_t>, int) const;

template std::vector<uint8_t> RiceCompressor::decompress<uint8_t>(
    std::span<const uint8_t>, size_t, int) const;
template std::vector<int16_t> RiceCompressor::decompress<int16_t>(
    std::span<const uint8_t>, size_t, int) const;
template std::vector<int32_t> RiceCompressor::decompress<int32_t>(
    std::span<const uint8_t>, size_t, int) const;

// GZIPCompressor implementation

GZIPCompressor::GZIPCompressor(int level) : level_(std::clamp(level, 1, 9)) {}

std::vector<uint8_t> GZIPCompressor::compress(
    std::span<const uint8_t> input) const {
    if (input.empty()) {
        return {};
    }

    // Estimate output size
    uLongf destLen = compressBound(input.size());
    std::vector<uint8_t> output(destLen);

    int ret =
        compress2(output.data(), &destLen, input.data(), input.size(), level_);

    if (ret != Z_OK) {
        throw std::runtime_error("GZIP compression failed");
    }

    output.resize(destLen);
    return output;
}

std::vector<uint8_t> GZIPCompressor::decompress(std::span<const uint8_t> input,
                                                size_t expectedSize) const {
    if (input.empty()) {
        return {};
    }

    // If expected size not provided, estimate
    size_t outSize = expectedSize > 0 ? expectedSize : input.size() * 10;
    std::vector<uint8_t> output(outSize);

    uLongf destLen = output.size();
    int ret = uncompress(output.data(), &destLen, input.data(), input.size());

    // Retry with larger buffer if needed
    while (ret == Z_BUF_ERROR && outSize < 1024 * 1024 * 1024) {
        outSize *= 2;
        output.resize(outSize);
        destLen = output.size();
        ret = uncompress(output.data(), &destLen, input.data(), input.size());
    }

    if (ret != Z_OK) {
        throw std::runtime_error("GZIP decompression failed");
    }

    output.resize(destLen);
    return output;
}

std::vector<uint8_t> GZIPCompressor::compressShuffled(
    std::span<const uint8_t> input, int bytesPerPixel) const {
    auto shuffled = shuffleBytes(input, bytesPerPixel);
    return compress(shuffled);
}

std::vector<uint8_t> GZIPCompressor::decompressShuffled(
    std::span<const uint8_t> input, int bytesPerPixel,
    size_t expectedSize) const {
    auto decompressed = decompress(input, expectedSize);
    return unshuffleBytes(decompressed, bytesPerPixel);
}

std::vector<uint8_t> GZIPCompressor::shuffleBytes(
    std::span<const uint8_t> input, int bytesPerPixel) const {
    if (bytesPerPixel <= 1 || input.empty()) {
        return std::vector<uint8_t>(input.begin(), input.end());
    }

    size_t numPixels = input.size() / bytesPerPixel;
    std::vector<uint8_t> output(input.size());

    // Separate bytes by position within pixel
    for (int b = 0; b < bytesPerPixel; ++b) {
        for (size_t p = 0; p < numPixels; ++p) {
            output[b * numPixels + p] = input[p * bytesPerPixel + b];
        }
    }

    return output;
}

std::vector<uint8_t> GZIPCompressor::unshuffleBytes(
    std::span<const uint8_t> input, int bytesPerPixel) const {
    if (bytesPerPixel <= 1 || input.empty()) {
        return std::vector<uint8_t>(input.begin(), input.end());
    }

    size_t numPixels = input.size() / bytesPerPixel;
    std::vector<uint8_t> output(input.size());

    // Recombine bytes
    for (int b = 0; b < bytesPerPixel; ++b) {
        for (size_t p = 0; p < numPixels; ++p) {
            output[p * bytesPerPixel + b] = input[b * numPixels + p];
        }
    }

    return output;
}

// HCompressor implementation

HCompressor::HCompressor(double scale, int smooth)
    : scale_(scale), smooth_(smooth) {}

template <typename T>
std::vector<uint8_t> HCompressor::compress(std::span<const T> input, int width,
                                           int height) const {
    if (input.empty() || width <= 0 || height <= 0) {
        return {};
    }

    // Convert to int64 for processing
    std::vector<int64_t> data(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        data[i] = static_cast<int64_t>(input[i]);
    }

    // Apply H-transform
    htrans(data, width, height);

    // Digitize if lossy
    if (scale_ > 0.0) {
        digitize(data, scale_);
    }

    // Encode using Rice or simple packing
    std::vector<uint8_t> output;
    output.reserve(data.size() * 4);

    // Simple encoding: store as 4-byte integers
    for (int64_t val : data) {
        int32_t v = static_cast<int32_t>(
            std::clamp(val, static_cast<int64_t>(INT32_MIN),
                       static_cast<int64_t>(INT32_MAX)));
        output.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        output.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        output.push_back(static_cast<uint8_t>(v & 0xFF));
    }

    return output;
}

template <typename T>
std::vector<T> HCompressor::decompress(std::span<const uint8_t> input,
                                       int width, int height) const {
    size_t numPixels = static_cast<size_t>(width) * height;
    if (input.size() < numPixels * 4) {
        throw std::runtime_error("Invalid HCompress data size");
    }

    // Decode
    std::vector<int64_t> data(numPixels);
    for (size_t i = 0; i < numPixels; ++i) {
        int32_t v = (static_cast<int32_t>(input[i * 4]) << 24) |
                    (static_cast<int32_t>(input[i * 4 + 1]) << 16) |
                    (static_cast<int32_t>(input[i * 4 + 2]) << 8) |
                    static_cast<int32_t>(input[i * 4 + 3]);
        data[i] = v;
    }

    // Undigitize if lossy
    if (scale_ > 0.0) {
        undigitize(data, scale_);
    }

    // Inverse H-transform
    htransInverse(data, width, height);

    // Convert to output type
    std::vector<T> output(numPixels);
    for (size_t i = 0; i < numPixels; ++i) {
        output[i] = static_cast<T>(data[i]);
    }

    return output;
}

void HCompressor::htrans(std::vector<int64_t>& data, int nx, int ny) const {
    // Simplified H-transform (Haar-like wavelet)
    std::vector<int64_t> temp(data.size());

    // Transform rows
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx / 2; ++x) {
            int64_t a = data[y * nx + 2 * x];
            int64_t b = data[y * nx + 2 * x + 1];
            temp[y * nx + x] = (a + b) / 2;     // Low frequency
            temp[y * nx + nx / 2 + x] = a - b;  // High frequency
        }
    }

    // Transform columns
    for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny / 2; ++y) {
            int64_t a = temp[2 * y * nx + x];
            int64_t b = temp[(2 * y + 1) * nx + x];
            data[y * nx + x] = (a + b) / 2;
            data[(ny / 2 + y) * nx + x] = a - b;
        }
    }
}

void HCompressor::htransInverse(std::vector<int64_t>& data, int nx,
                                int ny) const {
    std::vector<int64_t> temp(data.size());

    // Inverse column transform
    for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny / 2; ++y) {
            int64_t low = data[y * nx + x];
            int64_t high = data[(ny / 2 + y) * nx + x];
            temp[2 * y * nx + x] = low + (high + 1) / 2;
            temp[(2 * y + 1) * nx + x] = low - high / 2;
        }
    }

    // Inverse row transform
    for (int y = 0; y < ny; ++y) {
        for (int x = 0; x < nx / 2; ++x) {
            int64_t low = temp[y * nx + x];
            int64_t high = temp[y * nx + nx / 2 + x];
            data[y * nx + 2 * x] = low + (high + 1) / 2;
            data[y * nx + 2 * x + 1] = low - high / 2;
        }
    }
}

void HCompressor::digitize(std::vector<int64_t>& data, double scale) const {
    for (auto& val : data) {
        val = static_cast<int64_t>(std::round(val / scale));
    }
}

void HCompressor::undigitize(std::vector<int64_t>& data, double scale) const {
    for (auto& val : data) {
        val = static_cast<int64_t>(std::round(val * scale));
    }
}

// Explicit template instantiations for HCompressor
template std::vector<uint8_t> HCompressor::compress<int16_t>(
    std::span<const int16_t>, int, int) const;
template std::vector<uint8_t> HCompressor::compress<int32_t>(
    std::span<const int32_t>, int, int) const;
template std::vector<uint8_t> HCompressor::compress<float>(
    std::span<const float>, int, int) const;

template std::vector<int16_t> HCompressor::decompress<int16_t>(
    std::span<const uint8_t>, int, int) const;
template std::vector<int32_t> HCompressor::decompress<int32_t>(
    std::span<const uint8_t>, int, int) const;
template std::vector<float> HCompressor::decompress<float>(
    std::span<const uint8_t>, int, int) const;

// PLIOCompressor implementation

template <typename T>
std::vector<int16_t> PLIOCompressor::compress(std::span<const T> input) const {
    if (input.empty()) {
        return {};
    }

    std::vector<int16_t> output;
    output.reserve(input.size() / 4);  // Estimate

    size_t i = 0;
    while (i < input.size()) {
        T value = input[i];
        int16_t count = 1;

        // Count consecutive identical values
        while (i + count < input.size() && input[i + count] == value &&
               count < 32767) {
            ++count;
        }

        // Store (count, value) pair
        output.push_back(count);
        output.push_back(static_cast<int16_t>(value));

        i += count;
    }

    return output;
}

template <typename T>
std::vector<T> PLIOCompressor::decompress(std::span<const int16_t> input,
                                          size_t outputSize) const {
    std::vector<T> output;
    output.reserve(outputSize);

    for (size_t i = 0; i + 1 < input.size(); i += 2) {
        int16_t count = input[i];
        T value = static_cast<T>(input[i + 1]);

        for (int16_t j = 0; j < count && output.size() < outputSize; ++j) {
            output.push_back(value);
        }
    }

    return output;
}

// Explicit template instantiations for PLIO
template std::vector<int16_t> PLIOCompressor::compress<uint8_t>(
    std::span<const uint8_t>) const;
template std::vector<int16_t> PLIOCompressor::compress<int16_t>(
    std::span<const int16_t>) const;

template std::vector<uint8_t> PLIOCompressor::decompress<uint8_t>(
    std::span<const int16_t>, size_t) const;
template std::vector<int16_t> PLIOCompressor::decompress<int16_t>(
    std::span<const int16_t>, size_t) const;

// TileCompressor implementation

TileCompressor::TileCompressor(const CompressionParams& params)
    : params_(params),
      gzip_(params.gzipLevel),
      hcompress_(params.hcompScale, params.hcompSmooth) {}

std::vector<uint8_t> TileCompressor::compressTile(
    std::span<const uint8_t> tileData) const {
    switch (params_.algorithm) {
        case TileCompressionType::RICE_1:
            // Note: RICE_ONE is an alias for RICE_1
            return rice_.compressBytes(tileData, getBytesPerPixel(),
                                       params_.riceBlockSize);

        case TileCompressionType::GZIP_1:
            return gzip_.compress(tileData);

        case TileCompressionType::GZIP_2:
            return gzip_.compressShuffled(tileData, getBytesPerPixel());

        case TileCompressionType::NOCOMPRESS:
        case TileCompressionType::NONE:
        default:
            return std::vector<uint8_t>(tileData.begin(), tileData.end());
    }
}

std::vector<uint8_t> TileCompressor::decompressTile(
    std::span<const uint8_t> compressedData, size_t expectedSize) const {
    switch (params_.algorithm) {
        case TileCompressionType::RICE_1:
            // Note: RICE_ONE is an alias for RICE_1, same case handles both
            return rice_.decompressBytes(compressedData, expectedSize,
                                         getBytesPerPixel(),
                                         params_.riceBlockSize);

        case TileCompressionType::GZIP_1:
            return gzip_.decompress(compressedData, expectedSize);

        case TileCompressionType::GZIP_2:
            return gzip_.decompressShuffled(compressedData, getBytesPerPixel(),
                                            expectedSize);

        case TileCompressionType::NOCOMPRESS:
        case TileCompressionType::NONE:
        default:
            return std::vector<uint8_t>(compressedData.begin(),
                                        compressedData.end());
    }
}

std::vector<std::vector<uint8_t>> TileCompressor::compressImage(
    std::span<const uint8_t> imageData, int width, int height,
    int bytesPerPixel, std::function<void(float)> progressCallback) const {
    auto start = std::chrono::high_resolution_clock::now();

    auto [tileWidth, tileHeight] = getTileDimensions();
    int tilesX = (width + tileWidth - 1) / tileWidth;
    int tilesY = (height + tileHeight - 1) / tileHeight;
    int totalTiles = tilesX * tilesY;

    std::vector<std::vector<uint8_t>> tiles;
    tiles.reserve(totalTiles);

    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            // Extract tile
            int startX = tx * tileWidth;
            int startY = ty * tileHeight;
            int endX = std::min(startX + tileWidth, width);
            int endY = std::min(startY + tileHeight, height);

            std::vector<uint8_t> tileData;
            tileData.reserve((endX - startX) * (endY - startY) * bytesPerPixel);

            for (int y = startY; y < endY; ++y) {
                size_t rowOffset = (y * width + startX) * bytesPerPixel;
                size_t rowSize = (endX - startX) * bytesPerPixel;
                tileData.insert(tileData.end(), imageData.begin() + rowOffset,
                                imageData.begin() + rowOffset + rowSize);
            }

            // Compress tile
            tiles.push_back(compressTile(tileData));

            if (progressCallback) {
                float progress = static_cast<float>(tiles.size()) / totalTiles;
                progressCallback(progress);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats_.compressionTime =
        std::chrono::duration<double, std::milli>(end - start).count();
    stats_.tilesCompressed = totalTiles;
    stats_.originalSize = imageData.size();

    size_t compressedSize = 0;
    for (const auto& tile : tiles) {
        compressedSize += tile.size();
    }
    stats_.compressedSize = compressedSize;
    stats_.ratio = static_cast<double>(stats_.originalSize) / compressedSize;

    return tiles;
}

std::vector<uint8_t> TileCompressor::decompressImage(
    const std::vector<std::vector<uint8_t>>& tiles, int width, int height,
    int bytesPerPixel, std::function<void(float)> progressCallback) const {
    auto start = std::chrono::high_resolution_clock::now();

    auto [tileWidth, tileHeight] = getTileDimensions();
    int tilesX = (width + tileWidth - 1) / tileWidth;
    int tilesY = (height + tileHeight - 1) / tileHeight;

    std::vector<uint8_t> imageData(width * height * bytesPerPixel, 0);

    int tileIndex = 0;
    for (int ty = 0; ty < tilesY; ++ty) {
        for (int tx = 0; tx < tilesX; ++tx) {
            int startX = tx * tileWidth;
            int startY = ty * tileHeight;
            int endX = std::min(startX + tileWidth, width);
            int endY = std::min(startY + tileHeight, height);

            size_t expectedTileSize =
                (endX - startX) * (endY - startY) * bytesPerPixel;

            auto tileData = decompressTile(tiles[tileIndex], expectedTileSize);

            // Copy tile to image
            size_t tileOffset = 0;
            for (int y = startY; y < endY; ++y) {
                size_t rowOffset = (y * width + startX) * bytesPerPixel;
                size_t rowSize = (endX - startX) * bytesPerPixel;
                std::memcpy(imageData.data() + rowOffset,
                            tileData.data() + tileOffset, rowSize);
                tileOffset += rowSize;
            }

            ++tileIndex;

            if (progressCallback) {
                float progress = static_cast<float>(tileIndex) / tiles.size();
                progressCallback(progress);
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    stats_.decompressionTime =
        std::chrono::duration<double, std::milli>(end - start).count();

    return imageData;
}

void TileCompressor::setParams(const CompressionParams& params) {
    params_ = params;
    gzip_.setLevel(params.gzipLevel);
    hcompress_.setScale(params.hcompScale);
    hcompress_.setSmooth(params.hcompSmooth);
}

std::pair<int, int> TileCompressor::getDefaultTileSize(int width, int height) {
    // Default tile size: rows of the full width, or square tiles
    if (width <= 100 && height <= 100) {
        return {width, height};
    }

    // For larger images, use row tiles
    return {width, 1};
}

int TileCompressor::getBytesPerPixel() const noexcept {
    return std::abs(params_.bitpix) / 8;
}

std::pair<int, int> TileCompressor::getTileDimensions() const {
    if (params_.tileSize.size() >= 2) {
        return {static_cast<int>(params_.tileSize[0]),
                static_cast<int>(params_.tileSize[1])};
    }
    if (params_.tileSize.size() == 1) {
        return {static_cast<int>(params_.tileSize[0]), 1};
    }
    if (!params_.originalDims.empty()) {
        return getDefaultTileSize(
            static_cast<int>(params_.originalDims[0]),
            params_.originalDims.size() > 1
                ? static_cast<int>(params_.originalDims[1])
                : 1);
    }
    return {100, 1};  // Default
}

// Utility functions

TileCompressionType compressionFromString(const std::string& name) {
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Remove quotes
    while (!upper.empty() && (upper.front() == '\'' || upper.front() == ' ')) {
        upper.erase(0, 1);
    }
    while (!upper.empty() && (upper.back() == '\'' || upper.back() == ' ')) {
        upper.pop_back();
    }

    if (upper == "RICE_1" || upper == "RICE_ONE" || upper == "RICE") {
        return TileCompressionType::RICE_1;
    }
    if (upper == "GZIP_1" || upper == "GZIP") {
        return TileCompressionType::GZIP_1;
    }
    if (upper == "GZIP_2") {
        return TileCompressionType::GZIP_2;
    }
    if (upper == "HCOMPRESS_1" || upper == "HCOMPRESS") {
        return TileCompressionType::HCOMPRESS_1;
    }
    if (upper == "PLIO_1" || upper == "PLIO") {
        return TileCompressionType::PLIO_1;
    }
    if (upper == "NOCOMPRESS") {
        return TileCompressionType::NOCOMPRESS;
    }

    return TileCompressionType::NONE;
}

std::string compressionToString(TileCompressionType type) {
    switch (type) {
        case TileCompressionType::RICE_1:
            // Note: RICE_ONE is an alias for RICE_1
            return "RICE_1";
        case TileCompressionType::GZIP_1:
            return "GZIP_1";
        case TileCompressionType::GZIP_2:
            return "GZIP_2";
        case TileCompressionType::HCOMPRESS_1:
            return "HCOMPRESS_1";
        case TileCompressionType::PLIO_1:
            return "PLIO_1";
        case TileCompressionType::NOCOMPRESS:
            return "NOCOMPRESS";
        case TileCompressionType::NONE:
        default:
            return "NONE";
    }
}

bool isLossless(TileCompressionType type, double scale) {
    switch (type) {
        case TileCompressionType::RICE_1:
        // Note: RICE_ONE is an alias for RICE_1
        case TileCompressionType::GZIP_1:
        case TileCompressionType::GZIP_2:
        case TileCompressionType::PLIO_1:
        case TileCompressionType::NOCOMPRESS:
        case TileCompressionType::NONE:
            return true;
        case TileCompressionType::HCOMPRESS_1:
            return scale == 0.0;
        default:
            return false;
    }
}

}  // namespace atom::image::fits
