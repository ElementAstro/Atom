/*
 * matrix_compress_parallel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This file implements parallel compression and decompression functions
 * for matrices using multithreading.
 */

#include "matrix_compress_parallel.hpp"

#include <algorithm>
#include <future>
#include <thread>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

// Define default number of threads for compression/decompression
static usize getDefaultThreadCount() noexcept {
    return std::max(1u, std::thread::hardware_concurrency());
}

// Helper function to merge two CompressedData vectors
auto mergeCompressedData(const MatrixCompressor::CompressedData& data1,
                         const MatrixCompressor::CompressedData& data2)
    -> MatrixCompressor::CompressedData {
    MatrixCompressor::CompressedData merged_data;
    merged_data.reserve(data1.size() + data2.size());

    if (data1.empty()) {
        return data2;
    } else if (data2.empty()) {
        return data1;
    }

    merged_data.insert(merged_data.end(), data1.begin(), data1.end());

    // Merge the last element of data1 with the first element of data2 if they
    // are the same character
    if (merged_data.back().first == data2.front().first) {
        merged_data.back().second += data2.front().second;
        merged_data.insert(merged_data.end(), std::next(data2.begin()),
                           data2.end());
    } else {
        merged_data.insert(merged_data.end(), data2.begin(), data2.end());
    }

    return merged_data;
}

auto compressMatrixParallel(const MatrixCompressor::Matrix& matrix,
                            i32 thread_count)
    -> MatrixCompressor::CompressedData {
    if (matrix.empty() || matrix[0].empty()) {
        return {};
    }

    usize num_threads = thread_count > 0 ? static_cast<usize>(thread_count)
                                         : getDefaultThreadCount();

    if (matrix.size() < num_threads ||
        matrix.size() * matrix[0].size() < 10000) {
        return MatrixCompressor::compress(matrix);
    }

    try {
        usize rows_per_thread = matrix.size() / num_threads;
        std::vector<std::future<MatrixCompressor::CompressedData>> futures;
        futures.reserve(num_threads);

        // Launch initial compression tasks
        for (usize t = 0; t < num_threads; ++t) {
            usize start_row = t * rows_per_thread;
            usize end_row = (t == num_threads - 1) ? matrix.size()
                                                   : (t + 1) * rows_per_thread;

            futures.push_back(
                std::async(std::launch::async, [&matrix, start_row, end_row]() {
                    MatrixCompressor::CompressedData result;
                    if (start_row >= end_row)
                        return result;

                    char currentChar = matrix[start_row][0];
                    i32 count = 0;

                    for (usize i = start_row; i < end_row; ++i) {
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

        // Sequential merging of results to avoid deadlock
        // First, collect all results
        std::vector<MatrixCompressor::CompressedData> results;
        results.reserve(futures.size());
        for (auto& future : futures) {
            results.push_back(future.get());
        }

        // Merge results sequentially
        while (results.size() > 1) {
            std::vector<MatrixCompressor::CompressedData> merged_results;
            merged_results.reserve((results.size() + 1) / 2);
            for (size_t i = 0; i < results.size(); i += 2) {
                if (i + 1 < results.size()) {
                    merged_results.push_back(
                        mergeCompressedData(results[i], results[i + 1]));
                } else {
                    merged_results.push_back(std::move(results[i]));
                }
            }
            results = std::move(merged_results);
        }

        // Return the final result
        return results.empty() ? MatrixCompressor::CompressedData{}
                               : std::move(results[0]);

    } catch (const std::exception& e) {
        THROW_MATRIX_COMPRESS_EXCEPTION(
            "Error during parallel matrix compression: " +
            std::string(e.what()));
    }
}

auto decompressMatrixParallel(const MatrixCompressor::CompressedData& compressed,
                              i32 rows, i32 cols, i32 thread_count)
    -> MatrixCompressor::Matrix {
    if (rows <= 0 || cols <= 0) {
        THROW_MATRIX_DECOMPRESS_EXCEPTION(
            "Invalid dimensions: rows and cols must be positive");
    }

    if (compressed.empty()) {
        return MatrixCompressor::Matrix(rows, std::vector<char>(cols, 0));
    }

    if (rows * cols < 10000) {
        return MatrixCompressor::decompress(compressed, rows, cols);
    }

    try {
        usize num_threads = thread_count > 0 ? static_cast<usize>(thread_count)
                                             : getDefaultThreadCount();
        num_threads = std::min(num_threads, static_cast<usize>(rows));

        MatrixCompressor::Matrix result(rows, std::vector<char>(cols));

        std::vector<std::pair<usize, usize>> row_ranges;
        std::vector<std::pair<usize, usize>> element_ranges;

        usize rows_per_thread = rows / num_threads;
        usize elements_per_row = cols;

        for (usize t = 0; t < num_threads; ++t) {
            usize start_row = t * rows_per_thread;
            usize end_row =
                (t == num_threads - 1) ? rows : (t + 1) * rows_per_thread;
            row_ranges.emplace_back(start_row, end_row);

            usize start_element = start_row * elements_per_row;
            usize end_element = end_row * elements_per_row;
            element_ranges.emplace_back(start_element, end_element);
        }

        std::vector<usize> element_offsets = {0};
        for (const auto& [ch, count] : compressed) {
            element_offsets.push_back(element_offsets.back() + count);
        }

        std::vector<std::future<void>> futures;
        for (usize t = 0; t < num_threads; ++t) {
            futures.push_back(std::async(std::launch::async, [&, t]() {
                usize start_element = element_ranges[t].first;
                usize end_element = element_ranges[t].second;

                usize block_index = 0;
                while (block_index < element_offsets.size() - 1 &&
                       element_offsets[block_index + 1] <= start_element) {
                    block_index++;
                }

                usize current_element = start_element;
                while (current_element < end_element &&
                       block_index < compressed.size()) {
                    char ch = compressed[block_index].first;
                    usize block_start = element_offsets[block_index];
                    usize block_end = element_offsets[block_index + 1];

                    usize process_start =
                        std::max(current_element, block_start);
                    usize process_end = std::min(end_element, block_end);

                    for (usize i = process_start; i < process_end; ++i) {
                        i32 row = static_cast<i32>(i / cols);
                        i32 col = static_cast<i32>(i % cols);
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

}  // namespace atom::algorithm
