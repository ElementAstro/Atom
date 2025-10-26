/**
 * @file advanced_compression.cpp
 * @brief Advanced demonstration of compression operations and options
 *
 * This example demonstrates:
 * - Advanced compression options and levels
 * - Streaming compression for large files
 * - Compression ratio analysis
 * - Memory usage optimization
 * - Custom compression parameters
 * - Performance benchmarking
 * - Error handling and recovery
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
#include "atom/io/compression/compress.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test files with different characteristics for compression
 * testing
 */
void createTestFiles() {
    std::cout
        << "Creating test files with different compression characteristics..."
        << std::endl;

    // 1. Highly compressible text file
    {
        std::ofstream file("highly_compressible.txt");
        std::string pattern =
            "This is a repeating pattern for compression testing. ";
        for (int i = 0; i < 1000; ++i) {
            file << pattern << "Line " << i << std::endl;
        }
        file.close();
    }

    // 2. Moderately compressible file (mixed content)
    {
        std::ofstream file("moderately_compressible.txt");
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        // Mix of text and random data
        for (int i = 0; i < 500; ++i) {
            file << "Text line " << i << " with some structure." << std::endl;

            // Add some random bytes
            for (int j = 0; j < 50; ++j) {
                file << static_cast<char>(dis(gen));
            }
            file << std::endl;
        }
        file.close();
    }

    // 3. Poorly compressible file (random data)
    {
        std::ofstream file("poorly_compressible.bin", std::ios::binary);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        for (int i = 0; i < 100000; ++i) {
            char byte = static_cast<char>(dis(gen));
            file.write(&byte, 1);
        }
        file.close();
    }

    // 4. Large structured file
    {
        std::ofstream file("large_structured.csv");
        file << "id,name,email,age,city,country,salary" << std::endl;

        std::vector<std::string> names = {"John",  "Jane",    "Bob",
                                          "Alice", "Charlie", "Diana"};
        std::vector<std::string> cities = {"New York", "London", "Tokyo",
                                           "Paris", "Sydney"};
        std::vector<std::string> countries = {"USA", "UK", "Japan", "France",
                                              "Australia"};

        for (int i = 0; i < 10000; ++i) {
            file << i << "," << names[i % names.size()] << i << ","
                 << names[i % names.size()] << i << "@example.com,"
                 << (20 + (i % 50)) << "," << cities[i % cities.size()] << ","
                 << countries[i % countries.size()] << ","
                 << (30000 + (i % 70000)) << std::endl;
        }
        file.close();
    }

    std::cout
        << "✅ Created 4 test files with different compression characteristics"
        << std::endl;
}

/**
 * @brief Demonstrates compression with different levels and options
 */
void demonstrateCompressionLevels() {
    std::cout << "\n=== Compression Levels and Options ===" << std::endl;

    const std::string testFile = "highly_compressible.txt";
    auto originalSize = fs::file_size(testFile);

    std::cout << "Original file size: " << originalSize << " bytes"
              << std::endl;
    std::cout << "\nTesting different compression levels:" << std::endl;

    // Test different compression levels
    std::vector<int> levels = {1, 3, 6, 9};

    for (int level : levels) {
        atom::io::CompressionOptions options;
        options.level = level;
        options.chunk_size = 8192;  // 8KB chunks

        auto start = std::chrono::high_resolution_clock::now();
        auto result = atom::io::compressFile(testFile, ".", options);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result.success) {
            std::cout << "  Level " << level << ": " << result.compressed_size
                      << " bytes "
                      << "(" << (result.compression_ratio * 100) << "% ratio) "
                      << "in " << duration.count() << "ms" << std::endl;
        } else {
            std::cout << "  Level " << level << ": Failed - "
                      << result.error_message << std::endl;
        }

        // Clean up compressed file
        std::string compressedFile = testFile + ".gz";
        if (fs::exists(compressedFile)) {
            fs::remove(compressedFile);
        }
    }
}

/**
 * @brief Demonstrates compression of different file types
 */
void demonstrateFileTypeCompression() {
    std::cout << "\n=== Compression Analysis by File Type ===" << std::endl;

    std::vector<std::string> testFiles = {
        "highly_compressible.txt", "moderately_compressible.txt",
        "poorly_compressible.bin", "large_structured.csv"};

    atom::io::CompressionOptions options;
    options.level = 6;  // Balanced compression

    std::cout << "File Type Analysis (Compression Level 6):" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    for (const auto& file : testFiles) {
        if (!fs::exists(file))
            continue;

        auto originalSize = fs::file_size(file);

        auto start = std::chrono::high_resolution_clock::now();
        auto result = atom::io::compressFile(file, ".", options);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result.success) {
            double ratio = (double)result.compressed_size / originalSize;
            double savings = (1.0 - ratio) * 100.0;

            std::cout << "📄 " << file << std::endl;
            std::cout << "  Original: " << originalSize << " bytes"
                      << std::endl;
            std::cout << "  Compressed: " << result.compressed_size << " bytes"
                      << std::endl;
            std::cout << "  Ratio: " << (ratio * 100) << "%" << std::endl;
            std::cout << "  Savings: " << savings << "%" << std::endl;
            std::cout << "  Time: " << duration.count() << "ms" << std::endl;
            std::cout << std::endl;
        } else {
            std::cout << "❌ " << file << ": " << result.error_message
                      << std::endl;
        }

        // Clean up
        std::string compressedFile = file + ".gz";
        if (fs::exists(compressedFile)) {
            fs::remove(compressedFile);
        }
    }
}

/**
 * @brief Demonstrates folder compression with different options
 */
void demonstrateFolderCompression() {
    std::cout << "\n=== Folder Compression ===" << std::endl;

    // Create a test folder structure
    fs::create_directories("test_folder/subdir1");
    fs::create_directories("test_folder/subdir2");

    // Copy test files to the folder
    fs::copy_file("highly_compressible.txt", "test_folder/file1.txt");
    fs::copy_file("moderately_compressible.txt",
                  "test_folder/subdir1/file2.txt");
    fs::copy_file("large_structured.csv", "test_folder/subdir2/data.csv");

    // Add a small file
    std::ofstream smallFile("test_folder/small.txt");
    smallFile << "Small file content for testing.";
    smallFile.close();

    std::cout << "Created test folder structure" << std::endl;

    // Calculate total size
    size_t totalSize = 0;
    for (const auto& entry : fs::recursive_directory_iterator("test_folder")) {
        if (entry.is_regular_file()) {
            totalSize += entry.file_size();
        }
    }

    std::cout << "Total folder size: " << totalSize << " bytes" << std::endl;

    // Compress folder
    atom::io::CompressionOptions options;
    options.level = 6;
    options.chunk_size = 16384;  // 16KB chunks for better performance

    auto start = std::chrono::high_resolution_clock::now();
    auto result =
        atom::io::compressFolder("test_folder", "test_folder.zip", options);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result.success) {
        auto zipSize = fs::file_size("test_folder.zip");
        double ratio = (double)zipSize / totalSize;

        std::cout << "✅ Folder compressed successfully" << std::endl;
        std::cout << "  Original size: " << totalSize << " bytes" << std::endl;
        std::cout << "  ZIP size: " << zipSize << " bytes" << std::endl;
        std::cout << "  Compression ratio: " << (ratio * 100) << "%"
                  << std::endl;
        std::cout << "  Time: " << duration.count() << "ms" << std::endl;

        // List ZIP contents
        auto zipContents = atom::io::listZipContents("test_folder.zip");
        std::cout << "  Files in ZIP: " << zipContents.size() << std::endl;

    } else {
        std::cout << "❌ Folder compression failed: " << result.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates decompression and verification
 */
void demonstrateDecompression() {
    std::cout << "\n=== Decompression and Verification ===" << std::endl;

    if (!fs::exists("test_folder.zip")) {
        std::cout << "❌ No ZIP file found for decompression test" << std::endl;
        return;
    }

    // Extract to a new directory
    std::cout << "Extracting ZIP file..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = atom::io::extractZip("test_folder.zip", "extracted_folder");
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result.success) {
        std::cout << "✅ ZIP extracted successfully in " << duration.count()
                  << "ms" << std::endl;

        // Verify extracted contents
        std::cout << "Verifying extracted contents..." << std::endl;

        std::vector<std::string> expectedFiles = {
            "extracted_folder/file1.txt", "extracted_folder/subdir1/file2.txt",
            "extracted_folder/subdir2/data.csv", "extracted_folder/small.txt"};

        bool allFilesPresent = true;
        for (const auto& file : expectedFiles) {
            if (fs::exists(file)) {
                auto size = fs::file_size(file);
                std::cout << "  ✅ " << file << " (" << size << " bytes)"
                          << std::endl;
            } else {
                std::cout << "  ❌ Missing: " << file << std::endl;
                allFilesPresent = false;
            }
        }

        if (allFilesPresent) {
            std::cout << "🎉 All files extracted successfully!" << std::endl;
        }

    } else {
        std::cout << "❌ ZIP extraction failed: " << result.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates memory usage optimization
 */
void demonstrateMemoryOptimization() {
    std::cout << "\n=== Memory Usage Optimization ===" << std::endl;

    const std::string testFile = "large_structured.csv";

    // Test different chunk sizes
    std::vector<size_t> chunkSizes = {1024, 4096, 16384, 65536};  // 1KB to 64KB

    std::cout << "Testing different chunk sizes for memory optimization:"
              << std::endl;

    for (size_t chunkSize : chunkSizes) {
        atom::io::CompressionOptions options;
        options.level = 6;
        options.chunk_size = chunkSize;

        auto start = std::chrono::high_resolution_clock::now();
        auto result = atom::io::compressFile(testFile, ".", options);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result.success) {
            std::cout << "  Chunk size " << (chunkSize / 1024)
                      << "KB: " << duration.count() << "ms, "
                      << "ratio " << (result.compression_ratio * 100) << "%"
                      << std::endl;
        }

        // Clean up
        std::string compressedFile = testFile + ".gz";
        if (fs::exists(compressedFile)) {
            fs::remove(compressedFile);
        }
    }
}

/**
 * @brief Cleans up all test files and directories
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files and directories..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "highly_compressible.txt", "moderately_compressible.txt",
        "poorly_compressible.bin", "large_structured.csv", "test_folder.zip"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    // Remove directories
    if (fs::exists("test_folder")) {
        fs::remove_all("test_folder");
    }
    if (fs::exists("extracted_folder")) {
        fs::remove_all("extracted_folder");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🗜️  Atom I/O Advanced Compression Examples" << std::endl;
        std::cout << "===========================================" << std::endl;

        // Setup
        createTestFiles();

        // Run demonstrations
        demonstrateCompressionLevels();
        demonstrateFileTypeCompression();
        demonstrateFolderCompression();
        demonstrateDecompression();
        demonstrateMemoryOptimization();

        // Cleanup
        cleanup();

        std::cout << "\n🎉 All advanced compression operations completed "
                     "successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
