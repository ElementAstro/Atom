/**
 * @file data_compression.cpp
 * @brief Demonstration of generic data compression, slice compression, and
 *        decompression operations
 *
 * This example demonstrates:
 * - Generic data compression with compressData<T>
 * - Generic data decompression with decompressData<T>
 * - Slice-based file compression for large files
 * - Merging compressed slices back together
 * - Compression options tuning
 * - Error handling and result inspection
 */

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "atom/io/compression/compress.hpp"

namespace fs = std::filesystem;

/**
 * @brief Demonstrates generic data compression and decompression
 */
void demonstrateDataCompression() {
    std::cout << "\n=== Generic Data Compression ===" << std::endl;

    // Prepare test data
    std::string textData =
        "This is a repeating pattern for data compression testing. ";
    atom::io::Vector<unsigned char> inputData;
    for (int i = 0; i < 100; ++i) {
        inputData.insert(inputData.end(), textData.begin(), textData.end());
    }

    std::cout << "Original data size: " << inputData.size() << " bytes"
              << std::endl;

    // Compress with default options
    std::cout << "\n1. Compressing with default options..." << std::endl;
    atom::io::CompressionOptions options;
    auto [compResult, compressedData] =
        atom::io::compressData(inputData, options);

    if (compResult.success) {
        std::cout << "  Compressed size: " << compressedData.size() << " bytes"
                  << std::endl;
        std::cout << "  Compression ratio: "
                  << (compResult.compression_ratio * 100) << "%" << std::endl;
        std::cout << "  Original: " << compResult.original_size
                  << " Compressed: " << compResult.compressed_size << std::endl;
    } else {
        std::cerr << "  Compression failed: " << compResult.error_message
                  << std::endl;
        return;
    }

    // Decompress the data
    std::cout << "\n2. Decompressing data..." << std::endl;
    atom::io::DecompressionOptions decompOptions;
    auto [decompResult, decompressedData] =
        atom::io::decompressData(compressedData, inputData.size(), decompOptions);

    if (decompResult.success) {
        std::cout << "  Decompressed size: " << decompressedData.size()
                  << " bytes" << std::endl;

        // Verify integrity
        bool dataMatch = (decompressedData.size() == inputData.size()) &&
                         (std::memcmp(decompressedData.data(), inputData.data(),
                                      inputData.size()) == 0);
        std::cout << "  Data integrity: "
                  << (dataMatch ? "PASSED" : "FAILED") << std::endl;
    } else {
        std::cerr << "  Decompression failed: " << decompResult.error_message
                  << std::endl;
    }

    // Test different compression levels
    std::cout << "\n3. Comparing compression levels on data buffer..."
              << std::endl;
    std::vector<int> levels = {1, 4, 6, 9};
    for (int level : levels) {
        atom::io::CompressionOptions lvlOptions;
        lvlOptions.level = level;

        auto [result, data] = atom::io::compressData(inputData, lvlOptions);
        if (result.success) {
            double ratio =
                static_cast<double>(data.size()) / inputData.size() * 100.0;
            std::cout << "  Level " << level << ": " << data.size()
                      << " bytes (" << ratio << "%)" << std::endl;
        } else {
            std::cout << "  Level " << level << ": Failed - "
                      << result.error_message << std::endl;
        }
    }
}

/**
 * @brief Demonstrates binary data compression
 */
void demonstrateBinaryDataCompression() {
    std::cout << "\n=== Binary Data Compression ===" << std::endl;

    // Create binary data with patterns
    atom::io::Vector<unsigned char> binaryData(50000);
    for (size_t i = 0; i < binaryData.size(); ++i) {
        binaryData[i] = static_cast<unsigned char>(i % 256);
    }

    std::cout << "Binary data size: " << binaryData.size() << " bytes"
              << std::endl;

    auto [compResult, compressed] = atom::io::compressData(binaryData);
    if (compResult.success) {
        std::cout << "Compressed size: " << compressed.size() << " bytes"
                  << std::endl;
        std::cout << "Compression ratio: "
                  << (compResult.compression_ratio * 100) << "%" << std::endl;

        // Decompress and verify
        auto [decompResult, decompressed] =
            atom::io::decompressData(compressed, binaryData.size());

        if (decompResult.success) {
            bool match = (decompressed.size() == binaryData.size()) &&
                         (std::memcmp(decompressed.data(), binaryData.data(),
                                      binaryData.size()) == 0);
            std::cout << "Round-trip integrity: "
                      << (match ? "PASSED" : "FAILED") << std::endl;
        }
    } else {
        std::cerr << "Compression failed: " << compResult.error_message
                  << std::endl;
    }
}

/**
 * @brief Creates a large test file for slice compression
 */
void createLargeFile(const std::string& filename, size_t sizeKB) {
    std::ofstream file(filename, std::ios::binary);
    std::string pattern = "Slice compression test data pattern. ";
    size_t written = 0;
    size_t targetBytes = sizeKB * 1024;

    while (written < targetBytes) {
        size_t toWrite = std::min(pattern.size(), targetBytes - written);
        file.write(pattern.c_str(), toWrite);
        written += toWrite;
    }
    file.close();
}

/**
 * @brief Demonstrates slice-based file compression
 */
void demonstrateSliceCompression() {
    std::cout << "\n=== Slice-Based File Compression ===" << std::endl;

    const std::string testFile = "slice_test_file.txt";
    const size_t sliceSize = 16 * 1024;  // 16 KB slices

    // Create a large test file
    createLargeFile(testFile, 200);  // 200 KB
    auto originalSize = fs::file_size(testFile);
    std::cout << "Original file size: " << originalSize << " bytes"
              << std::endl;

    // Compress in slices
    std::cout << "\n1. Compressing file in " << (sliceSize / 1024)
              << " KB slices..." << std::endl;
    atom::io::CompressionOptions options;
    options.level = 6;

    auto result =
        atom::io::compressFileInSlices(testFile, sliceSize, options);

    if (result.success) {
        std::cout << "  Slice compression completed" << std::endl;
        std::cout << "  Original size: " << result.original_size << " bytes"
                  << std::endl;
        std::cout << "  Compressed size: " << result.compressed_size << " bytes"
                  << std::endl;
        std::cout << "  Ratio: " << (result.compression_ratio * 100) << "%"
                  << std::endl;
    } else {
        std::cerr << "  Slice compression failed: " << result.error_message
                  << std::endl;
    }

    // Demonstrate different slice sizes
    std::cout << "\n2. Comparing different slice sizes..." << std::endl;
    std::vector<size_t> sliceSizes = {4096, 16384, 65536};

    for (size_t ss : sliceSizes) {
        auto sliceResult =
            atom::io::compressFileInSlices(testFile, ss, options);

        if (sliceResult.success) {
            std::cout << "  Slice " << (ss / 1024)
                      << " KB: ratio " << (sliceResult.compression_ratio * 100)
                      << "%" << std::endl;
        } else {
            std::cout << "  Slice " << (ss / 1024)
                      << " KB: Failed - " << sliceResult.error_message
                      << std::endl;
        }
    }

    // Cleanup
    fs::remove(testFile);
}

/**
 * @brief Demonstrates error handling for data compression
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    // Compress empty data
    std::cout << "1. Compressing empty data..." << std::endl;
    atom::io::Vector<unsigned char> emptyData;
    auto [emptyResult, emptyCompressed] = atom::io::compressData(emptyData);
    std::cout << "  Result: "
              << (emptyResult.success ? "Success" : "Failed")
              << (emptyResult.error_message.empty()
                      ? ""
                      : " - " + std::string(emptyResult.error_message))
              << std::endl;

    // Decompress invalid data
    std::cout << "\n2. Decompressing invalid data..." << std::endl;
    atom::io::Vector<unsigned char> invalidData = {0x00, 0xFF, 0xAB, 0xCD};
    auto [invalidResult, invalidDecompressed] =
        atom::io::decompressData(invalidData);
    std::cout << "  Result: "
              << (invalidResult.success ? "Success" : "Failed")
              << (invalidResult.error_message.empty()
                      ? ""
                      : " - " + std::string(invalidResult.error_message))
              << std::endl;

    // Slice compression on non-existent file
    std::cout << "\n3. Slice compression on non-existent file..." << std::endl;
    auto sliceResult =
        atom::io::compressFileInSlices("non_existent_file.dat", 4096);
    std::cout << "  Result: "
              << (sliceResult.success ? "Success" : "Failed")
              << (sliceResult.error_message.empty()
                      ? ""
                      : " - " + std::string(sliceResult.error_message))
              << std::endl;
}

int main() {
    try {
        std::cout << "🔬 Atom I/O Data Compression Examples" << std::endl;
        std::cout << "======================================" << std::endl;

        demonstrateDataCompression();
        demonstrateBinaryDataCompression();
        demonstrateSliceCompression();
        demonstrateErrorHandling();

        std::cout
            << "\n🎉 All data compression operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        return 1;
    }
}
