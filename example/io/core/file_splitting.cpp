/**
 * @file file_splitting.cpp
 * @brief Demonstration of file splitting and merging operations
 *
 * This example demonstrates:
 * - File splitting into multiple chunks
 * - Calculating optimal chunk sizes
 * - Merging split files back together
 * - Quick split/merge operations
 * - File integrity verification
 * - Custom output patterns for split files
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "atom/io/core/io.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates a large test file for splitting demonstration
 */
void createLargeTestFile(const std::string& filename, size_t sizeKB = 100) {
    std::cout << "Creating test file: " << filename << " (" << sizeKB << " KB)"
              << std::endl;

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to create test file");
    }

    // Create content with repeating pattern for easy verification
    std::string pattern = "This is line number ";
    std::string lineEnd = " of the test file for splitting demonstration.\n";

    size_t bytesWritten = 0;
    size_t targetBytes = sizeKB * 1024;
    int lineNumber = 1;

    while (bytesWritten < targetBytes) {
        std::string line = pattern + std::to_string(lineNumber) + lineEnd;
        file.write(line.c_str(), line.length());
        bytesWritten += line.length();
        lineNumber++;
    }

    file.close();

    auto actualSize = atom::io::getFileSize(filename);
    std::cout << "✅ Created file with " << actualSize << " bytes" << std::endl;
}

/**
 * @brief Demonstrates basic file splitting operations
 */
void demonstrateBasicSplitting() {
    std::cout << "\n=== Basic File Splitting ===" << std::endl;

    const std::string testFile = "large_test_file.txt";
    const size_t chunkSize = 10 * 1024;  // 10 KB chunks

    // Create test file
    createLargeTestFile(testFile, 50);  // 50 KB file

    auto originalSize = atom::io::getFileSize(testFile);
    std::cout << "Original file size: " << originalSize << " bytes"
              << std::endl;

    // Split the file
    std::cout << "\nSplitting file into " << chunkSize << " byte chunks..."
              << std::endl;
    try {
        atom::io::splitFile(testFile, chunkSize, "split_part_");
        std::cout << "✅ File split successfully" << std::endl;

        // List the created parts
        std::cout << "\nCreated parts:" << std::endl;
        for (int i = 0;; ++i) {
            std::string partName = "split_part_" + std::to_string(i);
            if (atom::io::isFileExists(partName)) {
                auto partSize = atom::io::getFileSize(partName);
                std::cout << "  📄 " << partName << " (" << partSize
                          << " bytes)" << std::endl;
            } else {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ File splitting failed: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates file merging operations
 */
void demonstrateFileMerging() {
    std::cout << "\n=== File Merging ===" << std::endl;

    const std::string mergedFile = "merged_file.txt";

    // Collect part files
    std::vector<std::string> partFiles;
    for (int i = 0;; ++i) {
        std::string partName = "split_part_" + std::to_string(i);
        if (atom::io::isFileExists(partName)) {
            partFiles.push_back(partName);
        } else {
            break;
        }
    }

    if (partFiles.empty()) {
        std::cout << "❌ No part files found for merging" << std::endl;
        return;
    }

    std::cout << "Merging " << partFiles.size() << " parts into " << mergedFile
              << "..." << std::endl;

    try {
        atom::io::mergeFiles(mergedFile, partFiles);
        std::cout << "✅ Files merged successfully" << std::endl;

        auto mergedSize = atom::io::getFileSize(mergedFile);
        std::cout << "Merged file size: " << mergedSize << " bytes"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ File merging failed: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates quick split/merge operations
 */
void demonstrateQuickOperations() {
    std::cout << "\n=== Quick Split/Merge Operations ===" << std::endl;

    const std::string testFile = "quick_test_file.txt";
    const int numChunks = 5;

    // Create test file
    createLargeTestFile(testFile, 75);  // 75 KB file

    auto originalSize = atom::io::getFileSize(testFile);
    auto calculatedChunkSize =
        atom::io::calculateChunkSize(originalSize, numChunks);

    std::cout << "Original file size: " << originalSize << " bytes"
              << std::endl;
    std::cout << "Calculated chunk size for " << numChunks
              << " chunks: " << calculatedChunkSize << " bytes" << std::endl;

    // Quick split
    std::cout << "\nPerforming quick split into " << numChunks << " chunks..."
              << std::endl;
    try {
        atom::io::quickSplit(testFile, numChunks, "quick_part_");
        std::cout << "✅ Quick split completed" << std::endl;

        // List created parts
        std::cout << "\nCreated parts:" << std::endl;
        for (int i = 0; i < numChunks; ++i) {
            std::string partName = "quick_part_" + std::to_string(i);
            if (atom::io::isFileExists(partName)) {
                auto partSize = atom::io::getFileSize(partName);
                std::cout << "  📄 " << partName << " (" << partSize
                          << " bytes)" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Quick split failed: " << e.what() << std::endl;
        return;
    }

    // Quick merge
    std::cout << "\nPerforming quick merge..." << std::endl;
    const std::string quickMergedFile = "quick_merged_file.txt";

    try {
        atom::io::quickMerge(quickMergedFile, "quick_part_", numChunks);
        std::cout << "✅ Quick merge completed" << std::endl;

        auto quickMergedSize = atom::io::getFileSize(quickMergedFile);
        std::cout << "Quick merged file size: " << quickMergedSize << " bytes"
                  << std::endl;

        // Verify integrity
        if (quickMergedSize == originalSize) {
            std::cout << "✅ File integrity verified - sizes match!"
                      << std::endl;
        } else {
            std::cout << "❌ File integrity check failed - size mismatch!"
                      << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Quick merge failed: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates chunk size calculations
 */
void demonstrateChunkCalculations() {
    std::cout << "\n=== Chunk Size Calculations ===" << std::endl;

    std::vector<std::pair<size_t, int>> testCases = {
        {1024 * 1024, 4},      // 1 MB into 4 chunks
        {500 * 1024, 3},       // 500 KB into 3 chunks
        {2 * 1024 * 1024, 8},  // 2 MB into 8 chunks
        {100 * 1024, 10}       // 100 KB into 10 chunks
    };

    std::cout << "Chunk size calculations for various scenarios:" << std::endl;
    for (const auto& [fileSize, chunks] : testCases) {
        auto chunkSize = atom::io::calculateChunkSize(fileSize, chunks);
        std::cout << "  File: " << (fileSize / 1024)
                  << " KB, Chunks: " << chunks
                  << " -> Chunk size: " << chunkSize << " bytes ("
                  << (chunkSize / 1024.0) << " KB)" << std::endl;
    }
}

/**
 * @brief Cleans up all test files
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "large_test_file.txt", "merged_file.txt", "quick_test_file.txt",
        "quick_merged_file.txt"};

    // Remove main files
    for (const auto& file : filesToRemove) {
        if (atom::io::isFileExists(file)) {
            atom::io::removeFile(file);
        }
    }

    // Remove split parts
    for (int i = 0; i < 20; ++i) {  // Check up to 20 parts
        std::string splitPart = "split_part_" + std::to_string(i);
        std::string quickPart = "quick_part_" + std::to_string(i);

        if (atom::io::isFileExists(splitPart)) {
            atom::io::removeFile(splitPart);
        }
        if (atom::io::isFileExists(quickPart)) {
            atom::io::removeFile(quickPart);
        }
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "✂️  Atom I/O File Splitting Examples" << std::endl;
        std::cout << "====================================" << std::endl;

        // Run demonstrations
        demonstrateChunkCalculations();
        demonstrateBasicSplitting();
        demonstrateFileMerging();
        demonstrateQuickOperations();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All file splitting operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();  // Ensure cleanup even on error
        return 1;
    }
}
