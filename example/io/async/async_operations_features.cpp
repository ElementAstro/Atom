/**
 * @file async_operations_features.cpp
 * @brief Advanced demonstration of asynchronous I/O operations
 *
 * This example demonstrates:
 * - Timeout handling for async read operations
 * - Context cancellation and cleanup
 * - File copying and moving operations
 * - File status and existence checks
 * - Error handling and recovery patterns
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/io/async/async_io.hpp"

using namespace atom::io::async;
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
         "Content of async test file 2\nDifferent content here"},
        {"async_test3.txt",
         "Content of async test file 3\nFor comprehensive testing"},
        {"large_async_test.txt", std::string(10000, 'A') +
                                     "\nLarge file for performance testing\n" +
                                     std::string(10000, 'B')}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::cout << "  Created " << testFiles.size() << " test files" << std::endl;
}

/**
 * @brief Demonstrates timeout handling for async reads
 */
void demonstrateTimeoutHandling() {
    std::cout << "\n=== Timeout Handling ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Test with a reasonable timeout
    std::cout << "1. Reading file with 5-second timeout..." << std::endl;
    std::promise<AsyncResult<std::string>> timeoutPromise1;

    fileManager.asyncReadWithTimeout(
        std::string("large_async_test.txt"), std::chrono::milliseconds(5000),
        [&](AsyncResult<std::string> result) {
            timeoutPromise1.set_value(std::move(result));
        });

    auto timeoutResult1 = timeoutPromise1.get_future().get();
    if (timeoutResult1.success) {
        std::cout << "  File read within timeout ("
                  << timeoutResult1.value.size() << " bytes)" << std::endl;
    } else {
        std::cout << "  File read failed or timed out: "
                  << timeoutResult1.error_message << std::endl;
    }

    // Test with a very short timeout
    std::cout << "\n2. Reading file with 1ms timeout (may timeout)..."
              << std::endl;
    std::promise<AsyncResult<std::string>> timeoutPromise2;

    fileManager.asyncReadWithTimeout(
        std::string("large_async_test.txt"), std::chrono::milliseconds(1),
        [&](AsyncResult<std::string> result) {
            timeoutPromise2.set_value(std::move(result));
        });

    auto timeoutResult2 = timeoutPromise2.get_future().get();
    if (timeoutResult2.success) {
        std::cout << "  File read within very short timeout" << std::endl;
    } else {
        std::cout << "  Expected timeout or error: "
                  << timeoutResult2.error_message << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates context cancellation
 */
void demonstrateContextCancellation() {
    std::cout << "\n=== Context Cancellation ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    std::cout << "Starting async operation and cancelling context..."
              << std::endl;

    std::promise<AsyncResult<std::string>> cancelPromise;

    fileManager.asyncRead(std::string("large_async_test.txt"),
                          [&](AsyncResult<std::string> result) {
                              cancelPromise.set_value(std::move(result));
                          });

    // Cancel the context immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    context->cancel();
    std::cout << "  Context cancelled: "
              << (context->is_cancelled() ? "Yes" : "No") << std::endl;

    auto cancelResult = cancelPromise.get_future().get();
    std::cout << "  Operation result after cancellation: "
              << (cancelResult.success ? "Success" : "Failed") << std::endl;

    // Reset context for further operations
    context->reset();
    std::cout << "  Context reset: "
              << (context->is_cancelled() ? "Still cancelled"
                                          : "Ready for new operations")
              << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates file copying and moving operations
 */
void demonstrateFileCopyMove() {
    std::cout << "\n=== File Copy and Move Operations ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Async file copy
    std::cout << "1. Copying file asynchronously..." << std::endl;
    std::promise<AsyncResult<void>> copyPromise;

    fileManager.asyncCopy(std::string("async_test1.txt"),
                          std::string("copied_async_test.txt"),
                          [&](AsyncResult<void> result) {
                              copyPromise.set_value(std::move(result));
                          });

    auto copyResult = copyPromise.get_future().get();
    if (copyResult.success) {
        std::cout << "  File copied successfully" << std::endl;
        if (fs::exists("copied_async_test.txt")) {
            auto originalSize = fs::file_size("async_test1.txt");
            auto copiedSize = fs::file_size("copied_async_test.txt");
            std::cout << "  Original: " << originalSize
                      << " bytes, Copy: " << copiedSize << " bytes"
                      << std::endl;
        }
    } else {
        std::cerr << "  File copy failed: " << copyResult.error_message
                  << std::endl;
    }

    // Async file move
    std::cout << "\n2. Moving file asynchronously..." << std::endl;
    std::promise<AsyncResult<void>> movePromise;

    fileManager.asyncMove(std::string("copied_async_test.txt"),
                          std::string("moved_async_test.txt"),
                          [&](AsyncResult<void> result) {
                              movePromise.set_value(std::move(result));
                          });

    auto moveResult = movePromise.get_future().get();
    if (moveResult.success) {
        std::cout << "  File moved successfully" << std::endl;
        std::cout << "  Original exists: "
                  << (fs::exists("copied_async_test.txt") ? "Yes" : "No")
                  << std::endl;
        std::cout << "  Moved exists: "
                  << (fs::exists("moved_async_test.txt") ? "Yes" : "No")
                  << std::endl;
    } else {
        std::cerr << "  File move failed: " << moveResult.error_message
                  << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates file stat and existence checks
 */
void demonstrateFileStatus() {
    std::cout << "\n=== File Status and Existence ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Check file existence
    std::cout << "1. Checking file existence..." << std::endl;
    std::promise<AsyncResult<bool>> existsPromise;
    fileManager.asyncExists(std::string("async_test1.txt"),
                            [&](AsyncResult<bool> result) {
                                existsPromise.set_value(std::move(result));
                            });

    auto existsResult = existsPromise.get_future().get();
    if (existsResult.success) {
        std::cout << "  async_test1.txt exists: "
                  << (existsResult.value ? "Yes" : "No") << std::endl;
    }

    // Check non-existent file
    std::promise<AsyncResult<bool>> noExistsPromise;
    fileManager.asyncExists(std::string("nonexistent.txt"),
                            [&](AsyncResult<bool> result) {
                                noExistsPromise.set_value(std::move(result));
                            });

    auto noExistsResult = noExistsPromise.get_future().get();
    if (noExistsResult.success) {
        std::cout << "  nonexistent.txt exists: "
                  << (noExistsResult.value ? "Yes" : "No") << std::endl;
    }

    // Get file status
    std::cout << "\n2. Getting file status..." << std::endl;
    std::promise<AsyncResult<fs::file_status>> statPromise;
    fileManager.asyncStat(
        std::string("async_test1.txt"),
        [&](AsyncResult<fs::file_status> result) {
            statPromise.set_value(std::move(result));
        });

    auto statResult = statPromise.get_future().get();
    if (statResult.success) {
        auto status = statResult.value;
        std::cout << "  File type: "
                  << (status.type() == fs::file_type::regular ? "Regular file"
                                                              : "Other")
                  << std::endl;
    } else {
        std::cerr << "  Stat failed: " << statResult.error_message << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\nCleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "async_test1.txt",       "async_test2.txt",
        "async_test3.txt",       "large_async_test.txt",
        "copied_async_test.txt", "moved_async_test.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "Atom I/O Advanced Async Operations Examples" << std::endl;
        std::cout << "===========================================" << std::endl;

        createTestFiles();

        demonstrateTimeoutHandling();
        demonstrateContextCancellation();
        demonstrateFileCopyMove();
        demonstrateFileStatus();

        cleanup();

        std::cout
            << "\nAll advanced async operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
