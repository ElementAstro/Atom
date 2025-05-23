/*
 * matrix_compress.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This file defines the MatrixCompressor class for compressing and
 * decompressing matrices using run-length encoding, with support for
 * parallel processing and SIMD optimizations.
 */

#ifndef ATOM_MATRIX_COMPRESS_HPP
#define ATOM_MATRIX_COMPRESS_HPP

#include <concepts>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/algorithm/rust_numeric.hpp"
#include "atom/error/exception.hpp"

class MatrixCompressException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_MATRIX_COMPRESS_EXCEPTION(...)                      \
    throw MatrixCompressException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__);

class MatrixDecompressException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_MATRIX_DECOMPRESS_EXCEPTION(...)                      \
    throw MatrixDecompressException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                    ATOM_FUNC_NAME, __VA_ARGS__);

#define THROW_NESTED_MATRIX_DECOMPRESS_EXCEPTION(...)                        \
    MatrixDecompressException::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                             ATOM_FUNC_NAME, __VA_ARGS__);

namespace atom::algorithm {

// Concept constraints to ensure Matrix type meets requirements
template <typename T>
concept MatrixLike = requires(T m) {
    { m.size() } -> std::convertible_to<usize>;
    { m[0].size() } -> std::convertible_to<usize>;
    { m[0][0] } -> std::convertible_to<char>;
};

/**
 * @class MatrixCompressor
 * @brief A class for compressing and decompressing matrices with C++20
 * features.
 */
class MatrixCompressor {
public:
    using Matrix = std::vector<std::vector<char>>;
    using CompressedData = std::vector<std::pair<char, i32>>;

    /**
     * @brief Compresses a matrix using run-length encoding.
     * @param matrix The matrix to compress.
     * @return The compressed data.
     * @throws MatrixCompressException if compression fails.
     */
    static auto compress(const Matrix& matrix) -> CompressedData;

    /**
     * @brief Compress a large matrix using multiple threads
     * @param matrix The matrix to compress
     * @param thread_count Number of threads to use, defaults to system
     * available threads
     * @return The compressed data
     * @throws MatrixCompressException if compression fails
     */
    static auto compressParallel(const Matrix& matrix, i32 thread_count = 0)
        -> CompressedData;

    /**
     * @brief Decompresses data into a matrix.
     * @param compressed The compressed data.
     * @param rows The number of rows in the decompressed matrix.
     * @param cols The number of columns in the decompressed matrix.
     * @return The decompressed matrix.
     * @throws MatrixDecompressException if decompression fails.
     */
    static auto decompress(const CompressedData& compressed, i32 rows, i32 cols)
        -> Matrix;

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
    static auto decompressParallel(const CompressedData& compressed, i32 rows,
                                   i32 cols, i32 thread_count = 0) -> Matrix;

    /**
     * @brief Prints the matrix to the standard output.
     * @param matrix The matrix to print.
     */
    template <MatrixLike M>
    static void printMatrix(const M& matrix) noexcept;

    /**
     * @brief Generates a random matrix.
     * @param rows The number of rows in the matrix.
     * @param cols The number of columns in the matrix.
     * @param charset The set of characters to use for generating the matrix.
     * @return The generated random matrix.
     * @throws std::invalid_argument if rows or cols are not positive.
     */
    static auto generateRandomMatrix(i32 rows, i32 cols,
                                     std::string_view charset = "ABCD")
        -> Matrix;

    /**
     * @brief Saves the compressed data to a file.
     * @param compressed The compressed data to save.
     * @param filename The name of the file to save the data to.
     * @throws FileOpenException if the file cannot be opened.
     */
    static void saveCompressedToFile(const CompressedData& compressed,
                                     std::string_view filename);

    /**
     * @brief Loads compressed data from a file.
     * @param filename The name of the file to load the data from.
     * @return The loaded compressed data.
     * @throws FileOpenException if the file cannot be opened.
     */
    static auto loadCompressedFromFile(std::string_view filename)
        -> CompressedData;

    /**
     * @brief Calculates the compression ratio.
     * @param original The original matrix.
     * @param compressed The compressed data.
     * @return The compression ratio.
     */
    template <MatrixLike M>
    static auto calculateCompressionRatio(
        const M& original, const CompressedData& compressed) noexcept -> f64;

    /**
     * @brief Downsamples a matrix by a given factor.
     * @param matrix The matrix to downsample.
     * @param factor The downsampling factor.
     * @return The downsampled matrix.
     * @throws std::invalid_argument if factor is not positive.
     */
    template <MatrixLike M>
    static auto downsample(const M& matrix, i32 factor) -> Matrix;

    /**
     * @brief Upsamples a matrix by a given factor.
     * @param matrix The matrix to upsample.
     * @param factor The upsampling factor.
     * @return The upsampled matrix.
     * @throws std::invalid_argument if factor is not positive.
     */
    template <MatrixLike M>
    static auto upsample(const M& matrix, i32 factor) -> Matrix;

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
    static auto calculateMSE(const M1& matrix1, const M2& matrix2) -> f64;

private:
    // Internal methods for SIMD processing
    static auto compressWithSIMD(const Matrix& matrix) -> CompressedData;
    static auto decompressWithSIMD(const CompressedData& compressed, i32 rows,
                                   i32 cols) -> Matrix;
};

// Template function implementations
template <MatrixLike M>
void MatrixCompressor::printMatrix(const M& matrix) noexcept {
    for (const auto& row : matrix) {
        for (const auto& ch : row) {
            spdlog::info("{} ", ch);
        }
        spdlog::info("");
    }
}

template <MatrixLike M>
auto MatrixCompressor::calculateCompressionRatio(
    const M& original, const CompressedData& compressed) noexcept -> f64 {
    if (original.empty() || original[0].empty()) {
        return 0.0;
    }

    usize originalSize = 0;
    for (const auto& row : original) {
        originalSize += row.size() * sizeof(char);
    }

    usize compressedSize = compressed.size() * (sizeof(char) + sizeof(i32));
    return static_cast<f64>(compressedSize) / static_cast<f64>(originalSize);
}

template <MatrixLike M>
auto MatrixCompressor::downsample(const M& matrix, i32 factor) -> Matrix {
    if (factor <= 0) {
        THROW_INVALID_ARGUMENT("Downsampling factor must be positive");
    }

    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    i32 rows = static_cast<i32>(matrix.size());
    i32 cols = static_cast<i32>(matrix[0].size());
    i32 newRows = std::max(1, rows / factor);
    i32 newCols = std::max(1, cols / factor);

    Matrix downsampled(newRows, std::vector<char>(newCols));

    try {
        for (i32 i = 0; i < newRows; ++i) {
            for (i32 j = 0; j < newCols; ++j) {
                // Simple averaging as downsampling strategy
                i32 sum = 0;
                i32 count = 0;
                for (i32 di = 0; di < factor && i * factor + di < rows; ++di) {
                    for (i32 dj = 0; di < factor && j * factor + dj < cols;
                         ++dj) {
                        sum += matrix[i * factor + di][j * factor + dj];
                        count++;
                    }
                }
                downsampled[i][j] = static_cast<char>(sum / count);
            }
        }
    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION("Error during matrix downsampling: " +
                                        std::string(e.what()));
    }

    return downsampled;
}

template <MatrixLike M>
auto MatrixCompressor::upsample(const M& matrix, i32 factor) -> Matrix {
    if (factor <= 0) {
        THROW_INVALID_ARGUMENT("Upsampling factor must be positive");
    }

    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    i32 rows = static_cast<i32>(matrix.size());
    i32 cols = static_cast<i32>(matrix[0].size());
    i32 newRows = rows * factor;
    i32 newCols = cols * factor;

    Matrix upsampled(newRows, std::vector<char>(newCols));

    try {
        for (i32 i = 0; i < newRows; ++i) {
            for (i32 j = 0; j < newCols; ++j) {
                // Nearest neighbor interpolation
                upsampled[i][j] = matrix[i / factor][j / factor];
            }
        }
    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION("Error during matrix upsampling: " +
                                        std::string(e.what()));
    }

    return upsampled;
}

template <MatrixLike M1, MatrixLike M2>
    requires std::same_as<std::decay_t<decltype(std::declval<M1>()[0][0])>,
                          std::decay_t<decltype(std::declval<M2>()[0][0])>>
auto MatrixCompressor::calculateMSE(const M1& matrix1, const M2& matrix2)
    -> f64 {
    if (matrix1.empty() || matrix2.empty() ||
        matrix1.size() != matrix2.size() ||
        matrix1[0].size() != matrix2[0].size()) {
        THROW_INVALID_ARGUMENT("Matrices must have the same dimensions");
    }

    f64 mse = 0.0;
    auto rows = static_cast<i32>(matrix1.size());
    auto cols = static_cast<i32>(matrix1[0].size());
    i32 totalElements = 0;

    try {
        for (i32 i = 0; i < rows; ++i) {
            for (i32 j = 0; j < cols; ++j) {
                f64 diff = static_cast<f64>(matrix1[i][j]) -
                           static_cast<f64>(matrix2[i][j]);
                mse += diff * diff;
                totalElements++;
            }
        }
    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION("Error calculating MSE: " +
                                        std::string(e.what()));
    }

    return totalElements > 0 ? (mse / totalElements) : 0.0;
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

#endif  // ATOM_MATRIX_COMPRESS_HPP
