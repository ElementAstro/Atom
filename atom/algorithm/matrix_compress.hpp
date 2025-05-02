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
#include <iostream>
#include <string>
#include <vector>

#include "atom/error/exception.hpp"

class MatrixCompressException : public atom::error::Exception {
public:
    using atom::error::Exception::Exception;
};

#define THROW_MATRIX_COMPRESS_EXCEPTION(...)                      \
    throw MatrixCompressException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                  ATOM_FUNC_NAME, __VA_ARGS__);

#define THROW_NESTED_MATRIX_COMPRESS_EXCEPTION(...)                        \
    MatrixCompressException::rethrowNested(ATOM_FILE_NAME, ATOM_FILE_LINE, \
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

// 添加概念约束以确保Matrix类型满足要求
template <typename T>
concept MatrixLike = requires(T m) {
    { m.size() } -> std::convertible_to<std::size_t>;
    { m[0].size() } -> std::convertible_to<std::size_t>;
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
    using CompressedData = std::vector<std::pair<char, int>>;

    /**
     * @brief Compresses a matrix using run-length encoding.
     * @param matrix The matrix to compress.
     * @return The compressed data.
     * @throws MatrixCompressException if compression fails.
     */
    static auto compress(const Matrix& matrix) -> CompressedData;

    /**
     * @brief 使用多线程压缩大型矩阵
     * @param matrix 要压缩的矩阵
     * @param thread_count 使用的线程数，默认为系统可用线程数
     * @return 压缩后的数据
     * @throws MatrixCompressException 如果压缩失败
     */
    static auto compressParallel(const Matrix& matrix, int thread_count = 0)
        -> CompressedData;

    /**
     * @brief Decompresses data into a matrix.
     * @param compressed The compressed data.
     * @param rows The number of rows in the decompressed matrix.
     * @param cols The number of columns in the decompressed matrix.
     * @return The decompressed matrix.
     * @throws MatrixDecompressException if decompression fails.
     */
    static auto decompress(const CompressedData& compressed, int rows, int cols)
        -> Matrix;

    /**
     * @brief 使用多线程解压缩大型矩阵
     * @param compressed 压缩的数据
     * @param rows 解压后矩阵的行数
     * @param cols 解压后矩阵的列数
     * @param thread_count 使用的线程数，默认为系统可用线程数
     * @return 解压后的矩阵
     * @throws MatrixDecompressException 如果解压失败
     */
    static auto decompressParallel(const CompressedData& compressed, int rows,
                                   int cols, int thread_count = 0) -> Matrix;

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
    static auto generateRandomMatrix(int rows, int cols,
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
        const M& original, const CompressedData& compressed) noexcept -> double;

    /**
     * @brief Downsamples a matrix by a given factor.
     * @param matrix The matrix to downsample.
     * @param factor The downsampling factor.
     * @return The downsampled matrix.
     * @throws std::invalid_argument if factor is not positive.
     */
    template <MatrixLike M>
    static auto downsample(const M& matrix, int factor) -> Matrix;

    /**
     * @brief Upsamples a matrix by a given factor.
     * @param matrix The matrix to upsample.
     * @param factor The upsampling factor.
     * @return The upsampled matrix.
     * @throws std::invalid_argument if factor is not positive.
     */
    template <MatrixLike M>
    static auto upsample(const M& matrix, int factor) -> Matrix;

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
    static auto calculateMSE(const M1& matrix1, const M2& matrix2) -> double;

private:
    // 用于SIMD处理的内部方法
    static auto compressWithSIMD(const Matrix& matrix) -> CompressedData;
    static auto decompressWithSIMD(const CompressedData& compressed, int rows,
                                   int cols) -> Matrix;
};

// 实现模板函数
template <MatrixLike M>
void MatrixCompressor::printMatrix(const M& matrix) noexcept {
    for (const auto& row : matrix) {
        for (const auto& ch : row) {
            std::cout << ch << ' ';
        }
        std::cout << '\n';
    }
}

template <MatrixLike M>
auto MatrixCompressor::calculateCompressionRatio(
    const M& original, const CompressedData& compressed) noexcept -> double {
    if (original.empty() || original[0].empty()) {
        return 0.0;
    }

    size_t originalSize = 0;
    for (const auto& row : original) {
        originalSize += row.size() * sizeof(char);
    }

    size_t compressedSize = compressed.size() * (sizeof(char) + sizeof(int));
    return static_cast<double>(compressedSize) /
           static_cast<double>(originalSize);
}

template <MatrixLike M>
auto MatrixCompressor::downsample(const M& matrix, int factor) -> Matrix {
    if (factor <= 0) {
        THROW_INVALID_ARGUMENT("Downsampling factor must be positive");
    }

    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    int rows = static_cast<int>(matrix.size());
    int cols = static_cast<int>(matrix[0].size());
    int newRows = std::max(1, rows / factor);
    int newCols = std::max(1, cols / factor);

    Matrix downsampled(newRows, std::vector<char>(newCols));

    try {
        for (int i = 0; i < newRows; ++i) {
            for (int j = 0; j < newCols; ++j) {
                // 使用简单的平均值作为降采样策略
                int sum = 0;
                int count = 0;
                for (int di = 0; di < factor && i * factor + di < rows; ++di) {
                    for (int dj = 0; dj < factor && j * factor + dj < cols;
                         ++dj) {
                        sum += matrix[i * factor + di][j * factor + dj];
                        count++;
                    }
                }
                downsampled[i][j] = static_cast<char>(sum / count);
            }
        }
    } catch (const std::exception& e) {
        THROW_NESTED_MATRIX_COMPRESS_EXCEPTION(
            "Error during matrix downsampling: " + std::string(e.what()));
    }

    return downsampled;
}

template <MatrixLike M>
auto MatrixCompressor::upsample(const M& matrix, int factor) -> Matrix {
    if (factor <= 0) {
        THROW_INVALID_ARGUMENT("Upsampling factor must be positive");
    }

    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    int rows = static_cast<int>(matrix.size());
    int cols = static_cast<int>(matrix[0].size());
    int newRows = rows * factor;
    int newCols = cols * factor;

    Matrix upsampled(newRows, std::vector<char>(newCols));

    try {
        for (int i = 0; i < newRows; ++i) {
            for (int j = 0; j < newCols; ++j) {
                // 使用最近邻插值
                upsampled[i][j] = matrix[i / factor][j / factor];
            }
        }
    } catch (const std::exception& e) {
        THROW_NESTED_MATRIX_COMPRESS_EXCEPTION(
            "Error during matrix upsampling: " + std::string(e.what()));
    }

    return upsampled;
}

template <MatrixLike M1, MatrixLike M2>
    requires std::same_as<std::decay_t<decltype(std::declval<M1>()[0][0])>,
                          std::decay_t<decltype(std::declval<M2>()[0][0])>>
auto MatrixCompressor::calculateMSE(const M1& matrix1, const M2& matrix2)
    -> double {
    if (matrix1.empty() || matrix2.empty() ||
        matrix1.size() != matrix2.size() ||
        matrix1[0].size() != matrix2[0].size()) {
        THROW_INVALID_ARGUMENT("Matrices must have the same dimensions");
    }

    double mse = 0.0;
    auto rows = static_cast<int>(matrix1.size());
    auto cols = static_cast<int>(matrix1[0].size());
    int totalElements = 0;

    try {
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                double diff = static_cast<double>(matrix1[i][j]) -
                              static_cast<double>(matrix2[i][j]);
                mse += diff * diff;
                totalElements++;
            }
        }
    } catch (const std::exception& e) {
        THROW_NESTED_MATRIX_COMPRESS_EXCEPTION("Error calculating MSE: " +
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
void performanceTest(int rows, int cols, bool runParallel = true);
#endif

}  // namespace atom::algorithm

#endif  // ATOM_MATRIX_COMPRESS_HPP
