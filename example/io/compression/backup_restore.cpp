/**
 * @file backup_restore.cpp
 * @brief Demonstration of backup, restore, and async file processing operations
 *
 * This example demonstrates:
 * - Creating file backups (with and without compression)
 * - Restoring files from backups
 * - Asynchronous batch file processing with processFilesAsync
 * - Compression options for backup operations
 * - Error handling for backup/restore workflows
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "atom/io/compression/compress.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test files for backup demonstrations
 */
void createTestFiles() {
    std::cout << "Creating test files for backup demonstrations..." << std::endl;

    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"backup_source1.txt",
         "Important data file 1\nContains critical information\n"
         "Multiple lines of valuable content\nMust be backed up safely."},
        {"backup_source2.txt",
         "Configuration data\nSetting1=value1\nSetting2=value2\n"
         "Setting3=value3\nEnd of configuration."},
        {"backup_source3.csv",
         "id,name,value\n1,alpha,100\n2,beta,200\n3,gamma,300\n"
         "4,delta,400\n5,epsilon,500"}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    // Create a larger file for compression testing
    {
        std::ofstream large("backup_large.txt");
        std::string pattern = "Backup test data with repeating content. ";
        for (int i = 0; i < 500; ++i) {
            large << pattern << "Line " << i << "\n";
        }
        large.close();
    }

    std::cout << "  Created " << testFiles.size() + 1 << " test files"
              << std::endl;
}

/**
 * @brief Demonstrates creating a simple (uncompressed) backup
 */
void demonstrateSimpleBackup() {
    std::cout << "\n=== Simple Backup (No Compression) ===" << std::endl;

    const std::string source = "backup_source1.txt";
    const std::string backup = "backup_source1.txt.bak";

    std::cout << "Creating backup of " << source << "..." << std::endl;

    auto result = atom::io::createBackup(source, backup, false);

    if (result.success) {
        std::cout << "  Backup created successfully: " << backup << std::endl;
        std::cout << "  Original size: " << fs::file_size(source) << " bytes"
                  << std::endl;
        std::cout << "  Backup size: " << fs::file_size(backup) << " bytes"
                  << std::endl;
    } else {
        std::cerr << "  Backup failed: " << result.error_message << std::endl;
    }
}

/**
 * @brief Demonstrates creating a compressed backup
 */
void demonstrateCompressedBackup() {
    std::cout << "\n=== Compressed Backup ===" << std::endl;

    const std::string source = "backup_large.txt";
    const std::string backup = "backup_large.txt.bak.gz";

    auto originalSize = fs::file_size(source);
    std::cout << "Creating compressed backup of " << source << " ("
              << originalSize << " bytes)..." << std::endl;

    atom::io::CompressionOptions options;
    options.level = 6;

    auto result = atom::io::createBackup(source, backup, true, options);

    if (result.success) {
        auto backupSize = fs::file_size(backup);
        double ratio = static_cast<double>(backupSize) / originalSize * 100.0;

        std::cout << "  Compressed backup created: " << backup << std::endl;
        std::cout << "  Original: " << originalSize << " bytes" << std::endl;
        std::cout << "  Backup: " << backupSize << " bytes (" << ratio << "%)"
                  << std::endl;
    } else {
        std::cerr << "  Compressed backup failed: " << result.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates restoring from a simple backup
 */
void demonstrateSimpleRestore() {
    std::cout << "\n=== Restore from Simple Backup ===" << std::endl;

    const std::string backup = "backup_source1.txt.bak";
    const std::string restored = "backup_source1_restored.txt";

    if (!fs::exists(backup)) {
        std::cout << "  No backup file found, skipping restore demo"
                  << std::endl;
        return;
    }

    std::cout << "Restoring from " << backup << "..." << std::endl;

    auto result = atom::io::restoreFromBackup(backup, restored, false);

    if (result.success) {
        std::cout << "  Restored successfully to: " << restored << std::endl;
        std::cout << "  Restored size: " << fs::file_size(restored) << " bytes"
                  << std::endl;

        // Verify content matches original
        std::ifstream origFile("backup_source1.txt");
        std::string origContent((std::istreambuf_iterator<char>(origFile)),
                                std::istreambuf_iterator<char>());
        origFile.close();

        std::ifstream restFile(restored);
        std::string restContent((std::istreambuf_iterator<char>(restFile)),
                                std::istreambuf_iterator<char>());
        restFile.close();

        std::cout << "  Content matches: "
                  << (origContent == restContent ? "YES" : "NO")
                  << std::endl;
    } else {
        std::cerr << "  Restore failed: " << result.error_message << std::endl;
    }
}

/**
 * @brief Demonstrates restoring from a compressed backup
 */
void demonstrateCompressedRestore() {
    std::cout << "\n=== Restore from Compressed Backup ===" << std::endl;

    const std::string backup = "backup_large.txt.bak.gz";
    const std::string restored = "backup_large_restored.txt";

    if (!fs::exists(backup)) {
        std::cout << "  No compressed backup found, skipping" << std::endl;
        return;
    }

    std::cout << "Restoring from compressed backup..." << std::endl;

    auto result = atom::io::restoreFromBackup(backup, restored, true);

    if (result.success) {
        auto restoredSize = fs::file_size(restored);
        auto originalSize = fs::file_size("backup_large.txt");

        std::cout << "  Restored successfully: " << restored << std::endl;
        std::cout << "  Restored size: " << restoredSize << " bytes"
                  << std::endl;
        std::cout << "  Matches original size: "
                  << (restoredSize == originalSize ? "YES" : "NO") << std::endl;
    } else {
        std::cerr << "  Compressed restore failed: " << result.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates asynchronous processing of multiple files
 */
void demonstrateAsyncProcessing() {
    std::cout << "\n=== Async File Processing ===" << std::endl;

    atom::io::Vector<atom::io::String> filesToProcess;
    filesToProcess.push_back("backup_source1.txt");
    filesToProcess.push_back("backup_source2.txt");
    filesToProcess.push_back("backup_source3.csv");
    filesToProcess.push_back("backup_large.txt");

    std::cout << "Processing " << filesToProcess.size()
              << " files asynchronously..." << std::endl;

    atom::io::CompressionOptions options;
    options.level = 6;
    options.use_parallel = true;

    auto start = std::chrono::high_resolution_clock::now();

    auto futureResults = atom::io::processFilesAsync(filesToProcess, options);

    // Wait for results
    auto results = futureResults.get();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "  Completed in " << duration.count() << "ms" << std::endl;
    std::cout << "\n  Results:" << std::endl;

    int successCount = 0;
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        std::string filename =
            (i < filesToProcess.size()) ? std::string(filesToProcess[i]) : "?";

        if (result.success) {
            successCount++;
            std::cout << "    " << filename << ": "
                      << result.original_size << " -> "
                      << result.compressed_size << " bytes ("
                      << (result.compression_ratio * 100) << "%)" << std::endl;
        } else {
            std::cout << "    " << filename << ": FAILED - "
                      << result.error_message << std::endl;
        }
    }

    std::cout << "\n  Summary: " << successCount << "/" << results.size()
              << " files processed successfully" << std::endl;
}

/**
 * @brief Demonstrates error handling for backup operations
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    // Backup non-existent file
    std::cout << "1. Backing up non-existent file..." << std::endl;
    auto result1 =
        atom::io::createBackup("nonexistent.txt", "backup.bak", false);
    std::cout << "  Result: "
              << (result1.success ? "Success" : "Failed")
              << (result1.error_message.empty()
                      ? ""
                      : " - " + std::string(result1.error_message))
              << std::endl;

    // Restore from non-existent backup
    std::cout << "\n2. Restoring from non-existent backup..." << std::endl;
    auto result2 =
        atom::io::restoreFromBackup("nonexistent.bak", "restored.txt", false);
    std::cout << "  Result: "
              << (result2.success ? "Success" : "Failed")
              << (result2.error_message.empty()
                      ? ""
                      : " - " + std::string(result2.error_message))
              << std::endl;
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n  Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "backup_source1.txt",          "backup_source2.txt",
        "backup_source3.csv",          "backup_large.txt",
        "backup_source1.txt.bak",      "backup_large.txt.bak.gz",
        "backup_source1_restored.txt", "backup_large_restored.txt",
        "backup_source1.txt.gz",       "backup_source2.txt.gz",
        "backup_source3.csv.gz",       "backup_large.txt.gz"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "  Atom I/O Backup & Restore Examples" << std::endl;
        std::cout << "=====================================" << std::endl;

        createTestFiles();

        demonstrateSimpleBackup();
        demonstrateCompressedBackup();
        demonstrateSimpleRestore();
        demonstrateCompressedRestore();
        demonstrateAsyncProcessing();
        demonstrateErrorHandling();

        cleanup();

        std::cout << "\n  All backup/restore operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "  Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
