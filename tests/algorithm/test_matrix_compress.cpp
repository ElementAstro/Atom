#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "atom/algorithm/matrix_compress.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Test fixture for MatrixCompressor tests
class MatrixCompressorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Helper function to generate a test matrix with repeated patterns
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

    // Helper function to verify that two matrices are equal
    void expectMatricesEqual(const MatrixCompressor::Matrix& expected,
                             const MatrixCompressor::Matrix& actual) {
        ASSERT_EQ(expected.size(), actual.size())
            << "Matrices have different row counts";
        if (expected.empty())
            return;

        ASSERT_EQ(expected[0].size(), actual[0].size())
            << "Matrices have different column counts";

        for (size_t i = 0; i < expected.size(); ++i) {
            for (size_t j = 0; j < expected[i].size(); ++j) {
                EXPECT_EQ(expected[i][j], actual[i][j])
                    << "Matrices differ at position (" << i << ", " << j << ")";
            }
        }
    }

    // Helper function to create a test matrix with run-length patterns
    MatrixCompressor::Matrix createRunLengthMatrix(int rows, int cols) {
        MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols));

        // Fill with alternating runs of 'A', 'B', 'C'
        char currentChar = 'A';
        int runLength = 10;
        int count = 0;

        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                matrix[i][j] = currentChar;
                count++;

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

    // Create a temporary file path for testing file I/O
    std::string getTempFilePath() {
        return "/tmp/matrix_compress_test_" +
               std::to_string(std::chrono::system_clock::now()
                                  .time_since_epoch()
                                  .count()) +
               ".dat";
    }
};

// Basic compression test
TEST_F(MatrixCompressorTest, BasicCompression) {
    // Create a simple matrix with repeating pattern
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'A', 'B', 'B'},
                                       {'B', 'C', 'C', 'C', 'C'},
                                       {'C', 'A', 'A', 'A', 'A'}};

    // Expected compressed format: (A,3), (B,2), (B,1), (C,4), (C,1), (A,4)
    MatrixCompressor::CompressedData expected = {
        {'A', 3}, {'B', 3}, {'C', 5}, {'A', 4}};

    auto compressed = MatrixCompressor::compress(matrix);

    ASSERT_EQ(expected.size(), compressed.size())
        << "Compressed data has unexpected size";
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i].first, compressed[i].first)
            << "Character mismatch at index " << i;
        EXPECT_EQ(expected[i].second, compressed[i].second)
            << "Count mismatch at index " << i;
    }
}

// Basic decompression test
TEST_F(MatrixCompressorTest, BasicDecompression) {
    // Create compressed data
    MatrixCompressor::CompressedData compressed = {
        {'A', 3}, {'B', 3}, {'C', 5}, {'A', 4}};

    // Expected matrix after decompression
    MatrixCompressor::Matrix expected = {{'A', 'A', 'A', 'B', 'B'},
                                         {'B', 'C', 'C', 'C', 'C'},
                                         {'C', 'A', 'A', 'A', 'A'}};

    int rows = 3;
    int cols = 5;

    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);

    expectMatricesEqual(expected, decompressed);
}

// Test with empty matrix
TEST_F(MatrixCompressorTest, EmptyMatrix) {
    MatrixCompressor::Matrix emptyMatrix;

    auto compressed = MatrixCompressor::compress(emptyMatrix);

    EXPECT_TRUE(compressed.empty())
        << "Compressed result of empty matrix should be empty";

    // Decompressing an empty compressed result should give an empty matrix
    auto decompressed = MatrixCompressor::decompress(compressed, 0, 0);

    EXPECT_TRUE(decompressed.empty()) << "Decompressed result should be empty";
}

// Test with single-element matrix
TEST_F(MatrixCompressorTest, SingleElementMatrix) {
    MatrixCompressor::Matrix matrix = {{'X'}};

    auto compressed = MatrixCompressor::compress(matrix);

    ASSERT_EQ(1, compressed.size())
        << "Compressed data should have one element";
    EXPECT_EQ('X', compressed[0].first) << "Compressed character should be 'X'";
    EXPECT_EQ(1, compressed[0].second) << "Compressed count should be 1";

    auto decompressed = MatrixCompressor::decompress(compressed, 1, 1);

    expectMatricesEqual(matrix, decompressed);
}

// Test with homogeneous matrix (all same character)
TEST_F(MatrixCompressorTest, HomogeneousMatrix) {
    int rows = 5;
    int cols = 5;
    char value = 'Z';

    MatrixCompressor::Matrix matrix(rows, std::vector<char>(cols, value));

    auto compressed = MatrixCompressor::compress(matrix);

    ASSERT_EQ(1, compressed.size())
        << "Homogeneous matrix should compress to one element";
    EXPECT_EQ(value, compressed[0].first);
    EXPECT_EQ(rows * cols, compressed[0].second);

    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);

    expectMatricesEqual(matrix, decompressed);
}

// Test with large matrix to verify performance and correctness
TEST_F(MatrixCompressorTest, LargeMatrix) {
    int rows = 100;
    int cols = 100;

    auto matrix = createRunLengthMatrix(rows, cols);

    auto compressed = MatrixCompressor::compress(matrix);

    // A run-length matrix should compress well
    EXPECT_LT(compressed.size(), static_cast<size_t>(rows * cols / 5))
        << "Large matrix should compress to significantly fewer elements";

    auto decompressed = MatrixCompressor::decompress(compressed, rows, cols);

    expectMatricesEqual(matrix, decompressed);
}

// Test parallel compression
TEST_F(MatrixCompressorTest, ParallelCompression) {
    int rows = 200;
    int cols = 200;

    auto matrix = createRunLengthMatrix(rows, cols);

    // Test with explicitly set thread count
    auto compressedParallel = MatrixCompressor::compressParallel(matrix, 4);
    // Compare with sequential compression
    auto compressedSequential = MatrixCompressor::compress(matrix);

    // Verify that both compressions produce the same result
    ASSERT_EQ(compressedSequential.size(), compressedParallel.size())
        << "Parallel compression should produce same-sized result as "
           "sequential";

    for (size_t i = 0; i < compressedSequential.size(); ++i) {
        EXPECT_EQ(compressedSequential[i].first, compressedParallel[i].first);
        EXPECT_EQ(compressedSequential[i].second, compressedParallel[i].second);
    }

    // Ensure decompression works with parallel-compressed data
    auto decompressed =
        MatrixCompressor::decompress(compressedParallel, rows, cols);
    expectMatricesEqual(matrix, decompressed);
}

// Test parallel decompression
TEST_F(MatrixCompressorTest, ParallelDecompression) {
    int rows = 200;
    int cols = 200;

    auto matrix = createRunLengthMatrix(rows, cols);
    auto compressed = MatrixCompressor::compress(matrix);

    // Test parallel decompression with explicitly set thread count
    auto decompressedParallel =
        MatrixCompressor::decompressParallel(compressed, rows, cols, 4);
    // Compare with sequential decompression
    auto decompressedSequential =
        MatrixCompressor::decompress(compressed, rows, cols);

    // Verify that both decompressions produce the same result
    expectMatricesEqual(decompressedSequential, decompressedParallel);
    expectMatricesEqual(matrix, decompressedParallel);
}

// Test error handling in compression
TEST_F(MatrixCompressorTest, CompressionErrorHandling) {
    // This test should verify that compress handles errors properly
    // Since the code uses C++ exceptions, we can test that they are thrown when
    // expected

    // Create an invalid matrix (jagged array)
    MatrixCompressor::Matrix invalidMatrix;
    invalidMatrix.push_back(std::vector<char>{'A', 'B'});
    invalidMatrix.push_back(std::vector<char>{});  // Empty row

    // Test handling of jagged arrays in STL containers is
    // implementation-defined So we're not testing a specific exception, just
    // that the function doesn't crash
    MatrixCompressor::CompressedData result;
    EXPECT_NO_THROW(result = MatrixCompressor::compress(invalidMatrix));
}

// Test error handling in decompression
TEST_F(MatrixCompressorTest, DecompressionErrorHandling) {
    // Test invalid dimensions
    MatrixCompressor::CompressedData compressed = {{'A', 10}};

    EXPECT_THROW(MatrixCompressor::decompress(compressed, 0, 5),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 5, 0),
                 MatrixDecompressException);
    EXPECT_THROW(MatrixCompressor::decompress(compressed, -1, 5),
                 MatrixDecompressException);

    // Test element count mismatch
    EXPECT_THROW(MatrixCompressor::decompress(compressed, 2, 3),
                 MatrixDecompressException);
}

// Test file I/O for compressed data
TEST_F(MatrixCompressorTest, FileIO) {
    MatrixCompressor::CompressedData compressed = {
        {'A', 100}, {'B', 50}, {'C', 25}};

    std::string filePath = getTempFilePath();

    // Save compressed data to file
    EXPECT_NO_THROW(
        MatrixCompressor::saveCompressedToFile(compressed, filePath));

    // Load compressed data from file
    MatrixCompressor::CompressedData loaded;
    EXPECT_NO_THROW(loaded =
                        MatrixCompressor::loadCompressedFromFile(filePath));

    // Verify loaded data matches original
    ASSERT_EQ(compressed.size(), loaded.size());
    for (size_t i = 0; i < compressed.size(); ++i) {
        EXPECT_EQ(compressed[i].first, loaded[i].first);
        EXPECT_EQ(compressed[i].second, loaded[i].second);
    }

    // Test error handling for non-existent file
    EXPECT_THROW(MatrixCompressor::loadCompressedFromFile("/non/existent/file"),
                 atom::error::FailToOpenFile);
}

// Test compression ratio calculation
TEST_F(MatrixCompressorTest, CompressionRatio) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'A', 'A', 'A'},
                                       {'A', 'A', 'A', 'A', 'A'}};

    MatrixCompressor::CompressedData compressed = {{'A', 10}};

    double ratio =
        MatrixCompressor::calculateCompressionRatio(matrix, compressed);

    // Expected ratio: compressed size (1 pair of char+int) / original size (10
    // chars)
    size_t compressedSize = 1 * (sizeof(char) + sizeof(int));
    size_t originalSize = 10 * sizeof(char);
    double expectedRatio =
        static_cast<double>(compressedSize) / static_cast<double>(originalSize);

    EXPECT_DOUBLE_EQ(expectedRatio, ratio);
}

// Test random matrix generation
TEST_F(MatrixCompressorTest, RandomMatrixGeneration) {
    int rows = 10;
    int cols = 10;
    std::string charset = "ABC";

    auto matrix = MatrixCompressor::generateRandomMatrix(rows, cols, charset);

    ASSERT_EQ(rows, static_cast<int>(matrix.size()));
    ASSERT_EQ(cols, static_cast<int>(matrix[0].size()));

    // Verify all characters in the matrix are from the charset
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            EXPECT_NE(charset.find(matrix[i][j]), std::string::npos)
                << "Character at (" << i << ", " << j
                << ") not found in charset";
        }
    }
}

// Test downsampling
TEST_F(MatrixCompressorTest, Downsampling) {
    MatrixCompressor::Matrix matrix = {{'A', 'A', 'B', 'B'},
                                       {'A', 'A', 'B', 'B'},
                                       {'C', 'C', 'D', 'D'},
                                       {'C', 'C', 'D', 'D'}};

    // Downsample by factor of 2
    auto downsampled = MatrixCompressor::downsample(matrix, 2);

    // Expected result: 2x2 matrix with average values
    MatrixCompressor::Matrix expected = {{'A', 'B'}, {'C', 'D'}};

    ASSERT_EQ(2, static_cast<int>(downsampled.size()));
    ASSERT_EQ(2, static_cast<int>(downsampled[0].size()));

    expectMatricesEqual(expected, downsampled);
}

// Test upsampling
TEST_F(MatrixCompressorTest, Upsampling) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};

    // Upsample by factor of 2
    auto upsampled = MatrixCompressor::upsample(matrix, 2);

    // Expected result: 4x4 matrix with repeated values
    MatrixCompressor::Matrix expected = {{'A', 'A', 'B', 'B'},
                                         {'A', 'A', 'B', 'B'},
                                         {'C', 'C', 'D', 'D'},
                                         {'C', 'C', 'D', 'D'}};

    ASSERT_EQ(4, static_cast<int>(upsampled.size()));
    ASSERT_EQ(4, static_cast<int>(upsampled[0].size()));

    expectMatricesEqual(expected, upsampled);
}

// Test invalid factor for downsampling
TEST_F(MatrixCompressorTest, InvalidDownsamplingFactor) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};

    EXPECT_THROW(MatrixCompressor::downsample(matrix, 0),
                 atom::error::InvalidArgument);
    EXPECT_THROW(MatrixCompressor::downsample(matrix, -1),
                 atom::error::InvalidArgument);
}

// Test invalid factor for upsampling
TEST_F(MatrixCompressorTest, InvalidUpsamplingFactor) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};

    EXPECT_THROW(MatrixCompressor::upsample(matrix, 0),
                 atom::error::InvalidArgument);
    EXPECT_THROW(MatrixCompressor::upsample(matrix, -1),
                 atom::error::InvalidArgument);
}

// Test MSE calculation
TEST_F(MatrixCompressorTest, MSECalculation) {
    MatrixCompressor::Matrix matrix1 = {{'A', 'B'}, {'C', 'D'}};

    MatrixCompressor::Matrix matrix2 = {{'A', 'C'}, {'B', 'D'}};

    double mse = MatrixCompressor::calculateMSE(matrix1, matrix2);

    // Expected MSE: (0^2 + (B-C)^2 + (C-B)^2 + 0^2) / 4
    double expectedMSE =
        (std::pow('B' - 'C', 2) + std::pow('C' - 'B', 2)) / 4.0;

    EXPECT_DOUBLE_EQ(expectedMSE, mse);
}

// Test MSE calculation with different sized matrices
TEST_F(MatrixCompressorTest, MSEWithDifferentSizedMatrices) {
    MatrixCompressor::Matrix matrix1 = {{'A', 'B'}, {'C', 'D'}};

    MatrixCompressor::Matrix matrix2 = {{'A', 'B', 'C'}, {'D', 'E', 'F'}};

    EXPECT_THROW(MatrixCompressor::calculateMSE(matrix1, matrix2),
                 atom::error::InvalidArgument);
}

// Test with unicode characters
TEST_F(MatrixCompressorTest, UnicodeCharacters) {
    // Create a matrix with some Unicode characters
    MatrixCompressor::Matrix matrix = {
        {static_cast<char>(0xC3), static_cast<char>(0xA4)},  // ä
        {static_cast<char>(0xC3), static_cast<char>(0xB6)}   // ö
    };

    auto compressed = MatrixCompressor::compress(matrix);
    auto decompressed = MatrixCompressor::decompress(compressed, 2, 2);

    expectMatricesEqual(matrix, decompressed);
}

// Test matrix printing (this is more of a coverage test since it writes to
// stdout)
TEST_F(MatrixCompressorTest, MatrixPrinting) {
    MatrixCompressor::Matrix matrix = {{'A', 'B'}, {'C', 'D'}};

    // No assertion, just check it doesn't crash
    testing::internal::CaptureStdout();
    MatrixCompressor::printMatrix(matrix);
    testing::internal::GetCapturedStdout();
}
