/*
 * matrix_compress_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This file defines free-standing utility functions for matrix operations
 * including printing, random generation, file I/O, sampling, and quality
 * metrics. Template functions delegate to MatrixCompressor class methods.
 */

#ifndef ATOM_MATRIX_COMPRESS_UTILS_HPP
#define ATOM_MATRIX_COMPRESS_UTILS_HPP

#include "../core/rust_numeric.hpp"
#include "matrix_compress.hpp"

namespace atom::algorithm {

/**
 * @brief Prints the matrix to the standard output.
 * @param matrix The matrix to print.
 */
template <MatrixLike M>
void printMatrix(const M& matrix) noexcept {
    MatrixCompressor::printMatrix(matrix);
}

/**
 * @brief Generates a random matrix.
 * @param rows The number of rows in the matrix.
 * @param cols The number of columns in the matrix.
 * @param charset The set of characters to use for generating the matrix.
 * @return The generated random matrix.
 * @throws std::invalid_argument if rows or cols are not positive.
 */
auto generateRandomMatrix(i32 rows, i32 cols,
                           std::string_view charset = "ABCD")
    -> MatrixCompressor::Matrix;

/**
 * @brief Saves the compressed data to a file.
 * @param compressed The compressed data to save.
 * @param filename The name of the file to save the data to.
 * @throws FileOpenException if the file cannot be opened.
 */
void saveCompressedToFile(const MatrixCompressor::CompressedData& compressed,
                           std::string_view filename);

/**
 * @brief Loads compressed data from a file.
 * @param filename The name of the file to load the data from.
 * @return The loaded compressed data.
 * @throws FileOpenException if the file cannot be opened.
 */
auto loadCompressedFromFile(std::string_view filename)
    -> MatrixCompressor::CompressedData;

/**
 * @brief Calculates the compression ratio.
 * @param original The original matrix.
 * @param compressed The compressed data.
 * @return The compression ratio.
 */
template <MatrixLike M>
auto calculateCompressionRatio(
    const M& original,
    const MatrixCompressor::CompressedData& compressed) noexcept -> f64 {
    return MatrixCompressor::calculateCompressionRatio(original, compressed);
}

/**
 * @brief Downsamples a matrix by a given factor.
 * @param matrix The matrix to downsample.
 * @param factor The downsampling factor.
 * @return The downsampled matrix.
 * @throws std::invalid_argument if factor is not positive.
 */
template <MatrixLike M>
auto downsample(const M& matrix, i32 factor) -> MatrixCompressor::Matrix {
    return MatrixCompressor::downsample(matrix, factor);
}

/**
 * @brief Upsamples a matrix by a given factor.
 * @param matrix The matrix to upsample.
 * @param factor The upsampling factor.
 * @return The upsampled matrix.
 * @throws std::invalid_argument if factor is not positive.
 */
template <MatrixLike M>
auto upsample(const M& matrix, i32 factor) -> MatrixCompressor::Matrix {
    return MatrixCompressor::upsample(matrix, factor);
}

/**
 * @brief Calculates the mean squared error (MSE) between two matrices.
 * @param matrix1 The first matrix.
 * @param matrix2 The second matrix.
 * @return The mean squared error.
 * @throws std::invalid_argument if matrices have different dimensions.
 */
template <MatrixLike M1, MatrixLike M2>
    requires std::same_as<std::decay_t<decltype(std::declval<M1>()[0][0])>,
                          std::decay_t<decltype(std::declval<M2>()[0][0])>>
auto calculateMSE(const M1& matrix1, const M2& matrix2) -> f64 {
    return MatrixCompressor::calculateMSE(matrix1, matrix2);
}

#if ATOM_ENABLE_DEBUG
/**
 * @brief Runs a performance test on matrix compression and decompression.
 * @param rows The number of rows in the test matrix.
 * @param cols The number of columns in the test matrix.
 * @param runParallel Whether to test parallel versions.
 */
void performanceTest(i32 rows, i32 cols, bool runParallel = true);
#endif

}  // namespace atom::algorithm

#endif  // ATOM_MATRIX_COMPRESS_UTILS_HPP
