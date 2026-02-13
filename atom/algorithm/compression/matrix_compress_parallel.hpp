/*
 * matrix_compress_parallel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This file declares parallel compression and decompression functions
 * for matrices using multithreading.
 */

#ifndef ATOM_MATRIX_COMPRESS_PARALLEL_HPP
#define ATOM_MATRIX_COMPRESS_PARALLEL_HPP

#include <vector>

#include "../core/rust_numeric.hpp"
#include "matrix_compress.hpp"

namespace atom::algorithm {

/**
 * @brief Compress a large matrix using multiple threads
 * @param matrix The matrix to compress
 * @param thread_count Number of threads to use, defaults to system
 * available threads
 * @return The compressed data
 * @throws MatrixCompressException if compression fails
 */
auto compressMatrixParallel(const MatrixCompressor::Matrix& matrix,
                            i32 thread_count = 0)
    -> MatrixCompressor::CompressedData;

/**
 * @brief Decompress a large matrix using multiple threads
 * @param compressed The compressed data
 * @param rows Number of rows in the decompressed matrix
 * @param cols Number of columns in the decompressed matrix
 * @param thread_count Number of threads to use, defaults to system
 * available threads
 * @return The decompressed matrix
 * @throws MatrixDecompressException if decompression fails
 */
auto decompressMatrixParallel(const MatrixCompressor::CompressedData& compressed,
                              i32 rows, i32 cols, i32 thread_count = 0)
    -> MatrixCompressor::Matrix;

// Helper function to merge two CompressedData vectors
auto mergeCompressedData(const MatrixCompressor::CompressedData& data1,
                         const MatrixCompressor::CompressedData& data2)
    -> MatrixCompressor::CompressedData;

}  // namespace atom::algorithm

#endif  // ATOM_MATRIX_COMPRESS_PARALLEL_HPP
