#include "atom/algorithm/matrix_compress.hpp"

#include <iostream>
#include <string>

int main() {
    // Example usage of generateRandomMatrix
    {
        int rows = 5;
        int cols = 5;
        std::string charset = "ABCD";
        auto matrix = atom::algorithm::MatrixCompressor::generateRandomMatrix(
            rows, cols, charset);

        std::cout << "Generated Random Matrix:" << std::endl;
        atom::algorithm::MatrixCompressor::printMatrix(matrix);
    }

    // Example usage of compress and decompress
    {
        atom::algorithm::MatrixCompressor::Matrix matrix = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        std::cout << "\nOriginal Matrix:" << std::endl;
        atom::algorithm::MatrixCompressor::printMatrix(matrix);

        auto compressed = atom::algorithm::MatrixCompressor::compress(matrix);
        std::cout << "\nCompressed Data:" << std::endl;
        for (const auto& [ch, count] : compressed) {
            std::cout << "(" << ch << ", " << count << ") ";
        }
        std::cout << std::endl;

        auto decompressed =
            atom::algorithm::MatrixCompressor::decompress(compressed, 5, 5);
        std::cout << "\nDecompressed Matrix:" << std::endl;
        atom::algorithm::MatrixCompressor::printMatrix(decompressed);
    }

    // Example usage of saveCompressedToFile and loadCompressedFromFile
    {
        atom::algorithm::MatrixCompressor::Matrix matrix = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        auto compressed = atom::algorithm::MatrixCompressor::compress(matrix);
        std::string filename = "compressed_matrix.dat";
        atom::algorithm::MatrixCompressor::saveCompressedToFile(compressed,
                                                                filename);

        auto loadedCompressed =
            atom::algorithm::MatrixCompressor::loadCompressedFromFile(filename);
        std::cout << "\nLoaded Compressed Data:" << std::endl;
        for (const auto& [ch, count] : loadedCompressed) {
            std::cout << "(" << ch << ", " << count << ") ";
        }
        std::cout << std::endl;
    }

    // Example usage of calculateCompressionRatio
    {
        atom::algorithm::MatrixCompressor::Matrix matrix = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        auto compressed = atom::algorithm::MatrixCompressor::compress(matrix);
        double compressionRatio =
            atom::algorithm::MatrixCompressor::calculateCompressionRatio(
                matrix, compressed);
        std::cout << "\nCompression Ratio: " << compressionRatio << std::endl;
    }

    // Example usage of downsample and upsample
    {
        atom::algorithm::MatrixCompressor::Matrix matrix = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        int factor = 2;
        auto downsampled =
            atom::algorithm::MatrixCompressor::downsample(matrix, factor);
        std::cout << "\nDownsampled Matrix:" << std::endl;
        atom::algorithm::MatrixCompressor::printMatrix(downsampled);

        auto upsampled =
            atom::algorithm::MatrixCompressor::upsample(downsampled, factor);
        std::cout << "\nUpsampled Matrix:" << std::endl;
        atom::algorithm::MatrixCompressor::printMatrix(upsampled);
    }

    // Example usage of calculateMSE
    {
        atom::algorithm::MatrixCompressor::Matrix matrix1 = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        atom::algorithm::MatrixCompressor::Matrix matrix2 = {
            {'A', 'A', 'B', 'B', 'C'},
            {'A', 'A', 'B', 'B', 'C'},
            {'C', 'C', 'D', 'D', 'D'},
            {'C', 'C', 'D', 'D', 'D'},
            {'A', 'A', 'B', 'B', 'C'}};

        double mse =
            atom::algorithm::MatrixCompressor::calculateMSE(matrix1, matrix2);
        std::cout << "\nMean Squared Error (MSE): " << mse << std::endl;
    }

#if ATOM_ENABLE_DEBUG
    // Example usage of performanceTest
    {
        int rows = 1000;
        int cols = 1000;
        atom::algorithm::performanceTest(rows, cols);
    }
#endif

    return 0;
}