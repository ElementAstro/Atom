/**
 * @file async_compression.cpp
 * @brief Demonstration of asynchronous compression operations
 *
 * This example demonstrates:
 * - Asynchronous file compression with callbacks
 * - Parallel compression of multiple files
 * - Progress monitoring for large file compression
 * - Async folder compression
 * - Error handling in async compression
 * - Performance comparison with synchronous compression
 */

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/io/async/async_compress.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test files for async compression demonstration
 */
void createAsyncTestFiles() {
    std::cout << "Creating test files for async compression..." << std::endl;

    // Create files of different sizes
    std::vector<std::pair<std::string, size_t>> testFiles = {
        {"async_small.txt", 1024},           // 1KB
        {"async_medium.txt", 100 * 1024},    // 100KB
        {"async_large.txt", 1024 * 1024},    // 1MB
        {"async_huge.txt", 5 * 1024 * 1024}  // 5MB
    };

    for (const auto& [filename, size] : testFiles) {
        std::ofstream file(filename);

        // Create content with some pattern for better compression
        std::string pattern = "This is a test pattern for async compression. ";
        size_t patternSize = pattern.size();
        size_t written = 0;

        while (written < size) {
            size_t toWrite = std::min(patternSize, size - written);
            file.write(pattern.c_str(), toWrite);
            written += toWrite;
        }

        file.close();
        std::cout << "  📄 Created " << filename << " (" << size << " bytes)"
                  << std::endl;
    }

    std::cout << "✅ Created " << testFiles.size() << " test files"
              << std::endl;
}

/**
 * @brief Demonstrates basic async compression
 */
void demonstrateBasicAsyncCompression() {
    std::cout << "\n=== Basic Async Compression ===" << std::endl;

    // Note: This is a conceptual example. The actual async compression API
    // would depend on the specific implementation in the atom::async::io
    // namespace.

    const std::string inputFile = "async_medium.txt";
    const std::string outputFile = "async_medium.txt.gz";

    std::cout << "Compressing " << inputFile << " asynchronously..."
              << std::endl;

    auto originalSize = fs::file_size(inputFile);
    std::cout << "Original size: " << originalSize << " bytes" << std::endl;

    // Simulate async compression with future/promise
    std::promise<bool> compressionPromise;
    auto compressionFuture = compressionPromise.get_future();

    auto start = std::chrono::high_resolution_clock::now();

    // In a real implementation, this would use the actual async compression API
    std::thread([&]() {
        try {
            // Simulate compression work
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Use synchronous compression for demonstration
            auto result = atom::io::compressFile(inputFile, ".");
            compressionPromise.set_value(result.success);
        } catch (...) {
            compressionPromise.set_value(false);
        }
    }).detach();

    // Wait for completion
    bool success = compressionFuture.get();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (success && fs::exists(outputFile)) {
        auto compressedSize = fs::file_size(outputFile);
        double ratio = (double)compressedSize / originalSize;

        std::cout << "✅ Async compression completed in " << duration.count()
                  << "ms" << std::endl;
        std::cout << "Compressed size: " << compressedSize << " bytes"
                  << std::endl;
        std::cout << "Compression ratio: " << (ratio * 100) << "%" << std::endl;
    } else {
        std::cout << "❌ Async compression failed" << std::endl;
    }
}

/**
 * @brief Demonstrates parallel compression of multiple files
 */
void demonstrateParallelCompression() {
    std::cout << "\n=== Parallel File Compression ===" << std::endl;

    std::vector<std::string> filesToCompress = {
        "async_small.txt", "async_medium.txt", "async_large.txt"};

    std::cout << "Compressing " << filesToCompress.size()
              << " files in parallel..." << std::endl;

    std::vector<std::future<std::pair<std::string, bool>>> futures;
    auto start = std::chrono::high_resolution_clock::now();

    // Launch async compression for each file
    for (const auto& file : filesToCompress) {
        auto future = std::async(
            std::launch::async, [file]() -> std::pair<std::string, bool> {
                try {
                    auto result = atom::io::compressFile(file, ".");
                    return {file, result.success};
                } catch (...) {
                    return {file, false};
                }
            });

        futures.push_back(std::move(future));
    }

    // Collect results
    std::vector<std::pair<std::string, bool>> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "✅ Parallel compression completed in " << duration.count()
              << "ms" << std::endl;

    // Show results
    for (const auto& [filename, success] : results) {
        if (success) {
            auto originalSize = fs::file_size(filename);
            auto compressedFile = filename + ".gz";

            if (fs::exists(compressedFile)) {
                auto compressedSize = fs::file_size(compressedFile);
                double ratio = (double)compressedSize / originalSize;

                std::cout << "  ✅ " << filename << ": " << originalSize
                          << " -> " << compressedSize << " bytes ("
                          << (ratio * 100) << "%)" << std::endl;
            }
        } else {
            std::cout << "  ❌ " << filename << ": Compression failed"
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates progress monitoring for large file compression
 */
void demonstrateProgressMonitoring() {
    std::cout << "\n=== Progress Monitoring ===" << std::endl;

    const std::string largeFile = "async_huge.txt";

    if (!fs::exists(largeFile)) {
        std::cout << "❌ Large test file not found" << std::endl;
        return;
    }

    auto fileSize = fs::file_size(largeFile);
    std::cout << "Compressing large file: " << largeFile << " ("
              << (fileSize / 1024 / 1024) << " MB)" << std::endl;

    // Simulate progress monitoring
    std::atomic<bool> compressionDone{false};
    std::atomic<int> progress{0};

    // Start compression in background
    auto compressionFuture = std::async(std::launch::async, [&]() {
        auto result = atom::io::compressFile(largeFile, ".");
        compressionDone = true;
        return result.success;
    });

    // Monitor progress (simulated)
    std::cout << "Progress: ";
    while (!compressionDone) {
        std::cout << "█" << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    bool success = compressionFuture.get();
    std::cout << " Done!" << std::endl;

    if (success) {
        auto compressedFile = largeFile + ".gz";
        if (fs::exists(compressedFile)) {
            auto compressedSize = fs::file_size(compressedFile);
            double ratio = (double)compressedSize / fileSize;

            std::cout << "✅ Large file compression completed" << std::endl;
            std::cout << "Original: " << (fileSize / 1024 / 1024) << " MB"
                      << std::endl;
            std::cout << "Compressed: " << (compressedSize / 1024 / 1024)
                      << " MB" << std::endl;
            std::cout << "Ratio: " << (ratio * 100) << "%" << std::endl;
        }
    } else {
        std::cout << "❌ Large file compression failed" << std::endl;
    }
}

/**
 * @brief Demonstrates async folder compression
 */
void demonstrateAsyncFolderCompression() {
    std::cout << "\n=== Async Folder Compression ===" << std::endl;

    // Create a test folder structure
    fs::create_directories("async_test_folder/subdir");

    // Copy some test files
    if (fs::exists("async_small.txt")) {
        fs::copy_file("async_small.txt", "async_test_folder/file1.txt");
    }
    if (fs::exists("async_medium.txt")) {
        fs::copy_file("async_medium.txt", "async_test_folder/subdir/file2.txt");
    }

    std::cout << "Created test folder structure" << std::endl;

    // Compress folder asynchronously
    auto folderFuture = std::async(std::launch::async, []() {
        return atom::io::compressFolder("async_test_folder",
                                        "async_test_folder.zip");
    });

    std::cout << "Compressing folder asynchronously..." << std::endl;

    auto result = folderFuture.get();

    if (result.success && fs::exists("async_test_folder.zip")) {
        auto zipSize = fs::file_size("async_test_folder.zip");
        std::cout << "✅ Async folder compression completed" << std::endl;
        std::cout << "ZIP size: " << zipSize << " bytes" << std::endl;

        // List contents
        auto contents = atom::io::listZipContents("async_test_folder.zip");
        std::cout << "Files in ZIP: " << contents.size() << std::endl;
        for (const auto& file : contents) {
            std::cout << "  📄 " << file.name << " (" << file.size << " bytes)"
                      << std::endl;
        }
    } else {
        std::cout << "❌ Async folder compression failed: "
                  << result.error_message << std::endl;
    }
}

/**
 * @brief Demonstrates performance comparison between sync and async compression
 */
void demonstratePerformanceComparison() {
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    const std::string testFile = "async_large.txt";

    if (!fs::exists(testFile)) {
        std::cout << "❌ Test file not found for performance comparison"
                  << std::endl;
        return;
    }

    // Synchronous compression
    std::cout << "1. Synchronous compression..." << std::endl;
    auto syncStart = std::chrono::high_resolution_clock::now();
    auto syncResult =
        atom::io::compressFile(testFile, ".", atom::io::CompressionOptions{});
    auto syncEnd = std::chrono::high_resolution_clock::now();
    auto syncDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        syncEnd - syncStart);

    // Clean up sync result
    std::string syncCompressed = testFile + ".gz";
    if (fs::exists(syncCompressed)) {
        fs::remove(syncCompressed);
    }

    // Asynchronous compression
    std::cout << "2. Asynchronous compression..." << std::endl;
    auto asyncStart = std::chrono::high_resolution_clock::now();
    auto asyncFuture = std::async(std::launch::async, [&testFile]() {
        return atom::io::compressFile(testFile, ".");
    });
    auto asyncResult = asyncFuture.get();
    auto asyncEnd = std::chrono::high_resolution_clock::now();
    auto asyncDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        asyncEnd - asyncStart);

    // Results
    std::cout << "\nPerformance Results:" << std::endl;
    std::cout << "  Sync time:  " << syncDuration.count() << "ms" << std::endl;
    std::cout << "  Async time: " << asyncDuration.count() << "ms" << std::endl;

    if (asyncDuration < syncDuration) {
        auto improvement = ((syncDuration - asyncDuration).count() * 100.0) /
                           syncDuration.count();
        std::cout << "  🚀 Async is " << improvement << "% faster" << std::endl;
    } else {
        auto overhead = ((asyncDuration - syncDuration).count() * 100.0) /
                        syncDuration.count();
        std::cout << "  ⚠️  Async has " << overhead << "% overhead" << std::endl;
    }
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "async_small.txt",     "async_small.txt.gz", "async_medium.txt",
        "async_medium.txt.gz", "async_large.txt",    "async_large.txt.gz",
        "async_huge.txt",      "async_huge.txt.gz",  "async_test_folder.zip"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    if (fs::exists("async_test_folder")) {
        fs::remove_all("async_test_folder");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "⚡ Atom I/O Async Compression Examples" << std::endl;
        std::cout << "=====================================" << std::endl;

        // Setup
        createAsyncTestFiles();

        // Run demonstrations
        demonstrateBasicAsyncCompression();
        demonstrateParallelCompression();
        demonstrateProgressMonitoring();
        demonstrateAsyncFolderCompression();
        demonstratePerformanceComparison();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All async compression operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
