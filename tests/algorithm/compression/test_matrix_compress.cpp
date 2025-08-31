#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <chrono>
#include <string>
#include <vector>
#include "atom/algorithm/matrix_compress.hpp"
#include "atom/error/exception.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

class MatrixCompressorTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }

    MatrixCompressor::Matrix generatePatternMatrix(int rows, int cols,
                                                   const std::string& pattern) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
        size_t patternIndex = 0;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = pattern[patternIndex];
                patternIndex = (patternIndex + 1) % pattern.size();
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

    MatrixCompressor::Matrix createRunLengthMatrix(int rows, int cols) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));
        char currentChar = 'A';
        int runLength = 10;
        int count = 0;
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = currentChar;
                ++count;
                if (count >= runLength) {
                    currentChar =
                        (currentChar == 'A' ? 'B'
                                            : (currentChar == 'B' ? 'C' : 'A'));
                    count = 0;
                }
            }
        }
        return matrix;
    }

    std::string getTempFilePath() {
        return "/tmp/matrix_compress_test_" +
               std::to_string(std::chrono::system_clock::now()
                                  .time_since_epoch()
                                  .count()) +
               ".dat";
    }
};

TEST_F(MatrixCompressorTest, BasicCompression) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'A', 'B', 'B'},
                                       {'B', 'C', 'C', 'C', 'C'},
                                       {'C', 'A', 'A', 'A', 'A'}};
    MatrixCompressor::CompressedData expected = {
        {'A', 3}, {'B', 3}, {'C', 5}, {'A', 4}};
    auto compressed = MatrixCompressor::compress(matrix);
    ASSERT_EQ(expected.size(), compressed.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i].first, compressed[i].first);
        EXPECT_EQ(expected[i].second, compressed[i].second);
    }
}

TEST_F(MatrixCompressorTest, BasicDecompression) {
    MatrixCompressor::CompressedData compressed = {
        {'A', 3}, {'B', 3}, {'C', 5}, {'A', 4}};
    MatrixCompressor::Matrix expected = {{'A', 'A', 'A', 'B', 'B'},
                                         {'B', 'C', 'C', 'C', 'C'},
                                         {'C', 'A', 'A', 'A', 'A'}};
    int rows = 3;
    int cols = 5;
    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);
    expectMatricesEqual(expected, decompressed);
}

TEST_F(MatrixCompressorTest, EmptyMatrix) {
    MatrixCompressor::Matrix emptyMatrix;
    auto compressed = MatrixCompressor::compress(emptyMatrix);
    EXPECT_TRUE(compressed.empty());
    auto decompressed = MatrixCompressor::decompress(compressed, 0, 0);
    EXPECT_TRUE(decompressed.empty());
}

TEST_F(MatrixCompressorTest, SingleElementMatrix) {
    MatrixCompressor::Matrix matrix = {{'X'}};
    auto compressed = MatrixCompressor::compress(matrix);
    ASSERT_EQ(1, compressed.size());
    EXPECT_EQ('X', compressed[0].first);
    EXPECT_EQ(1, compressed[0].second);
    auto decompressed = MatrixCompressor::decompress(compressed, 1, 1);
    expectMatricesEqual(matrix, decompressed);
}

TEST_F(MatrixCompressorTest, HomogeneousMatrix) {
    int rows = 5;
    int cols = 5;
    char value = 'Z';
    MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols, value));
    auto compressed = MatrixCompressor::compress(matrix);
    ASSERT_EQ(1, compressed.size());
    EXPECT_EQ(value, compressed[0].first);
    EXPECT_EQ(rows * cols, compressed[0].second);
    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);
    expectMatricesEqual(matrix, decompressed);
}

TEST_F(MatrixCompressorTest, LargeMatrix) {
    int rows = 100;
    int cols = 100;
    auto matrix = createRunLengthMatrix(rows, cols);
    auto compressed = MatrixCompressor::compress(matrix);
    EXPECT_LT(compressed.size(), static_cast<size_t>(rows * cols / 5));
    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);
    expectMatricesEqual(matrix, decompressed);
}

TEST_F(MatrixCompressorTest, ParallelCompression) {
    int rows = 200;
    int cols = 200;
    auto matrix = createRunLengthMatrix(rows, cols);
    auto compressedParallel = MatrixCompressor::compressParallel(matrix, 4);
    auto compressedSequential = MatrixCompressor::compress(matrix);
    ASSERT_EQ(compressedSequential.size(), compressedParallel.size());
    for (size_t i = 0; i < compressedSequential.size(); ++i) {
        EXPECT_EQ(compressedSequential[i].first, compressedParallel[i].first);
        EXPECT_EQ(compressedSequential[i].second, compressedParallel[i].second);
    }
    auto decompressed =
        MatrixCompressor::decompress(compressedParallel, rows, cols);
    expectMatricesEqual(matrix, decompressed);
}

TEST_F(MatrixCompressorTest, ParallelDecompression) {
    int rows = 200;
    int cols = 200;
    auto matrix = createRunLengthMatrix(rows, cols);
    auto compressed = MatrixCompressor::compress(matrix);
    auto decompressedParallel =
        MatrixCompressor::decompressParallel(compressed, rows, cols, 4);
    auto decompressedSequential =
        MatrixCompressor::decompress(compressed, rows, cols);
    expectMatricesEqual(decompressedSequential, decompressedParallel);
    expectMatricesEqual(matrix, decompressedParallel);
}

TEST_F(MatrixCompressorTest, CompressionErrorHandling) {
    MatrixCompressor::Matrix invalidMatrix;
    invalidMatrix.push_back(std::vector<char>{'A', 'B'});
    invalidMatrix.push_back(std::vector<char>{});
    MatrixCompressor::CompressedData result;
    EXPECT_NO_THROW(result = MatrixCompressor::compress(invalidMatrix));
}

TEST_F(MatrixCompressorTest, DecompressionErrorHandling) {
    MatrixCompressor::CompressedData compressed = {{'A', 10}};
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 0, 5),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 5, 0),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, -1, 5),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 2, 3),
                 MatrixDecompressException);
}

TEST_F(MatrixCompressorTest, FileIO) {
    MatrixCompressor::CompressedData compressed = {
        {'A', 100}, {'B', 50}, {'C', 25}};
    std::string filePath = getTempFilePath();
    EXPECT_NO_THROW(
        MatrixCompressor::saveCompressedToFile(compressed, filePath));
    MatrixCompressor::CompressedData loaded;
    EXPECT_NO_THROW(loaded =
                        MatrixCompressor::loadCompressedFromFile(filePath));
    ASSERT_EQ(compressed.size(), loaded.size());
    for (size_t i = 0; i < compressed.size(); ++i) {
        EXPECT_EQ(compressed[i].first, loaded[i].first);
        EXPECT_EQ(compressed[i].second, loaded[i].second);
    }
    EXPECT_THROW(MatrixCompressor::loadCompressedFromFile("/non/existent/file"),
                 atom::error::FailToOpenFile);
}

TEST_F(MatrixCompressorTest, CompressionRatio) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'A', 'A', 'A'},
                                       {'A', 'A', 'A', 'A', 'A'}};
    MatrixCompressor::CompressedData compressed = {{'A', 10}};
    double ratio =
        MatrixCompressor::calculateCompressionRatio(matrix, compressed);
    size_t compressedSize = 1 * (sizeof(char) + sizeof(int));
    size_t originalSize = 10 * sizeof(char);
    double expectedRatio =
        static_cast<double>(compressedSize) / static_cast<double>(originalSize);
    EXPECT_DOUBLE_EQ(expectedRatio, ratio);
}

TEST_F(MatrixCompressorTest, RandomMatrixGeneration) {
    int rows = 10;
    int cols = 10;
    std::string charset = "ABC";
    auto matrix = MatrixCompressor::generateRandomMatrix(rows, cols, charset);
    ASSERT_EQ(rows, static_cast<int>(matrix.size()));
    ASSERT_EQ(cols, static_cast<int>(matrix[0].size()));
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            EXPECT_NE(charset.find(matrix[i][j]), std::string::npos);
        }
    }
}

TEST_F(MatrixCompressorTest, Downsampling) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'B', 'B'},
                                       {'A', 'A', 'B', 'B'},
                                       {'C', 'C', 'D', 'D'},
                                       {'C', 'C', 'D', 'D'}};
    auto downsampled = MatrixCompressor::downsample(matrix, 2);
    MatrixCompressor::Matrix expected = {{'A', 'B'}, {'C', 'D'}};
    ASSERT_EQ(2, static_cast<int>(downsampled.size()));
    ASSERT_EQ(2, static_cast<int>(downsampled[0].size()));
    expectMatricesEqual(expected, downsampled);
}

TEST_F(MatrixCompressorTest, Upsampling) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};
    auto upsampled = MatrixCompressor::upsample(matrix, 2);
    MatrixCompressor::Matrix expected = {{'A', 'A', 'B', 'B'},
                                         {'A', 'A', 'B', 'B'},
                                         {'C', 'C', 'D', 'D'},
                                         {'C', 'C', 'D', 'D'}};
    ASSERT_EQ(4, static_cast<int>(upsampled.size()));
    ASSERT_EQ(4, static_cast<int>(upsampled[0].size()));
    expectMatricesEqual(expected, upsampled);
}

TEST_F(MatrixCompressorTest, InvalidDownsamplingFactor) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};
    EXPECT_THROW(MatrixCompressor::downsample(matrix, 0),
                 atom::error::InvalidArgument);
    EXPECT_THROW(MatrixCompressor::downsample(matrix, -1),
                 atom::error::InvalidArgument);
}

TEST_F(MatrixCompressorTest, InvalidUpsamplingFactor) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};
    EXPECT_THROW(MatrixCompressor::upsample(matrix, 0),
                 atom::error::InvalidArgument);
    EXPECT_THROW(MatrixCompressor::upsample(matrix, -1),
                 atom::error::InvalidArgument);
}

TEST_F(MatrixCompressorTest, MSECalculation) {
    MatrixCompressor::Matrix matrix1 = {{'A', 'B'}, {'C', 'D'}};
    MatrixCompressor::Matrix matrix2 = {{'A', 'C'}, {'B', 'D'}};
    double mse = MatrixCompressor::calculateMSE(matrix1, matrix2);
    double expectedMSE =
        (std::pow('B' - 'C', 2) + std::pow('C' - 'B', 2)) / 4.0;
    EXPECT_DOUBLE_EQ(expectedMSE, mse);
}

TEST_F(MatrixCompressorTest, MSEWithDifferentSizedMatrices) {
    MatrixCompressor::Matrix matrix1 = {{'A', 'B'}, {'C', 'D'}};
    MatrixCompressor::Matrix matrix2 = {{'A', 'B', 'C'}, {'D', 'E', 'F'}};
    EXPECT_THROW(MatrixCompressor::calculateMSE(matrix1, matrix2),
                 atom::error::InvalidArgument);
}

TEST_F(MatrixCompressorTest, UnicodeCharacters) {
    MatrixCompressor::Matrix matrix = {
        {static_cast<char>(0xC3), static_cast<char>(0xA4)},
        {static_cast<char>(0xC3), static_cast<char>(0xB6)}};
    auto compressed = MatrixCompressor::compress(matrix);
    auto decompressed = MatrixCompressor::decompress(compressed, 2, 2);
    expectMatricesEqual(matrix, decompressed);
}

TEST_F(MatrixCompressorTest, MatrixPrinting) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};
    testing::internal::CaptureStdout();
    MatrixCompressor::printMatrix(matrix);
    testing::internal::GetCapturedStdout();
}
