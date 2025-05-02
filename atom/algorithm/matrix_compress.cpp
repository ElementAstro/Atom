#include "matrix_compress.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <iostream>
#include <random>
#include <thread>

#include "atom/error/exception.hpp"

#ifdef __AVX2__
#define USE_SIMD 2  // AVX2
#include <immintrin.h>
#elif defined(__SSE4_1__)
#define USE_SIMD 1  // SSE4.1
#include <smmintrin.h>
#else
#define USE_SIMD 0
#endif

#ifdef ATOM_USE_BOOST
#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>
#endif

namespace atom::algorithm {

// 定义压缩/解压缩线程的默认数量
static size_t getDefaultThreadCount() noexcept {
    return std::max(1u, std::thread::hardware_concurrency());
}

auto MatrixCompressor::compress(const Matrix& matrix) -> CompressedData {
    // 输入验证
    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    try {
        // 如果支持SIMD，使用SIMD优化版本
#if USE_SIMD > 0
        return compressWithSIMD(matrix);
#else
        CompressedData compressed;
        compressed.reserve(
            std::min<size_t>(1000, matrix.size() * matrix[0].size() / 2));

        char currentChar = matrix[0][0];
        int count = 0;

        // 使用C++20 ranges
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

auto MatrixCompressor::compressParallel(const Matrix& matrix, int thread_count)
    -> CompressedData {
    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    size_t num_threads = thread_count > 0 ? static_cast<size_t>(thread_count)
                                          : getDefaultThreadCount();

    if (matrix.size() < num_threads ||
        matrix.size() * matrix[0].size() < 10000) {
        return compress(matrix);
    }

    try {
        size_t rows_per_thread = matrix.size() / num_threads;
        std::vector<std::future<CompressedData>> futures;
        futures.reserve(num_threads);

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start_row = t * rows_per_thread;
            size_t end_row = (t == num_threads - 1) ? matrix.size()
                                                    : (t + 1) * rows_per_thread;

            futures.push_back(
                std::async(std::launch::async, [&matrix, start_row, end_row]() {
                    CompressedData result;
                    if (start_row >= end_row)
                        return result;

                    char currentChar = matrix[start_row][0];
                    int count = 0;

                    for (size_t i = start_row; i < end_row; ++i) {
                        for (char ch : matrix[i]) {
                            if (ch == currentChar) {
                                count++;
                            } else {
                                result.emplace_back(currentChar, count);
                                currentChar = ch;
                                count = 1;
                            }
                        }
                    }

                    if (count > 0) {
                        result.emplace_back(currentChar, count);
                    }

                    return result;
                }));
        }

        CompressedData result;
        for (auto& future : futures) {
            auto partial = future.get();
            if (result.empty()) {
                result = std::move(partial);
            } else if (!partial.empty()) {
                if (result.back().first == partial.front().first) {
                    result.back().second += partial.front().second;
                    result.insert(result.end(), std::next(partial.begin()),
                                  partial.end());
                } else {
                    result.insert(result.end(), partial.begin(), partial.end());
                }
            }
        }

        return result;
    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION(
            "Error during parallel matrix compression: " +
            std::string(e.what()));
    }
}

auto MatrixCompressor::decompress(const CompressedData& compressed, int rows,
                                  int cols) -> Matrix {
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
        int index = 0;
        int totalElements = rows * cols;
        size_t elementCount = 0;

        for (const auto& [ch, count] : compressed) {
            elementCount += count;
        }

        if (elementCount != static_cast<size_t>(totalElements)) {
            THROW_MATRIX_DECOMPRESS_EXCEPTION(
                "Decompression error: Element count mismatch - expected " +
                std::to_string(totalElements) + ", got " +
                std::to_string(elementCount));
        }

        for (const auto& [ch, count] : compressed) {
            for (int i = 0; i < count; ++i) {
                int row = index / cols;
                int col = index % cols;

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

auto MatrixCompressor::decompressParallel(const CompressedData& compressed,
                                          int rows, int cols, int thread_count)
    -> Matrix {
    if (rows <= 0 || cols <= 0) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Invalid dimensions: rows and cols must be positive");
    }

    if (compressed.empty()) {
        return Matrix(rows, std::vector<char>(cols, 0));
    }

    if (rows * cols < 10000) {
        return decompress(compressed, rows, cols);
    }

    try {
        size_t num_threads = thread_count > 0
                                 ? static_cast<size_t>(thread_count)
                                 : getDefaultThreadCount();
        num_threads = std::min(num_threads, static_cast<size_t>(rows));

        Matrix result(rows, std::vector<char>(cols));

        std::vector<std::pair<size_t, size_t>> row_ranges;
        std::vector<std::pair<size_t, size_t>> element_ranges;

        size_t rows_per_thread = rows / num_threads;
        size_t elements_per_row = cols;

        for (size_t t = 0; t < num_threads; ++t) {
            size_t start_row = t * rows_per_thread;
            size_t end_row =
                (t == num_threads - 1) ? rows : (t + 1) * rows_per_thread;
            row_ranges.emplace_back(start_row, end_row);

            size_t start_element = start_row * elements_per_row;
            size_t end_element = end_row * elements_per_row;
            element_ranges.emplace_back(start_element, end_element);
        }

        std::vector<size_t> element_offsets = {0};
        for (const auto& [ch, count] : compressed) {
            element_offsets.push_back(element_offsets.back() + count);
        }

        std::vector<std::future<void>> futures;
        for (size_t t = 0; t < num_threads; ++t) {
            futures.push_back(std::async(std::launch::async, [&, t]() {
                size_t start_element = element_ranges[t].first;
                size_t end_element = element_ranges[t].second;

                size_t block_index = 0;
                while (block_index < element_offsets.size() - 1 &&
                       element_offsets[block_index + 1] <= start_element) {
                    block_index++;
                }

                size_t current_element = start_element;
                while (current_element < end_element &&
                       block_index < compressed.size()) {
                    char ch = compressed[block_index].first;
                    size_t block_start = element_offsets[block_index];
                    size_t block_end = element_offsets[block_index + 1];

                    size_t process_start =
                        std::max(current_element, block_start);
                    size_t process_end = std::min(end_element, block_end);

                    for (size_t i = process_start; i < process_end; ++i) {
                        int row = static_cast<int>(i / cols);
                        int col = static_cast<int>(i % cols);
                        result[row][col] = ch;
                    }

                    current_element = process_end;
                    if (current_element >= block_end) {
                        block_index++;
                    }
                }
            }));
        }

        for (auto& future : futures) {
            future.get();
        }

        return result;
    } catch (const std::exception& e) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Error during parallel matrix decompression: " +
            std::string(e.what()));
    }
}

auto MatrixCompressor::compressWithSIMD(const Matrix& matrix)
    -> CompressedData {
    CompressedData compressed;
    compressed.reserve(
        std::min<size_t>(1000, matrix.size() * matrix[0].size() / 4));

    char currentChar = matrix[0][0];
    int count = 0;

#if USE_SIMD == 2  // AVX2
    for (const auto& row : matrix) {
        size_t i = 0;
        for (; i + 32 <= row.size(); i += 32) {
            __m256i chars1 =
                _mm256_load_si256(reinterpret_cast<const __m256i*>(&row[i]));
            __m256i chars2 = _mm256_load_si256(
                reinterpret_cast<const __m256i*>(&row[i + 16]));

            for (int j = 0; j < 16; ++j) {
                char ch = reinterpret_cast<const char*>(&chars1)[j];
                if (ch == currentChar) {
                    count++;
                } else {
                    compressed.emplace_back(currentChar, count);
                    currentChar = ch;
                    count = 1;
                }
            }

            for (int j = 0; j < 16; ++j) {
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
        size_t i = 0;
        for (; i + 16 <= row.size(); i += 16) {
            __m128i chars =
                _mm_load_si128(reinterpret_cast<const __m128i*>(&row[i]));

            for (int j = 0; j < 16; ++j) {
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

auto MatrixCompressor::decompressWithSIMD(const CompressedData& compressed,
                                          int rows, int cols) -> Matrix {
    Matrix matrix(rows, std::vector<char>(cols));
    int index = 0;
    int total_elements = rows * cols;

    size_t elementCount = 0;
    for (const auto& [ch, count] : compressed) {
        elementCount += count;
    }

    if (elementCount != static_cast<size_t>(total_elements)) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Decompression error: Element count mismatch - expected " +
            std::to_string(total_elements) + ", got " +
            std::to_string(elementCount));
    }

#if USE_SIMD == 2  // AVX2
    for (const auto& [ch, count] : compressed) {
        __m256i chars = _mm256_set1_epi8(ch);
        for (int i = 0; i < count; i += 32) {
            int remaining = std::min(32, count - i);
            for (int j = 0; j < remaining; ++j) {
                int row = index / cols;
                int col = index % cols;
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
        for (int i = 0; i < count; i += 16) {
            int remaining = std::min(16, count - i);
            for (int j = 0; j < remaining; ++j) {
                int row = index / cols;
                int col = index % cols;
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
        for (int i = 0; i < count; ++i) {
            int row = index / cols;
            int col = index % cols;
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

auto MatrixCompressor::generateRandomMatrix(int rows, int cols,
                                            std::string_view charset)
    -> Matrix {
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> distribution(
        0, static_cast<int>(charset.length()) - 1);

    Matrix matrix(rows, std::vector<char>(cols));
    for (auto& row : matrix) {
        std::ranges::generate(row.begin(), row.end(), [&]() {
            return charset[distribution(generator)];
        });
    }
    return matrix;
}

void MatrixCompressor::saveCompressedToFile(const CompressedData& compressed,
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

auto MatrixCompressor::loadCompressedFromFile(std::string_view filename)
    -> CompressedData {
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

    CompressedData compressed;
    char ch;
    int count;
    while (file.read(reinterpret_cast<char*>(&ch), sizeof(ch)) &&
           file.read(reinterpret_cast<char*>(&count), sizeof(count))) {
        compressed.emplace_back(ch, count);
    }

    return compressed;
}

#if ATOM_ENABLE_DEBUG
void performanceTest(int rows, int cols, bool runParallel) {
    auto matrix = MatrixCompressor::generateRandomMatrix(rows, cols);

    auto start = std::chrono::high_resolution_clock::now();
    auto compressed = MatrixCompressor::compress(matrix);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> compression_time = end - start;

    start = std::chrono::high_resolution_clock::now();
    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);
    end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> decompression_time = end - start;

    double compression_ratio =
        MatrixCompressor::calculateCompressionRatio(matrix, compressed);

    std::cout << "Matrix size: " << rows << "x" << cols << "\n";
    std::cout << "Compression time: " << compression_time.count() << " ms\n";
    std::cout << "Decompression time: " << decompression_time.count()
              << " ms\n";
    std::cout << "Compression ratio: " << compression_ratio << "\n";
    std::cout << "Compressed size: " << compressed.size() << " elements\n";

    if (runParallel) {
        start = std::chrono::high_resolution_clock::now();
        compressed = MatrixCompressor::compressParallel(matrix);
        end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> parallel_compression_time =
            end - start;

        start = std::chrono::high_resolution_clock::now();
        decompressed =
            MatrixCompressor::decompressParallel(compressed, rows, cols);
        end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> parallel_decompression_time =
            end - start;

        std::cout << "\nParallel processing:\n";
        std::cout << "Compression time: " << parallel_compression_time.count()
                  << " ms\n";
        std::cout << "Decompression time: "
                  << parallel_decompression_time.count() << " ms\n";
    }
}
#endif

}  // namespace atom::algorithm