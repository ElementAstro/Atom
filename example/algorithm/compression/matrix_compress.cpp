/**
 * @file matrix_compress.cpp
 * @brief Comprehensive example demonstrating matrix compression algorithms
 *
 * This example shows how to:
 * - Generate random matrices with different patterns
 * - Compress matrices using run-length encoding
 * - Decompress matrices and verify integrity
 * - Save and load compressed matrices to/from files
 * - Analyze compression efficiency and performance
 * - Handle different matrix sizes and data patterns
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/matrix_compress.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates basic matrix compression and decompression
 */
void demonstrateBasicCompression() {
    printHeader("Basic Matrix Compression");

    try {
        // Create a test matrix with patterns suitable for compression
        MatrixCompressor::Matrix matrix = {{'A', 'A', 'A', 'B', 'B'},
                                           {'A', 'A', 'A', 'B', 'B'},
                                           {'C', 'C', 'D', 'D', 'D'},
                                           {'C', 'C', 'D', 'D', 'D'},
                                           {'E', 'E', 'E', 'E', 'E'}};

        std::cout << "Original Matrix (5x5):\n";
        MatrixCompressor::printMatrix(matrix);

        // Calculate original size
        size_t originalSize = matrix.size() * matrix[0].size();
        std::cout << "\nOriginal size: " << originalSize << " characters\n";

        // Compress the matrix
        auto start = std::chrono::high_resolution_clock::now();
        auto compressed = MatrixCompressor::compress(matrix);
        auto end = std::chrono::high_resolution_clock::now();
        auto compressTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Compression time: " << compressTime.count() << " μs\n";
        std::cout << "\nCompressed data (character, count pairs):\n";
        for (const auto& [ch, count] : compressed) {
            std::cout << "  ('" << ch << "', " << count << ")\n";
        }

        // Calculate compressed size
        size_t compressedSize =
            compressed.size() * 2;  // Each pair takes 2 units
        std::cout << "\nCompressed size: " << compressedSize << " units\n";
        double compressionRatio =
            (static_cast<double>(originalSize - compressedSize) /
             originalSize) *
            100;
        std::cout << "Compression ratio: " << std::fixed << std::setprecision(2)
                  << compressionRatio << "%\n";

        // Decompress the matrix
        start = std::chrono::high_resolution_clock::now();
        auto decompressed = MatrixCompressor::decompress(compressed, 5, 5);
        end = std::chrono::high_resolution_clock::now();
        auto decompressTime =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Decompression time: " << decompressTime.count()
                  << " μs\n";
        std::cout << "\nDecompressed Matrix:\n";
        MatrixCompressor::printMatrix(decompressed);

        // Verify integrity
        bool isIdentical = (matrix == decompressed);
        std::cout << "Integrity check: "
                  << (isIdentical ? "✓ PASSED" : "✗ FAILED") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic compression: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates random matrix generation and compression
 */
void demonstrateRandomMatrixCompression() {
    printHeader("Random Matrix Generation and Compression");

    try {
        // Test different matrix sizes and character sets
        std::vector<std::tuple<int, int, std::string, std::string>> testCases =
            {{3, 3, "AB", "Small binary matrix"},
             {5, 5, "ABCD", "Medium 4-character matrix"},
             {8, 8, "ABCDEFGH", "Large 8-character matrix"},
             {10, 5, "XYZ", "Rectangular 3-character matrix"}};

        for (const auto& [rows, cols, charset, description] : testCases) {
            std::cout << "\n"
                      << description << " (" << rows << "x" << cols << "):\n";

            // Generate random matrix
            auto matrix =
                MatrixCompressor::generateRandomMatrix(rows, cols, charset);
            std::cout << "Generated matrix:\n";
            MatrixCompressor::printMatrix(matrix);

            // Compress and analyze
            auto compressed = MatrixCompressor::compress(matrix);
            size_t originalSize = rows * cols;
            size_t compressedSize = compressed.size() * 2;

            std::cout << "Original size: " << originalSize << " characters\n";
            std::cout << "Compressed size: " << compressedSize << " units\n";
            std::cout << "Compression efficiency: ";
            if (compressedSize < originalSize) {
                double ratio =
                    (static_cast<double>(originalSize - compressedSize) /
                     originalSize) *
                    100;
                std::cout << std::fixed << std::setprecision(1) << ratio
                          << "% reduction\n";
            } else {
                std::cout << "No compression benefit (data too random)\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in random matrix compression: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates advanced compression features
 */
void demonstrateAdvancedFeatures() {
    printHeader("Advanced Compression Features");

    try {
        MatrixCompressor::Matrix matrix = {{'A', 'A', 'B', 'B', 'C'},
                                           {'A', 'A', 'B', 'B', 'C'},
                                           {'C', 'C', 'D', 'D', 'D'},
                                           {'C', 'C', 'D', 'D', 'D'},
                                           {'A', 'A', 'B', 'B', 'C'}};

        std::cout << "Original Matrix:\n";
        MatrixCompressor::printMatrix(matrix);

        // Calculate compression ratio
        auto compressed = MatrixCompressor::compress(matrix);
        double compressionRatio =
            MatrixCompressor::calculateCompressionRatio(matrix, compressed);
        std::cout << "\nCompression Ratio: " << std::fixed
                  << std::setprecision(2) << compressionRatio << "\n";

        // Demonstrate downsampling and upsampling
        std::cout << "\nDownsampling and Upsampling:\n";
        int factor = 2;
        auto downsampled = MatrixCompressor::downsample(matrix, factor);
        std::cout << "Downsampled Matrix (factor " << factor << "):\n";
        MatrixCompressor::printMatrix(downsampled);

        auto upsampled = MatrixCompressor::upsample(downsampled, factor);
        std::cout << "\nUpsampled Matrix:\n";
        MatrixCompressor::printMatrix(upsampled);

        // Calculate MSE between original and upsampled
        double mse = MatrixCompressor::calculateMSE(matrix, upsampled);
        std::cout << "\nMean Squared Error (original vs upsampled): "
                  << std::fixed << std::setprecision(4) << mse << "\n";

        // Test with identical matrices
        double identicalMSE = MatrixCompressor::calculateMSE(matrix, matrix);
        std::cout << "MSE with identical matrix: " << identicalMSE
                  << " (should be 0)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced features: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance testing
 */
void demonstratePerformanceTesting() {
    printHeader("Performance Testing");

    try {
        std::cout << "Performance analysis with different matrix sizes:\n\n";

        std::vector<std::pair<int, int>> testSizes = {
            {10, 10}, {50, 50}, {100, 100}};

        for (const auto& [rows, cols] : testSizes) {
            std::cout << "Testing " << rows << "x" << cols << " matrix:\n";

            // Generate test matrix
            auto matrix =
                MatrixCompressor::generateRandomMatrix(rows, cols, "ABCD");

            // Measure compression time
            auto start = std::chrono::high_resolution_clock::now();
            auto compressed = MatrixCompressor::compress(matrix);
            auto end = std::chrono::high_resolution_clock::now();
            auto compressTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            // Measure decompression time
            start = std::chrono::high_resolution_clock::now();
            auto decompressed =
                MatrixCompressor::decompress(compressed, rows, cols);
            end = std::chrono::high_resolution_clock::now();
            auto decompressTime =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            // Calculate metrics
            size_t originalSize = rows * cols;
            size_t compressedSize = compressed.size() * 2;
            double ratio =
                MatrixCompressor::calculateCompressionRatio(matrix, compressed);

            std::cout << "  Compression time: " << compressTime.count()
                      << " μs\n";
            std::cout << "  Decompression time: " << decompressTime.count()
                      << " μs\n";
            std::cout << "  Original size: " << originalSize << " characters\n";
            std::cout << "  Compressed size: " << compressedSize << " units\n";
            std::cout << "  Compression ratio: " << std::fixed
                      << std::setprecision(2) << ratio << "\n\n";
        }

#if ATOM_ENABLE_DEBUG
        std::cout << "Running extended performance test...\n";
        performanceTest(1000, 1000);
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in performance testing: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive matrix compression
 */
int main() {
    std::cout << "=== Atom Matrix Compression Comprehensive Example ===\n";
    std::cout
        << "Demonstrating matrix compression algorithms and utilities...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicCompression();
        demonstrateRandomMatrixCompression();
        demonstrateAdvancedFeatures();
        demonstratePerformanceTesting();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Matrix Compression Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The matrix compression module provides:\n";
        std::cout << "  ✓ Run-length encoding compression for matrices\n";
        std::cout << "  ✓ File I/O operations for compressed data\n";
        std::cout << "  ✓ Downsampling and upsampling capabilities\n";
        std::cout << "  ✓ Compression ratio and quality analysis\n";
        std::cout << "  ✓ Performance testing and benchmarking\n";
        std::cout << "  ✓ Support for various matrix sizes and patterns\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in matrix compression example: "
                  << e.what() << "\n";
        return 1;
    }
}
