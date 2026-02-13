#include "matrix_compress.hpp"

#include <algorithm>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"
#include "matrix_compress_parallel.hpp"
#include "matrix_compress_utils.hpp"

#ifdef __AVX2__
#define USE_SIMD 2  // AVX2
#include <immintrin.h>
#elif defined(__SSE4_1__)
#define USE_SIMD 1  // SSE4.1
#include <smmintrin.h>
#else
#define USE_SIMD 0
#endif

namespace atom::algorithm {

/* -------------------- Core Compress -------------------- */

auto MatrixCompressor::compress(const Matrix& matrix) -> CompressedData {
    // Input validation
    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    try {
        // Use SIMD optimized version if available
#if USE_SIMD > 0
        return compressWithSIMD(matrix);
#else
        CompressedData compressed;
        compressed.reserve(
            std::min<usize>(1000, matrix.size() * matrix[0].size() / 2));

        char currentChar = matrix[0][0];
        i32 count = 0;

        // Use C++20 ranges
        for (const auto& row : matrix) {
            for (const char ch : row) {
                if (ch == currentChar) {
                    count++;
                } else {
                    compressed.emplace_back(currentChar, count);
                    currentChar = ch;
                    count = 1;
                }
            }
        }

        if (count > 0) {
            compressed.emplace_back(currentChar, count);
        }

        return compressed;
#endif
    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION("Error during matrix compression: " +
                                        std::string(e.what()));
    }
}

/* -------------------- Core Decompress -------------------- */

auto MatrixCompressor::decompress(const CompressedData& compressed, i32 rows,
                                  i32 cols) -> Matrix {
    // Handle empty matrix case
    if (rows == 0 && cols == 0 && compressed.empty()) {
        return Matrix{};
    }

    if (rows <= 0 || cols <= 0) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Invalid dimensions: rows and cols must be positive");
    }

    if (compressed.empty()) {
        return Matrix(rows, std::vector<char>(cols, 0));
    }

    try {
#if USE_SIMD > 0
        return decompressWithSIMD(compressed, rows, cols);
#else
        Matrix matrix(rows, std::vector<char>(cols));
        i32 index = 0;
        i32 totalElements = rows * cols;
        usize elementCount = 0;

        for (const auto& [ch, count] : compressed) {
            elementCount += count;
        }

        if (elementCount != static_cast<usize>(totalElements)) {
            THROW_MATRIX_DECOMPRESS_EXCEPTION(
                "Decompression error: Element count mismatch - expected " +
                std::to_string(totalElements) + ", got " +
                std::to_string(elementCount));
        }

        for (const auto& [ch, count] : compressed) {
            for (i32 i = 0; i < count; ++i) {
                i32 row = index / cols;
                i32 col = index % cols;

                if (row >= rows || col >= cols) {
                    THROW_MATRIX_DECOMPRESS_EXCEPTION(
                        "Decompression error: Index out of bounds at " +
                        std::to_string(index) + " (row=" + std::to_string(row) +
                        ", col=" + std::to_string(col) + ")");
                }

                matrix[row][col] = ch;
                index++;
            }
        }

        return matrix;
#endif
    } catch (const std::exception& e) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Error during matrix decompression: " + std::string(e.what()));
    }
}

/* -------------------- Parallel Delegates -------------------- */

auto MatrixCompressor::compressParallel(const Matrix& matrix, i32 thread_count)
    -> CompressedData {
    return compressMatrixParallel(matrix, thread_count);
}

auto MatrixCompressor::decompressParallel(const CompressedData& compressed,
                                          i32 rows, i32 cols, i32 thread_count)
    -> Matrix {
    return decompressMatrixParallel(compressed, rows, cols, thread_count);
}

/* -------------------- Utility Delegates -------------------- */

auto MatrixCompressor::generateRandomMatrix(i32 rows, i32 cols,
                                            std::string_view charset)
    -> Matrix {
    return atom::algorithm::generateRandomMatrix(rows, cols, charset);
}

void MatrixCompressor::saveCompressedToFile(const CompressedData& compressed,
                                            std::string_view filename) {
    atom::algorithm::saveCompressedToFile(compressed, filename);
}

auto MatrixCompressor::loadCompressedFromFile(std::string_view filename)
    -> CompressedData {
    return atom::algorithm::loadCompressedFromFile(filename);
}

/* -------------------- SIMD Compress -------------------- */

auto MatrixCompressor::compressWithSIMD(const Matrix& matrix)
    -> CompressedData {
    CompressedData compressed;
    compressed.reserve(
        std::min<usize>(1000, matrix.size() * matrix[0].size() / 4));

    char currentChar = matrix[0][0];
    i32 count = 0;

#if USE_SIMD == 2  // AVX2
    for (const auto& row : matrix) {
        usize i = 0;
        for (; i + 32 <= row.size(); i += 32) {
            __m256i chars1 =
                _mm256_load_si256(reinterpret_cast<const __m256i*>(&row[i]));
            __m256i chars2 = _mm256_load_si256(
                reinterpret_cast<const __m256i*>(&row[i + 16]));

            for (i32 j = 0; j < 16; ++j) {
                char ch = reinterpret_cast<const char*>(&chars1)[j];
                if (ch == currentChar) {
                    count++;
                } else {
                    compressed.emplace_back(currentChar, count);
                    currentChar = ch;
                    count = 1;
                }
            }

            for (i32 j = 0; j < 16; ++j) {
                char ch = reinterpret_cast<const char*>(&chars2)[j];
                if (ch == currentChar) {
                    count++;
                } else {
                    compressed.emplace_back(currentChar, count);
                    currentChar = ch;
                    count = 1;
                }
            }
        }

        for (; i < row.size(); ++i) {
            char ch = row[i];
            if (ch == currentChar) {
                count++;
            } else {
                compressed.emplace_back(currentChar, count);
                currentChar = ch;
                count = 1;
            }
        }
    }
#elif USE_SIMD == 1
    for (const auto& row : matrix) {
        usize i = 0;
        for (; i + 16 <= row.size(); i += 16) {
            __m128i chars =
                _mm_load_si128(reinterpret_cast<const __m128i*>(&row[i]));

            for (i32 j = 0; j < 16; ++j) {
                char ch = reinterpret_cast<const char*>(&chars)[j];
                if (ch == currentChar) {
                    count++;
                } else {
                    compressed.emplace_back(currentChar, count);
                    currentChar = ch;
                    count = 1;
                }
            }
        }

        for (; i < row.size(); ++i) {
            char ch = row[i];
            if (ch == currentChar) {
                count++;
            } else {
                compressed.emplace_back(currentChar, count);
                currentChar = ch;
                count = 1;
            }
        }
    }
#else
    for (const auto& row : matrix) {
        for (char ch : row) {
            if (ch == currentChar) {
                count++;
            } else {
                compressed.emplace_back(currentChar, count);
                currentChar = ch;
                count = 1;
            }
        }
    }
#endif

    if (count > 0) {
        compressed.emplace_back(currentChar, count);
    }

    return compressed;
}

/* -------------------- SIMD Decompress -------------------- */

auto MatrixCompressor::decompressWithSIMD(const CompressedData& compressed,
                                          i32 rows, i32 cols) -> Matrix {
    Matrix matrix(rows, std::vector<char>(cols));
    i32 index = 0;
    i32 total_elements = rows * cols;

    usize elementCount = 0;
    for (const auto& [ch, count] : compressed) {
        elementCount += count;
    }

    if (elementCount != static_cast<usize>(total_elements)) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Decompression error: Element count mismatch - expected " +
            std::to_string(total_elements) + ", got " +
            std::to_string(elementCount));
    }

#if USE_SIMD == 2  // AVX2
    for (const auto& [ch, count] : compressed) {
        __m256i chars = _mm256_set1_epi8(ch);
        for (i32 i = 0; i < count; i += 32) {
            i32 remaining = std::min(32, count - i);
            for (i32 j = 0; j < remaining; ++j) {
                i32 row = index / cols;
                i32 col = index % cols;
                if (row >= rows || col >= cols) {
                    THROW_MATRIX_DECOMPRESS_EXCEPTION(
                        "Decompression error: Index out of bounds at " +
                        std::to_string(index) + " (row=" + std::to_string(row) +
                        ", col=" + std::to_string(col) + ")");
                }
                matrix[row][col] = reinterpret_cast<const char*>(&chars)[j];
                index++;
            }
        }
    }
#elif USE_SIMD == 1  // SSE4.1
    for (const auto& [ch, count] : compressed) {
        __m128i chars = _mm_set1_epi8(ch);
        for (i32 i = 0; i < count; i += 16) {
            i32 remaining = std::min(16, count - i);
            for (i32 j = 0; j < remaining; ++j) {
                i32 row = index / cols;
                i32 col = index % cols;
                if (row >= rows || col >= cols) {
                    THROW_MATRIX_DECOMPRESS_EXCEPTION(
                        "Decompression error: Index out of bounds at " +
                        std::to_string(index) + " (row=" + std::to_string(row) +
                        ", col=" + std::to_string(col) + ")");
                }
                matrix[row][col] = reinterpret_cast<const char*>(&chars)[j];
                index++;
            }
        }
    }
#else
    for (const auto& [ch, count] : compressed) {
        for (i32 i = 0; i < count; ++i) {
            i32 row = index / cols;
            i32 col = index % cols;
            if (row >= rows || col >= cols) {
                THROW_MATRIX_DECOMPRESS_EXCEPTION(
                    "Decompression error: Index out of bounds at " +
                    std::to_string(index) + " (row=" + std::to_string(row) +
                    ", col=" + std::to_string(col) + ")");
            }
            matrix[row][col] = ch;
            index++;
        }
    }
#endif

    return matrix;
}

}  // namespace atom::algorithm
