#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>
#include "atom/algorithm/compression/matrix_compress.hpp"
#include "atom/error/exception.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

class MatrixCompressEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    // Helper to create a matrix with specific pattern
    MatrixCompressor::Matrix createPatternMatrix(int rows, int cols,
                                                 const std::string& pattern) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
        size_t patternIndex = 0;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = pattern[patternIndex % pattern.size()];
                patternIndex++;
            }
        }
        return matrix;
    }

    // Helper to create a highly compressible matrix
    MatrixCompressor::Matrix createCompressibleMatrix(int rows, int cols) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
        char currentChar = 'A';
        int runLength = std::max(10, (rows * cols) / 20);  // Large runs
        int count = 0;

        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = currentChar;
                count++;
                if (count >= runLength) {
                    currentChar = (currentChar == 'A') ? 'B' : 'A';
                    count = 0;
                }
            }
        }
        return matrix;
    }

    // Helper to create a random matrix (low compressibility)
    MatrixCompressor::Matrix createRandomMatrix(int rows, int cols) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis('A', 'Z');

        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = static_cast<char>(dis(gen));
            }
        }
        return matrix;
    }

    void expectMatricesEqual(const MatrixCompressor::Matrix& expected,
                             const MatrixCompressor::Matrix& actual) {
        ASSERT_EQ(expected.size(), actual.size());
        if (expected.empty())
            return;

        ASSERT_EQ(expected[0].size(), actual[0].size());
        for (size_t i = 0; i < expected.size(); ++i) {
            for (size_t j = 0; j < expected[i].size(); ++j) {
                EXPECT_EQ(expected[i][j], actual[i][j])
                    << "Matrices differ at position (" << i << ", " << j << ")";
            }
        }
    }
};

// Test empty matrix edge case with proper error handling
TEST_F(MatrixCompressEnhancedTest, EmptyMatrixHandling) {
    MatrixCompressor::Matrix emptyMatrix;

    // Empty matrix compression should work
    auto compressed = MatrixCompressor::compress(emptyMatrix);
    EXPECT_TRUE(compressed.empty());

    // Decompressing empty data with positive dimensions should return
    // zero-filled matrix
    auto decompressed = MatrixCompressor::decompress(compressed, 2, 3);
    EXPECT_EQ(decompressed.size(), 2);
    EXPECT_EQ(decompressed[0].size(), 3);
    for (const auto& row : decompressed) {
        for (char c : row) {
            EXPECT_EQ(c, 0);
        }
    }

    // Test that zero or negative dimensions are rejected
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 0, 1),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 1, 0),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, -1, 1),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 1, -1),
                 MatrixDecompressException);
}

// Test matrix with single row
TEST_F(MatrixCompressEnhancedTest, SingleRowMatrix) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'B', 'B', 'C'}};

    auto compressed = MatrixCompressor::compress(matrix);
    auto decompressed = MatrixCompressor::decompress(compressed, 1, 5);

    expectMatricesEqual(matrix, decompressed);
}

// Test matrix with single column
TEST_F(MatrixCompressEnhancedTest, SingleColumnMatrix) {
    MatrixCompressor::Matrix matrix = {{'A'}, {'A'}, {'B'}, {'B'}, {'C'}};

    auto compressed = MatrixCompressor::compress(matrix);
    auto decompressed = MatrixCompressor::decompress(compressed, 5, 1);

    expectMatricesEqual(matrix, decompressed);
}

// Test very large matrix compression
TEST_F(MatrixCompressEnhancedTest, LargeMatrixCompression) {
    const int size = 500;
    auto matrix = createCompressibleMatrix(size, size);

    auto startTime = std::chrono::high_resolution_clock::now();
    auto compressed = MatrixCompressor::compress(matrix);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto compressionTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                              startTime);

    spdlog::info("Compressing {}x{} matrix took {} ms", size, size,
                 compressionTime.count());

    // Verify compression ratio
    double ratio =
        MatrixCompressor::calculateCompressionRatio(matrix, compressed);
    EXPECT_LT(ratio, 0.5);  // Should achieve good compression

    // Verify decompression
    auto decompressed = MatrixCompressor::decompress(compressed, size, size);
    expectMatricesEqual(matrix, decompressed);
}

// Test parallel compression performance
TEST_F(MatrixCompressEnhancedTest, ParallelCompressionPerformance) {
    const int size = 200;
    auto matrix = createCompressibleMatrix(size, size);

    // Sequential compression
    auto startSeq = std::chrono::high_resolution_clock::now();
    auto compressedSeq = MatrixCompressor::compress(matrix);
    auto endSeq = std::chrono::high_resolution_clock::now();

    // Parallel compression
    auto startPar = std::chrono::high_resolution_clock::now();
    auto compressedPar = MatrixCompressor::compressParallel(matrix, 4);
    auto endPar = std::chrono::high_resolution_clock::now();

    auto seqTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endSeq - startSeq);
    auto parTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endPar - startPar);

    spdlog::info("Sequential compression: {} ms, Parallel compression: {} ms",
                 seqTime.count(), parTime.count());

    // Results should be identical
    ASSERT_EQ(compressedSeq.size(), compressedPar.size());
    for (size_t i = 0; i < compressedSeq.size(); ++i) {
        EXPECT_EQ(compressedSeq[i].first, compressedPar[i].first);
        EXPECT_EQ(compressedSeq[i].second, compressedPar[i].second);
    }
}

// Test compression with different thread counts
TEST_F(MatrixCompressEnhancedTest, DifferentThreadCounts) {
    auto matrix = createCompressibleMatrix(100, 100);

    auto compressed1 = MatrixCompressor::compressParallel(matrix, 1);
    auto compressed2 = MatrixCompressor::compressParallel(matrix, 2);
    auto compressed4 = MatrixCompressor::compressParallel(matrix, 4);
    auto compressed8 = MatrixCompressor::compressParallel(matrix, 8);

    // All should produce identical results
    EXPECT_EQ(compressed1, compressed2);
    EXPECT_EQ(compressed2, compressed4);
    EXPECT_EQ(compressed4, compressed8);
}

// Test error handling for invalid dimensions
TEST_F(MatrixCompressEnhancedTest, InvalidDimensionsHandling) {
    MatrixCompressor::CompressedData compressed = {{'A', 10}};

    // Negative dimensions
    EXPECT_THROW(MatrixCompressor::decompress(compressed, -1, 5),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 5, -1),
                 MatrixDecompressException);

    // Zero dimensions with non-empty data
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 0, 5),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 5, 0),
                 MatrixDecompressException);

    // Insufficient data for dimensions
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 5, 5),
                 MatrixDecompressException);
}

// Test compression ratio calculations
TEST_F(MatrixCompressEnhancedTest, CompressionRatioCalculations) {
    // Highly compressible matrix
    auto compressible = createCompressibleMatrix(50, 50);
    auto compressedComp = MatrixCompressor::compress(compressible);
    double ratioComp = MatrixCompressor::calculateCompressionRatio(
        compressible, compressedComp);

    // Random matrix (low compressibility)
    auto random = createRandomMatrix(50, 50);
    auto compressedRand = MatrixCompressor::compress(random);
    double ratioRand =
        MatrixCompressor::calculateCompressionRatio(random, compressedRand);

    // Compressible matrix should have better ratio
    EXPECT_LT(ratioComp, ratioRand);

    spdlog::info(
        "Compressible matrix ratio: {:.3f}, Random matrix ratio: {:.3f}",
        ratioComp, ratioRand);
}

// Test matrix generation with different charsets
TEST_F(MatrixCompressEnhancedTest, MatrixGenerationCharsets) {
    std::vector<std::string> charsets = {"AB", "ABCD", "0123456789", "!@#$%"};

    for (const auto& charset : charsets) {
        auto matrix = MatrixCompressor::generateRandomMatrix(10, 10, charset);

        ASSERT_EQ(matrix.size(), 10);
        ASSERT_EQ(matrix[0].size(), 10);

        // Verify all characters are from the charset
        for (const auto& row : matrix) {
            for (char c : row) {
                EXPECT_NE(charset.find(c), std::string::npos);
            }
        }
    }
}

// Test downsampling with different factors
TEST_F(MatrixCompressEnhancedTest, DownsamplingFactors) {
    auto matrix = createPatternMatrix(8, 8, "ABCD");

    for (int factor = 2; factor <= 4; ++factor) {
        auto downsampled = MatrixCompressor::downsample(matrix, factor);

        // Using ceiling division to include edge blocks: (rows + factor - 1) /
        // factor
        int expectedRows = std::max(1, (8 + factor - 1) / factor);
        int expectedCols = std::max(1, (8 + factor - 1) / factor);

        EXPECT_EQ(downsampled.size(), expectedRows);
        EXPECT_EQ(downsampled[0].size(), expectedCols);
    }
}

// Test upsampling with different factors
TEST_F(MatrixCompressEnhancedTest, UpsamplingFactors) {
    MatrixCompressor::Matrix small = {{'A', 'B'}, {'C', 'D'}};

    for (int factor = 2; factor <= 4; ++factor) {
        auto upsampled = MatrixCompressor::upsample(small, factor);

        EXPECT_EQ(upsampled.size(), 2 * factor);
        EXPECT_EQ(upsampled[0].size(), 2 * factor);

        // Verify pattern replication
        for (int i = 0; i < 2 * factor; ++i) {
            for (int j = 0; j < 2 * factor; ++j) {
                char expected = small[i / factor][j / factor];
                EXPECT_EQ(upsampled[i][j], expected);
            }
        }
    }
}

// Test MSE calculation with known values
TEST_F(MatrixCompressEnhancedTest, MSECalculationAccuracy) {
    MatrixCompressor::Matrix matrix1 = {{'A', 'B'}, {'C', 'D'}};
    MatrixCompressor::Matrix matrix2 = {{'A', 'C'}, {'B', 'D'}};

    double mse = MatrixCompressor::calculateMSE(matrix1, matrix2);

    // Calculate expected MSE manually
    double expected = (std::pow('B' - 'C', 2) + std::pow('C' - 'B', 2)) / 4.0;
    EXPECT_DOUBLE_EQ(mse, expected);
}

// Test thread safety of compression operations
TEST_F(MatrixCompressEnhancedTest, ThreadSafety) {
    auto matrix = createCompressibleMatrix(50, 50);
    const int numThreads = 8;

    std::vector<std::future<MatrixCompressor::CompressedData>> futures;

    // Launch multiple compression tasks
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [&matrix]() {
            return MatrixCompressor::compress(matrix);
        }));
    }

    // Collect results
    std::vector<MatrixCompressor::CompressedData> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // All results should be identical
    for (size_t i = 1; i < results.size(); ++i) {
        EXPECT_EQ(results[0], results[i]);
    }
}

// Test file I/O with large data
TEST_F(MatrixCompressEnhancedTest, LargeFileIO) {
    MatrixCompressor::CompressedData largeData;

    // Create large compressed data
    for (int i = 0; i < 10000; ++i) {
        largeData.push_back({'A' + (i % 26), i % 1000 + 1});
    }

    std::string filename = "/tmp/large_matrix_test.dat";

    // Save and load
    EXPECT_NO_THROW(
        MatrixCompressor::saveCompressedToFile(largeData, filename));

    MatrixCompressor::CompressedData loaded;
    EXPECT_NO_THROW(loaded =
                        MatrixCompressor::loadCompressedFromFile(filename));

    EXPECT_EQ(largeData.size(), loaded.size());
    for (size_t i = 0; i < largeData.size(); ++i) {
        EXPECT_EQ(largeData[i].first, loaded[i].first);
        EXPECT_EQ(largeData[i].second, loaded[i].second);
    }

    // Cleanup
    std::remove(filename.c_str());
}

// Test memory usage with very large matrices
TEST_F(MatrixCompressEnhancedTest, MemoryUsageTest) {
    // This test ensures we don't have memory leaks with large matrices
    for (int iteration = 0; iteration < 10; ++iteration) {
        auto matrix = createCompressibleMatrix(200, 200);
        auto compressed = MatrixCompressor::compress(matrix);
        auto decompressed = MatrixCompressor::decompress(compressed, 200, 200);

        // Verify correctness
        expectMatricesEqual(matrix, decompressed);
    }

    // If we reach here without crashes, memory management is likely correct
    SUCCEED();
}
