/**
 * @file coroutine_operations.cpp
 * @brief Demonstration of callback-based async I/O operations with chaining
 *
 * This example demonstrates:
 * - Async file reading and writing with callbacks
 * - Sequential async operations via chained callbacks
 * - Error handling in async operations
 * - File existence and status checks
 * - Combining multiple async operations
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <span>
#include <string>
#include <vector>
#include "atom/io/async/async_io.hpp"

using namespace atom::io::async;
namespace fs = std::filesystem;

/**
 * @brief Creates test files for demonstrations
 */
void createTestFiles() {
    std::cout << "Creating test files..." << std::endl;

    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"coro_test1.txt",
         "First test file\nWith some content\nFor async operations"},
        {"coro_test2.txt",
         "Second test file\nDifferent content here\nFor testing purposes"},
        {"coro_input.txt",
         "Input file for processing\nLine 2\nLine 3\nLine 4\nFinal line"}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::cout << "  Created " << testFiles.size() << " test files" << std::endl;
}

/**
 * @brief Demonstrates simple async read and write
 */
void demonstrateSimpleReadWrite() {
    std::cout << "\n=== Simple Async Read/Write ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Async read
    std::cout << "1. Reading coro_test1.txt..." << std::endl;
    std::promise<AsyncResult<std::string>> readPromise;
    fileManager.asyncRead(std::string("coro_test1.txt"),
                          [&](AsyncResult<std::string> result) {
                              readPromise.set_value(std::move(result));
                          });

    auto readResult = readPromise.get_future().get();
    if (readResult.success) {
        std::cout << "  Read " << readResult.value.size() << " bytes"
                  << std::endl;
        std::cout << "  Preview: "
                  << readResult.value.substr(
                         0, std::min<size_t>(50, readResult.value.size()))
                  << "..." << std::endl;
    } else {
        std::cerr << "  Read failed: " << readResult.error_message << std::endl;
    }

    // Async write
    std::cout << "\n2. Writing coro_output.txt..." << std::endl;
    std::string content =
        "Hello from async operations!\nThis is async I/O.";
    std::promise<AsyncResult<void>> writePromise;
    fileManager.asyncWrite(
        std::string("coro_output.txt"), content,
        [&](AsyncResult<void> result) {
            writePromise.set_value(std::move(result));
        });

    auto writeResult = writePromise.get_future().get();
    if (writeResult.success) {
        std::cout << "  Write successful (" << content.size() << " bytes)"
                  << std::endl;
    } else {
        std::cerr << "  Write failed: " << writeResult.error_message
                  << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates sequential file processing via chained callbacks
 */
void demonstrateSequentialProcessing() {
    std::cout << "\n=== Sequential File Processing ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    std::vector<std::string> filesToProcess = {
        "coro_test1.txt", "coro_test2.txt", "coro_input.txt"};

    for (const auto& filename : filesToProcess) {
        // Read file
        std::promise<AsyncResult<std::string>> readPromise;
        fileManager.asyncRead(std::string(filename),
                              [&](AsyncResult<std::string> result) {
                                  readPromise.set_value(std::move(result));
                              });

        auto readResult = readPromise.get_future().get();
        if (readResult.success) {
            std::cout << "  Read " << filename << " ("
                      << readResult.value.size() << " bytes)" << std::endl;

            // Process content (convert to uppercase)
            std::string processed = readResult.value;
            std::transform(processed.begin(), processed.end(),
                           processed.begin(), ::toupper);

            // Write processed content
            std::string outputName = "processed_" + filename;
            std::promise<AsyncResult<void>> writePromise;
            fileManager.asyncWrite(
                std::string(outputName), processed,
                [&](AsyncResult<void> result) {
                    writePromise.set_value(std::move(result));
                });

            auto writeResult = writePromise.get_future().get();
            if (writeResult.success) {
                std::cout << "  Processed -> " << outputName << std::endl;
            } else {
                std::cerr << "  Write failed: " << writeResult.error_message
                          << std::endl;
            }
        } else {
            std::cerr << "  Read failed for " << filename << ": "
                      << readResult.error_message << std::endl;
        }
    }

    std::cout << "  Sequential processing completed" << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates error handling in async operations
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Read non-existent file
    std::cout << "1. Reading non-existent file..." << std::endl;
    std::promise<AsyncResult<std::string>> readPromise;
    fileManager.asyncRead(std::string("non_existent_file.txt"),
                          [&](AsyncResult<std::string> result) {
                              readPromise.set_value(std::move(result));
                          });

    auto readResult = readPromise.get_future().get();
    if (!readResult.success) {
        std::cout << "  Expected error: " << readResult.error_message
                  << std::endl;
    }

    // Check existence
    std::cout << "\n2. Checking file existence..." << std::endl;
    std::promise<AsyncResult<bool>> existsPromise;
    fileManager.asyncExists(std::string("coro_test1.txt"),
                            [&](AsyncResult<bool> result) {
                                existsPromise.set_value(std::move(result));
                            });

    auto existsResult = existsPromise.get_future().get();
    if (existsResult.success) {
        std::cout << "  coro_test1.txt exists: "
                  << (existsResult.value ? "Yes" : "No") << std::endl;
    }

    std::promise<AsyncResult<bool>> noExistsPromise;
    fileManager.asyncExists(std::string("non_existent.txt"),
                            [&](AsyncResult<bool> result) {
                                noExistsPromise.set_value(std::move(result));
                            });

    auto noExistsResult = noExistsPromise.get_future().get();
    if (noExistsResult.success) {
        std::cout << "  non_existent.txt exists: "
                  << (noExistsResult.value ? "Yes" : "No") << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates combined read-process-write operations
 */
void demonstrateCombinedOperations() {
    std::cout << "\n=== Combined Read-Process-Write ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
#else
    AsyncFile fileManager(context);
#endif

    // Read input file
    std::promise<AsyncResult<std::string>> readPromise;
    fileManager.asyncRead(std::string("coro_input.txt"),
                          [&](AsyncResult<std::string> result) {
                              readPromise.set_value(std::move(result));
                          });

    auto inputResult = readPromise.get_future().get();
    if (!inputResult.success) {
        std::cerr << "  Failed to read input: " << inputResult.error_message
                  << std::endl;
        return;
    }

    std::cout << "  Read input file (" << inputResult.value.size() << " bytes)"
              << std::endl;

    // Process: split into lines, create summary
    std::istringstream iss(inputResult.value);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    std::string summary = "File Processing Summary\n";
    summary += "=======================\n";
    summary += "Total lines: " + std::to_string(lines.size()) + "\n";
    summary +=
        "Total characters: " + std::to_string(inputResult.value.size()) + "\n";
    summary += "\nLines:\n";
    for (size_t i = 0; i < lines.size(); ++i) {
        summary += std::to_string(i + 1) + ". " + lines[i] + "\n";
    }

    // Write summary
    std::promise<AsyncResult<void>> writePromise;
    fileManager.asyncWrite(
        std::string("file_summary.txt"), summary,
        [&](AsyncResult<void> result) {
            writePromise.set_value(std::move(result));
        });

    auto writeResult = writePromise.get_future().get();
    if (writeResult.success) {
        std::cout << "  Created summary: file_summary.txt" << std::endl;
    } else {
        std::cerr << "  Failed to write summary: "
                  << writeResult.error_message << std::endl;
    }

    // Verify with async delete (cleanup processed file)
    std::promise<AsyncResult<void>> deletePromise;
    fileManager.asyncDelete(std::string("file_summary.txt"),
                            [&](AsyncResult<void> result) {
                                deletePromise.set_value(std::move(result));
                            });

    auto deleteResult = deletePromise.get_future().get();
    if (deleteResult.success) {
        std::cout << "  Deleted file_summary.txt" << std::endl;
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
        "coro_test1.txt",           "coro_test2.txt",
        "coro_input.txt",           "coro_output.txt",
        "processed_coro_test1.txt", "processed_coro_test2.txt",
        "processed_coro_input.txt", "file_summary.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "Atom I/O Async Operations Examples" << std::endl;
        std::cout << "==================================" << std::endl;

        createTestFiles();

        demonstrateSimpleReadWrite();
        demonstrateSequentialProcessing();
        demonstrateErrorHandling();
        demonstrateCombinedOperations();

        cleanup();

        std::cout << "\nAll async operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
