/**
 * @file advanced_async_operations.cpp
 * @brief Advanced demonstration of asynchronous I/O operations
 *
 * This example demonstrates:
 * - Batch file operations with parallel processing
 * - Timeout handling for async operations
 * - Context cancellation and cleanup
 * - File copying and moving operations
 * - Directory operations (create, remove, list)
 * - File status and permission operations
 * - Error handling and recovery patterns
 */

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/io/async/async_io.hpp"

using namespace atom::async::io;
namespace fs = std::filesystem;

/**
 * @brief Creates test files for async operations
 */
void createTestFiles() {
    std::cout << "Creating test files for async operations..." << std::endl;

    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"async_test1.txt",
         "Content of async test file 1\nMultiple lines for testing\nAsync "
         "operations"},
        {"async_test2.txt",
         "Content of async test file 2\nDifferent content here\nFor batch "
         "operations"},
        {"async_test3.txt",
         "Content of async test file 3\nYet another file\nFor comprehensive "
         "testing"},
        {"large_async_test.txt", std::string(10000, 'A') +
                                     "\nLarge file for performance testing\n" +
                                     std::string(10000, 'B')}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::cout << "✅ Created " << testFiles.size() << " test files"
              << std::endl;
}

/**
 * @brief Demonstrates batch file reading operations
 */
void demonstrateBatchOperations() {
    std::cout << "\n=== Batch File Operations ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    std::vector<std::string> filesToRead = {
        "async_test1.txt", "async_test2.txt", "async_test3.txt"};

    std::cout << "Reading " << filesToRead.size() << " files in batch..."
              << std::endl;

    std::promise<AsyncResult<std::vector<std::string>>> batchPromise;
    auto start = std::chrono::high_resolution_clock::now();

    fileManager.asyncBatchRead(
        filesToRead, [&](AsyncResult<std::vector<std::string>> result) {
            batchPromise.set_value(std::move(result));
        });

    auto batchResult = batchPromise.get_future().get();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (batchResult.success) {
        std::cout << "✅ Batch read completed in " << duration.count() << "ms"
                  << std::endl;
        std::cout << "Read " << batchResult.value.size()
                  << " files:" << std::endl;

        for (size_t i = 0;
             i < batchResult.value.size() && i < filesToRead.size(); ++i) {
            std::cout << "  📄 " << filesToRead[i] << " ("
                      << batchResult.value[i].size() << " bytes)" << std::endl;
        }
    } else {
        std::cerr << "❌ Batch read failed: " << batchResult.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates timeout handling
 */
void demonstrateTimeoutHandling() {
    std::cout << "\n=== Timeout Handling ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    // Test with a reasonable timeout
    std::cout << "1. Reading file with 5-second timeout..." << std::endl;
    std::promise<AsyncResult<std::string>> timeoutPromise1;

    fileManager.asyncReadWithTimeout(
        "large_async_test.txt", std::chrono::milliseconds(5000),
        [&](AsyncResult<std::string> result) {
            timeoutPromise1.set_value(std::move(result));
        });

    auto timeoutResult1 = timeoutPromise1.get_future().get();
    if (timeoutResult1.success) {
        std::cout << "✅ File read within timeout ("
                  << timeoutResult1.value.size() << " bytes)" << std::endl;
    } else {
        std::cout << "❌ File read failed or timed out: "
                  << timeoutResult1.error_message << std::endl;
    }

    // Test with a very short timeout (likely to timeout)
    std::cout << "\n2. Reading file with 1ms timeout (should timeout)..."
              << std::endl;
    std::promise<AsyncResult<std::string>> timeoutPromise2;

    fileManager.asyncReadWithTimeout(
        "large_async_test.txt", std::chrono::milliseconds(1),
        [&](AsyncResult<std::string> result) {
            timeoutPromise2.set_value(std::move(result));
        });

    auto timeoutResult2 = timeoutPromise2.get_future().get();
    if (timeoutResult2.success) {
        std::cout << "✅ File read within very short timeout" << std::endl;
    } else {
        std::cout << "⏰ Expected timeout or error: "
                  << timeoutResult2.error_message << std::endl;
    }
}

/**
 * @brief Demonstrates context cancellation
 */
void demonstrateContextCancellation() {
    std::cout << "\n=== Context Cancellation ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    std::cout << "Starting async operation and cancelling context..."
              << std::endl;

    // Start an async operation
    std::promise<AsyncResult<std::string>> cancelPromise;
    bool operationStarted = false;

    fileManager.asyncRead("large_async_test.txt",
                          [&](AsyncResult<std::string> result) {
                              cancelPromise.set_value(std::move(result));
                          });

    // Cancel the context immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    context->cancel();
    std::cout << "Context cancelled: "
              << (context->is_cancelled() ? "Yes" : "No") << std::endl;

    // The operation might still complete, but context is marked as cancelled
    auto cancelResult = cancelPromise.get_future().get();
    std::cout << "Operation result after cancellation: "
              << (cancelResult.success ? "Success" : "Failed") << std::endl;

    // Reset context for further operations
    context->reset();
    std::cout << "Context reset: "
              << (context->is_cancelled() ? "Still cancelled"
                                          : "Ready for new operations")
              << std::endl;
}

/**
 * @brief Demonstrates file copying and moving operations
 */
void demonstrateFileCopyMove() {
    std::cout << "\n=== File Copy and Move Operations ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    // Async file copy
    std::cout << "1. Copying file asynchronously..." << std::endl;
    std::promise<AsyncResult<void>> copyPromise;

    fileManager.asyncCopy("async_test1.txt", "copied_async_test.txt",
                          [&](AsyncResult<void> result) {
                              copyPromise.set_value(std::move(result));
                          });

    auto copyResult = copyPromise.get_future().get();
    if (copyResult.success) {
        std::cout << "✅ File copied successfully" << std::endl;

        // Verify copy
        if (fs::exists("copied_async_test.txt")) {
            auto originalSize = fs::file_size("async_test1.txt");
            auto copiedSize = fs::file_size("copied_async_test.txt");
            std::cout << "Original size: " << originalSize
                      << " bytes, Copied size: " << copiedSize << " bytes"
                      << std::endl;
        }
    } else {
        std::cerr << "❌ File copy failed: " << copyResult.error_message
                  << std::endl;
    }

    // Async file move
    std::cout << "\n2. Moving file asynchronously..." << std::endl;
    std::promise<AsyncResult<void>> movePromise;

    fileManager.asyncMove("copied_async_test.txt", "moved_async_test.txt",
                          [&](AsyncResult<void> result) {
                              movePromise.set_value(std::move(result));
                          });

    auto moveResult = movePromise.get_future().get();
    if (moveResult.success) {
        std::cout << "✅ File moved successfully" << std::endl;
        std::cout << "Original exists: "
                  << (fs::exists("copied_async_test.txt") ? "Yes" : "No")
                  << std::endl;
        std::cout << "Moved exists: "
                  << (fs::exists("moved_async_test.txt") ? "Yes" : "No")
                  << std::endl;
    } else {
        std::cerr << "❌ File move failed: " << moveResult.error_message
                  << std::endl;
    }
}

/**
 * @brief Demonstrates directory operations
 */
void demonstrateDirectoryOperations() {
    std::cout << "\n=== Directory Operations ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    // Create directory
    std::cout << "1. Creating directory asynchronously..." << std::endl;
    std::promise<AsyncResult<void>> createDirPromise;

    fileManager.asyncCreateDirectory(
        "async_test_dir", [&](AsyncResult<void> result) {
            createDirPromise.set_value(std::move(result));
        });

    auto createDirResult = createDirPromise.get_future().get();
    if (createDirResult.success) {
        std::cout << "✅ Directory created successfully" << std::endl;
    } else {
        std::cerr << "❌ Directory creation failed: "
                  << createDirResult.error_message << std::endl;
    }

    // List directory contents
    std::cout << "\n2. Listing directory contents..." << std::endl;
    std::promise<AsyncResult<std::vector<fs::path>>> listPromise;

    fileManager.asyncListDirectory(
        ".", [&](AsyncResult<std::vector<fs::path>> result) {
            listPromise.set_value(std::move(result));
        });

    auto listResult = listPromise.get_future().get();
    if (listResult.success) {
        std::cout << "✅ Directory listing completed ("
                  << listResult.value.size() << " items)" << std::endl;
        std::cout << "Sample items:" << std::endl;

        int count = 0;
        for (const auto& path : listResult.value) {
            if (count++ >= 5)
                break;  // Show only first 5 items
            std::cout << "  " << (fs::is_directory(path) ? "📁" : "📄") << " "
                      << path.filename() << std::endl;
        }
        if (listResult.value.size() > 5) {
            std::cout << "  ... and " << (listResult.value.size() - 5)
                      << " more items" << std::endl;
        }
    } else {
        std::cerr << "❌ Directory listing failed: " << listResult.error_message
                  << std::endl;
    }
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "async_test1.txt",       "async_test2.txt",
        "async_test3.txt",       "large_async_test.txt",
        "copied_async_test.txt", "moved_async_test.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    if (fs::exists("async_test_dir")) {
        fs::remove("async_test_dir");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "⚡ Atom I/O Advanced Async Operations Examples"
                  << std::endl;
        std::cout << "============================================="
                  << std::endl;

        // Setup
        createTestFiles();

        // Run demonstrations
        demonstrateBatchOperations();
        demonstrateTimeoutHandling();
        demonstrateContextCancellation();
        demonstrateFileCopyMove();
        demonstrateDirectoryOperations();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All advanced async operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
