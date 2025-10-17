/**
 * @file basic_async_io.cpp
 * @brief Basic demonstration of asynchronous file I/O operations
 *
 * This example demonstrates:
 * - Creating an async context for cancellation support
 * - Asynchronous file writing with proper error handling
 * - Asynchronous file reading with callback-based approach
 * - Asynchronous file deletion
 * - Proper resource cleanup and error handling patterns
 */

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include "atom/io/async/async_io.hpp"

using namespace atom::async::io;

/**
 * @brief Demonstrates basic asynchronous file operations
 *
 * This function shows the fundamental async I/O operations:
 * write, read, and delete operations with proper error handling.
 */
void demonstrateBasicAsyncOperations() {
    std::cout << "=== Basic Async I/O Operations Demo ===" << std::endl;

    const std::string filename = "async_example.txt";
    const std::string data_to_write =
        "Hello, Async World!\nThis is a test file for async I/O operations.";

    // Create async context for cancellation support
    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    try {
        // Asynchronous write operation
        std::cout << "1. Writing data to file asynchronously..." << std::endl;
        std::promise<AsyncResult<void>> writePromise;
        fileManager.asyncWrite(
            filename,
            std::span<const char>(data_to_write.data(), data_to_write.size()),
            [&](AsyncResult<void> result) {
                writePromise.set_value(std::move(result));
            });

        auto writeResult = writePromise.get_future().get();
        if (!writeResult.success) {
            std::cerr << "❌ Failed to write file: "
                      << writeResult.error_message << std::endl;
            return;
        }
        std::cout << "✅ Data written successfully to: " << filename
                  << std::endl;

        // Asynchronous read operation
        std::cout << "\n2. Reading data from file asynchronously..."
                  << std::endl;
        std::promise<AsyncResult<std::string>> readPromise;
        fileManager.asyncRead(filename, [&](AsyncResult<std::string> result) {
            readPromise.set_value(std::move(result));
        });

        auto readResult = readPromise.get_future().get();
        if (!readResult.success) {
            std::cerr << "❌ Failed to read file: " << readResult.error_message
                      << std::endl;
            return;
        }
        std::cout << "✅ Data read successfully:" << std::endl;
        std::cout << "Content: " << readResult.value << std::endl;
        std::cout << "Size: " << readResult.value.size() << " bytes"
                  << std::endl;

        // Asynchronous delete operation
        std::cout << "\n3. Deleting file asynchronously..." << std::endl;
        std::promise<AsyncResult<void>> deletePromise;
        fileManager.asyncDelete(filename, [&](AsyncResult<void> result) {
            deletePromise.set_value(std::move(result));
        });

        auto deleteResult = deletePromise.get_future().get();
        if (!deleteResult.success) {
            std::cerr << "❌ Failed to delete file: "
                      << deleteResult.error_message << std::endl;
            return;
        }
        std::cout << "✅ File deleted successfully: " << filename << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Exception during async operations: " << e.what()
                  << std::endl;
        throw;
    }
}

/**
 * @brief Demonstrates async context cancellation
 */
void demonstrateCancellation() {
    std::cout << "\n=== Async Context Cancellation Demo ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();

    std::cout << "Context cancelled: "
              << (context->is_cancelled() ? "Yes" : "No") << std::endl;

    // Simulate cancellation
    context->cancel();
    std::cout << "After cancel() - Context cancelled: "
              << (context->is_cancelled() ? "Yes" : "No") << std::endl;

    // Reset context
    context->reset();
    std::cout << "After reset() - Context cancelled: "
              << (context->is_cancelled() ? "Yes" : "No") << std::endl;
}

int main() {
    try {
        demonstrateBasicAsyncOperations();
        demonstrateCancellation();

        std::cout << "\n🎉 All async operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        return 1;
    }
}
