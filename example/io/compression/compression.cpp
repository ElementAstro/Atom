/**
 * @file basic_compression.cpp
 * @brief Comprehensive demonstration of compression and archive operations
 *
 * This example demonstrates:
 * - File compression using GZip
 * - ZIP archive creation and manipulation
 * - Listing ZIP contents
 * - File existence checking in archives
 * - Archive size retrieval
 * - File removal from archives
 * - Error handling for compression operations
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include "atom/io/compression/compress.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates a sample text file for compression testing
 * @param fileName Name of the file to create
 * @param content Content to write to the file
 */
void createSampleFile(const std::string& fileName,
                      const std::string& content = "") {
    std::ofstream outFile(fileName);
    if (outFile) {
        if (content.empty()) {
            outFile << "This is a sample text file for compression testing.\n";
            outFile << "It contains multiple lines of text to demonstrate\n";
            outFile << "the compression capabilities of the atom::io module.\n";
            outFile << "Line 4: Lorem ipsum dolor sit amet, consectetur "
                       "adipiscing elit.\n";
            outFile << "Line 5: Sed do eiusmod tempor incididunt ut labore et "
                       "dolore magna aliqua.\n";
        } else {
            outFile << content;
        }
        outFile.close();
        std::cout << "✅ Created sample file: " << fileName << std::endl;
    } else {
        std::cerr << "❌ Failed to create file: " << fileName << std::endl;
    }
}

/**
 * @brief Demonstrates basic file compression using GZip
 */
void demonstrateFileCompression() {
    std::cout << "\n=== File Compression Demo ===" << std::endl;

    const std::string sampleFile = "compression_test.txt";
    const std::string outputFolder = ".";

    // Create a sample file
    createSampleFile(sampleFile);

    // Get original file size
    auto originalSize = fs::file_size(sampleFile);
    std::cout << "Original file size: " << originalSize << " bytes"
              << std::endl;

    // Compress the file
    std::cout << "Compressing file..." << std::endl;
    auto compressResult = atom::io::compressFile(sampleFile, outputFolder);

    if (compressResult.success) {
        std::cout << "✅ Successfully compressed " << sampleFile << std::endl;
        std::cout << "Compression ratio: "
                  << (compressResult.compression_ratio * 100) << "%"
                  << std::endl;
        std::cout << "Original size: " << compressResult.original_size
                  << " bytes" << std::endl;
        std::cout << "Compressed size: " << compressResult.compressed_size
                  << " bytes" << std::endl;
    } else {
        std::cerr << "❌ Failed to compress " << sampleFile << ": "
                  << compressResult.error_message << std::endl;
    }

    // Clean up
    fs::remove(sampleFile);
}

/**
 * @brief Demonstrates ZIP archive operations
 */
void demonstrateZipOperations() {
    std::cout << "\n=== ZIP Archive Operations Demo ===" << std::endl;

    const std::string zipDir = "zip_test_dir";
    const std::string zipFile = "test_archive.zip";
    const std::string testFile1 = "test1.txt";
    const std::string testFile2 = "test2.txt";

    // Create a dedicated directory with test files
    fs::create_directories(zipDir);
    createSampleFile(zipDir + "/" + testFile1,
                     "Content of test file 1\nMultiple lines here.");
    createSampleFile(zipDir + "/" + testFile2,
                     "Content of test file 2\nDifferent content here.");

    // Create ZIP archive from the dedicated directory (not ".")
    std::cout << "Creating ZIP archive..." << std::endl;
    auto zipResult = atom::io::createZip(zipDir, zipFile);
    if (zipResult.success) {
        std::cout << "✅ Successfully created ZIP file: " << zipFile
                  << std::endl;
    } else {
        std::cerr << "❌ Failed to create ZIP file: " << zipResult.error_message
                  << std::endl;
        return;
    }

    // List files in ZIP
    std::cout << "\nListing files in ZIP archive:" << std::endl;
    auto filesInZip = atom::io::listZipContents(zipFile);
    for (const auto& file : filesInZip) {
        std::cout << "  📄 " << file.name << " (" << file.size << " bytes)"
                  << std::endl;
    }

    // Check if specific files exist in ZIP
    std::cout << "\nChecking file existence in ZIP:" << std::endl;
    if (atom::io::fileExistsInZip(zipFile, testFile1)) {
        std::cout << "✅ " << testFile1 << " exists in " << zipFile
                  << std::endl;
    } else {
        std::cout << "❌ " << testFile1 << " does not exist in " << zipFile
                  << std::endl;
    }

    // Get ZIP size
    auto zipSizeOpt = atom::io::getZipSize(zipFile);
    if (zipSizeOpt.has_value()) {
        std::cout << "ZIP archive size: " << zipSizeOpt.value() << " bytes"
                  << std::endl;
    }

    // Remove file from ZIP
    std::cout << "\nRemoving " << testFile1 << " from ZIP..." << std::endl;
    auto removeResult = atom::io::removeFromZip(zipFile, testFile1);
    if (removeResult.success) {
        std::cout << "✅ Removed " << testFile1 << " from " << zipFile
                  << std::endl;
    } else {
        std::cerr << "❌ Failed to remove " << testFile1 << " from " << zipFile
                  << ": " << removeResult.error_message << std::endl;
    }

    // List files again to verify removal
    std::cout << "\nFiles in ZIP after removal:" << std::endl;
    filesInZip = atom::io::listZipContents(zipFile);
    for (const auto& file : filesInZip) {
        std::cout << "  📄 " << file.name << " (" << file.size << " bytes)"
                  << std::endl;
    }

    // Clean up
    fs::remove_all(zipDir);
    fs::remove(zipFile);
}

/**
 * @brief Demonstrates GZ file decompression
 */
void demonstrateGzDecompression() {
    std::cout << "\n=== GZ File Decompression ===" << std::endl;

    const std::string sourceFile = "decompress_test.txt";
    const std::string compressedFile = "decompress_test.txt.gz";
    const std::string outputDir = "decompressed_output";

    // Create and compress a test file
    std::cout << "1. Creating and compressing test file..." << std::endl;
    {
        std::ofstream file(sourceFile);
        for (int i = 0; i < 500; ++i) {
            file << "Decompression test line " << i
                 << " with repeating pattern data.\n";
        }
        file.close();
    }

    auto originalSize = fs::file_size(sourceFile);
    std::cout << "  Original file: " << originalSize << " bytes" << std::endl;

    auto compResult = atom::io::compressFile(sourceFile, ".");
    if (!compResult.success) {
        std::cerr << "  Compression failed: " << compResult.error_message
                  << std::endl;
        return;
    }
    std::cout << "  Compressed to: " << compressedFile << std::endl;

    // Decompress
    std::cout << "\n2. Decompressing file..." << std::endl;
    fs::create_directories(outputDir);

    atom::io::DecompressionOptions decompOptions;
    auto decompResult =
        atom::io::decompressFile(compressedFile, outputDir, decompOptions);

    if (decompResult.success) {
        std::cout << "  Decompression successful" << std::endl;

        // Check the decompressed file
        std::string decompressedPath = outputDir + "/decompress_test.txt";
        if (fs::exists(decompressedPath)) {
            auto decompSize = fs::file_size(decompressedPath);
            std::cout << "  Decompressed size: " << decompSize << " bytes"
                      << std::endl;
            std::cout << "  Matches original: "
                      << (decompSize == originalSize ? "YES" : "NO")
                      << std::endl;
        }
    } else {
        std::cerr << "  Decompression failed: " << decompResult.error_message
                  << std::endl;
    }

    // Cleanup
    fs::remove(sourceFile);
    fs::remove(compressedFile);
    fs::remove_all(outputDir);
}

int main() {
    try {
        std::cout << "🗜️  Atom I/O Compression Examples" << std::endl;
        std::cout << "=================================" << std::endl;

        demonstrateFileCompression();
        demonstrateZipOperations();
        demonstrateGzDecompression();

        std::cout << "\n🎉 All compression operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        return 1;
    }
}
