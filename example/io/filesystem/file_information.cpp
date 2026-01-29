/**
 * @file file_information.cpp
 * @brief Comprehensive demonstration of file information retrieval
 *
 * This example demonstrates:
 * - Retrieving detailed file information and metadata
 * - Displaying file properties in a formatted manner
 * - Handling different file types (regular files, directories, symlinks)
 * - Cross-platform file information handling
 * - Error handling for inaccessible files
 * - Batch file information processing
 */

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include "atom/io/filesystem/file_info.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates diverse test files for information demonstration
 */
void createTestFiles() {
    std::cout << "Creating diverse test files for information retrieval..."
              << std::endl;

    // Create regular files with different characteristics
    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"info_small.txt", "Small file content."},
        {"info_medium.txt",
         std::string(1000, 'A') + "\nMedium sized file for testing."},
        {"info_large.csv", ""},   // Will be filled with CSV data
        {"info_binary.dat", ""},  // Will be filled with binary data
        {"info_empty.txt", ""},   // Empty file
        {".info_hidden.txt", "Hidden file content (starts with dot)."}};

    // Create small and medium files
    for (const auto& [filename, content] : testFiles) {
        if (filename == "info_large.csv")
            continue;
        if (filename == "info_binary.dat")
            continue;

        std::ofstream file(filename);
        file << content;
        file.close();
    }

    // Create large CSV file
    {
        std::ofstream csvFile("info_large.csv");
        csvFile << "id,name,value,timestamp" << std::endl;
        for (int i = 0; i < 5000; ++i) {
            csvFile << i << ",Item" << i << "," << (i * 1.5)
                    << ",2024-01-01T00:00:00Z" << std::endl;
        }
        csvFile.close();
    }

    // Create binary file
    {
        std::ofstream binFile("info_binary.dat", std::ios::binary);
        for (int i = 0; i < 1000; ++i) {
            char data[4] = {static_cast<char>(i & 0xFF),
                            static_cast<char>((i >> 8) & 0xFF),
                            static_cast<char>((i >> 16) & 0xFF),
                            static_cast<char>((i >> 24) & 0xFF)};
            binFile.write(data, 4);
        }
        binFile.close();
    }

    // Create directory
    fs::create_directory("info_test_dir");

    // Create subdirectory with files
    fs::create_directories("info_test_dir/subdir");
    std::ofstream subFile("info_test_dir/subdir/nested_file.txt");
    subFile << "File in subdirectory";
    subFile.close();

    std::cout << "✅ Created test files and directories" << std::endl;
}

/**
 * @brief Demonstrates basic file information retrieval
 */
void demonstrateBasicFileInfo() {
    std::cout << "\n=== Basic File Information ===" << std::endl;

    std::vector<std::string> testFiles = {"info_small.txt", "info_medium.txt",
                                          "info_large.csv", "info_binary.dat",
                                          "info_empty.txt"};

    for (const auto& filename : testFiles) {
        if (!fs::exists(filename)) {
            std::cout << "❌ File not found: " << filename << std::endl;
            continue;
        }

        try {
            std::cout << "\n📄 File: " << filename << std::endl;
            std::cout << "----------------------------------------"
                      << std::endl;

            auto fileInfo = atom::io::getFileInfo(filename);
            atom::io::printFileInfo(fileInfo);

        } catch (const std::exception& e) {
            std::cout << "❌ Error getting file info: " << e.what()
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates directory information retrieval
 */
void demonstrateDirectoryInfo() {
    std::cout << "\n=== Directory Information ===" << std::endl;

    std::vector<std::string> directories = {
        "info_test_dir", "info_test_dir/subdir",
        "."  // Current directory
    };

    for (const auto& dirname : directories) {
        if (!fs::exists(dirname)) {
            std::cout << "❌ Directory not found: " << dirname << std::endl;
            continue;
        }

        try {
            std::cout << "\n📁 Directory: " << dirname << std::endl;
            std::cout << "----------------------------------------"
                      << std::endl;

            auto dirInfo = atom::io::getFileInfo(dirname);
            atom::io::printFileInfo(dirInfo);

            // Count contents
            if (fs::is_directory(dirname)) {
                int fileCount = 0, dirCount = 0;
                for (const auto& entry : fs::directory_iterator(dirname)) {
                    if (entry.is_regular_file())
                        fileCount++;
                    else if (entry.is_directory())
                        dirCount++;
                }
                std::cout << "Contents: " << fileCount << " files, " << dirCount
                          << " directories" << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "❌ Error getting directory info: " << e.what()
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates file information comparison
 */
void demonstrateFileComparison() {
    std::cout << "\n=== File Information Comparison ===" << std::endl;

    std::vector<std::string> filesToCompare = {
        "info_small.txt", "info_medium.txt", "info_large.csv",
        "info_binary.dat"};

    std::cout << "File Size Comparison:" << std::endl;
    std::cout << std::left << std::setw(20) << "File" << " | " << std::right
              << std::setw(10) << "Size (bytes)" << " | " << std::left
              << std::setw(15) << "Type" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    for (const auto& filename : filesToCompare) {
        if (!fs::exists(filename))
            continue;

        try {
            auto fileInfo = atom::io::getFileInfo(filename);

            std::cout << std::left << std::setw(20) << fileInfo.fileName
                      << " | " << std::right << std::setw(10)
                      << fileInfo.fileSize << " | " << std::left
                      << std::setw(15) << fileInfo.fileType << std::endl;

        } catch (const std::exception& e) {
            std::cout << std::left << std::setw(20) << filename << " | "
                      << std::right << std::setw(10) << "Error" << " | "
                      << std::left << std::setw(15) << e.what() << std::endl;
        }
    }
}

/**
 * @brief Demonstrates file time information analysis
 */
void demonstrateFileTimeAnalysis() {
    std::cout << "\n=== File Time Analysis ===" << std::endl;

    std::vector<std::string> testFiles = {"info_small.txt", "info_medium.txt",
                                          "info_large.csv"};

    std::cout << "File Time Information:" << std::endl;

    for (const auto& filename : testFiles) {
        if (!fs::exists(filename))
            continue;

        try {
            auto fileInfo = atom::io::getFileInfo(filename);

            std::cout << "\n📄 " << filename << ":" << std::endl;
            std::cout << "  Created: " << fileInfo.creationTime << std::endl;
            std::cout << "  Modified: " << fileInfo.lastModifiedTime
                      << std::endl;
            std::cout << "  Accessed: " << fileInfo.lastAccessTime << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ Error getting time info for " << filename << ": "
                      << e.what() << std::endl;
        }
    }
}

/**
 * @brief Demonstrates hidden file detection
 */
void demonstrateHiddenFileDetection() {
    std::cout << "\n=== Hidden File Detection ===" << std::endl;

    std::vector<std::string> testFiles = {
        "info_small.txt",   // Regular file
        ".info_hidden.txt"  // Hidden file (starts with dot)
    };

    std::cout << "Hidden File Status:" << std::endl;

    for (const auto& filename : testFiles) {
        if (!fs::exists(filename)) {
            std::cout << "❌ File not found: " << filename << std::endl;
            continue;
        }

        try {
            auto fileInfo = atom::io::getFileInfo(filename);

            std::cout << "📄 " << filename << ": "
                      << (fileInfo.isHidden ? "Hidden" : "Visible")
                      << std::endl;

        } catch (const std::exception& e) {
            std::cout << "❌ Error checking " << filename << ": " << e.what()
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates batch file information processing
 */
void demonstrateBatchProcessing() {
    std::cout << "\n=== Batch File Information Processing ===" << std::endl;

    std::cout << "Processing all test files in batch..." << std::endl;

    size_t totalFiles = 0;
    size_t totalSize = 0;
    size_t totalDirectories = 0;

    try {
        for (const auto& entry : fs::directory_iterator(".")) {
            std::string filename = entry.path().filename().string();

            // Only process our test files
            if (filename.find("info_") != 0 && filename != ".info_hidden.txt") {
                continue;
            }

            try {
                auto fileInfo = atom::io::getFileInfo(entry.path());

                if (fileInfo.fileType == "Directory") {
                    totalDirectories++;
                } else {
                    totalFiles++;
                    totalSize += fileInfo.fileSize;
                }

            } catch (const std::exception& e) {
                std::cout << "⚠️  Skipped " << filename << ": " << e.what()
                          << std::endl;
            }
        }

        std::cout << "\nBatch Processing Summary:" << std::endl;
        std::cout << "  Total files processed: " << totalFiles << std::endl;
        std::cout << "  Total directories: " << totalDirectories << std::endl;
        std::cout << "  Total file size: " << totalSize << " bytes ("
                  << (totalSize / 1024.0) << " KB)" << std::endl;
        std::cout << "  Average file size: "
                  << (totalFiles > 0 ? totalSize / totalFiles : 0) << " bytes"
                  << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Error during batch processing: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Demonstrates error handling for file information
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    // Test with non-existent file
    std::cout << "1. Testing with non-existent file..." << std::endl;
    try {
        auto fileInfo = atom::io::getFileInfo("non_existent_file.txt");
        std::cout << "❌ Should have thrown an exception" << std::endl;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ Correctly caught runtime error: " << e.what()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Caught exception: " << e.what() << std::endl;
    }

    // Test with empty path
    std::cout << "\n2. Testing with empty path..." << std::endl;
    try {
        auto fileInfo = atom::io::getFileInfo("");
        std::cout << "❌ Should have thrown an exception" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Correctly handled empty path: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Cleans up test files and directories
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files and directories..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "info_small.txt",  "info_medium.txt", "info_large.csv",
        "info_binary.dat", "info_empty.txt",  ".info_hidden.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    if (fs::exists("info_test_dir")) {
        fs::remove_all("info_test_dir");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "📊 Atom I/O File Information Examples" << std::endl;
        std::cout << "=====================================" << std::endl;

        // Setup
        createTestFiles();

        // Run demonstrations
        demonstrateBasicFileInfo();
        demonstrateDirectoryInfo();
        demonstrateFileComparison();
        demonstrateFileTimeAnalysis();
        demonstrateHiddenFileDetection();
        demonstrateBatchProcessing();
        demonstrateErrorHandling();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All file information operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
