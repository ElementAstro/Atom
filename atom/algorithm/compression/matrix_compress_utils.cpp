/*
 * matrix_compress_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This file implements utility functions for matrix operations including
 * random generation, file I/O, and performance testing.
 */

#include "matrix_compress_utils.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#endif

namespace atom::algorithm {

auto generateRandomMatrix(i32 rows, i32 cols, std::string_view charset)
    -> MatrixCompressor::Matrix {
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<i32> distribution(
        0, static_cast<i32>(charset.length()) - 1);

    MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
    for (auto& row : matrix) {
        std::ranges::generate(row.begin(), row.end(), [&]() {
            return charset[distribution(generator)];
        });
    }
    return matrix;
}

void saveCompressedToFile(const MatrixCompressor::CompressedData& compressed,
                           std::string_view filename) {
#ifdef ATOM_USE_BOOST
    boost::filesystem::path filepath(filename);
    std::ofstream file(filepath.string(), std::ios::binary);
#else
    std::ofstream file(std::string(filename), std::ios::binary);
#endif
    if (!file) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(FileOpenException())
            << boost::errinfo_api_function("Unable to open file for writing: " +
                                           std::string(filename));
#else
        THROW_FAIL_TO_OPEN_FILE("Unable to open file for writing: " +
                                std::string(filename));
#endif
    }

    for (const auto& [ch, count] : compressed) {
        file.write(reinterpret_cast<const char*>(&ch), sizeof(ch));
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    }
}

auto loadCompressedFromFile(std::string_view filename)
    -> MatrixCompressor::CompressedData {
#ifdef ATOM_USE_BOOST
    boost::filesystem::path filepath(filename);
    std::ifstream file(filepath.string(), std::ios::binary);
#else
    std::ifstream file(std::string(filename), std::ios::binary);
#endif
    if (!file) {
#ifdef ATOM_USE_BOOST
        throw boost::enable_error_info(FileOpenException())
            << boost::errinfo_api_function("Unable to open file for reading: " +
                                           std::string(filename));
#else
        THROW_FAIL_TO_OPEN_FILE("Unable to open file for reading: " +
                                std::string(filename));
#endif
    }

    MatrixCompressor::CompressedData compressed;
    char ch;
    i32 count;
    while (file.read(reinterpret_cast<char*>(&ch), sizeof(ch)) &&
           file.read(reinterpret_cast<char*>(&count), sizeof(count))) {
        compressed.emplace_back(ch, count);
    }

    return compressed;
}

#if ATOM_ENABLE_DEBUG
void performanceTest(i32 rows, i32 cols, bool runParallel) {
    auto matrix = generateRandomMatrix(rows, cols);

    auto start = std::chrono::high_resolution_clock::now();
    auto compressed = MatrixCompressor::compress(matrix);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<f64, std::milli> compression_time = end - start;

    start = std::chrono::high_resolution_clock::now();
    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);
    end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<f64, std::milli> decompression_time = end - start;

    f64 compression_ratio = calculateCompressionRatio(matrix, compressed);

    spdlog::info("Matrix size: {}x{}", rows, cols);
    spdlog::info("Compression time: {} ms", compression_time.count());
    spdlog::info("Decompression time: {} ms", decompression_time.count());
    spdlog::info("Compression ratio: {}", compression_ratio);
    spdlog::info("Compressed size: {} elements", compressed.size());

    if (runParallel) {
        start = std::chrono::high_resolution_clock::now();
        compressed = MatrixCompressor::compressParallel(matrix);
        end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<f64, std::milli> parallel_compression_time =
            end - start;

        start = std::chrono::high_resolution_clock::now();
        decompressed =
            MatrixCompressor::decompressParallel(compressed, rows, cols);
        end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<f64, std::milli> parallel_decompression_time =
            end - start;

        spdlog::info("\nParallel processing:");
        spdlog::info("Compression time: {} ms",
                     parallel_compression_time.count());
        spdlog::info("Decompression time: {} ms",
                     parallel_decompression_time.count());
    }
}
#endif

}  // namespace atom::algorithm
