/**
 * @file async_stream_operations.cpp
 * @brief Demonstration of asynchronous streaming file I/O operations
 *
 * This example demonstrates:
 * - AsyncStreamOps for chunked file reading
 * - AsyncStreamOps for chunked file writing
 * - Memory-efficient processing of large files
 * - Chunk size tuning for performance
 * - Read-process-write pipeline using AsyncFile + AsyncStreamOps
 * - Error handling in streaming operations
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include "atom/io/async/async_stream.hpp"

using namespace atom::io::async;
namespace fs = std::filesystem;

/**
 * @brief Creates test files for streaming demonstrations
 */
void createTestFiles() {
    std::cout << "Creating test files for streaming operations..." << std::endl;

    {
        std::ofstream file("stream_test_medium.txt");
        for (int i = 0; i < 5000; ++i) {
            file << "Line " << i
                 << ": This is test content for streaming read operations. "
                 << "Each line has enough data to demonstrate chunked reading."
                 << std::endl;
        }
        file.close();
    }

    {
        std::ofstream file("stream_test_large.bin", std::ios::binary);
        std::string pattern = "STREAM_DATA_BLOCK_";
        for (int i = 0; i < 20000; ++i) {
            file << pattern << i << "\n";
        }
        file.close();
    }

    std::cout << "  stream_test_medium.txt: "
              << fs::file_size("stream_test_medium.txt") << " bytes"
              << std::endl;
    std::cout << "  stream_test_large.bin: "
              << fs::file_size("stream_test_large.bin") << " bytes"
              << std::endl;
}

/**
 * @brief Demonstrates basic async streaming read using AsyncStreamOps
 */
void demonstrateStreamRead() {
    std::cout << "\n=== Async Streaming Read ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncStreamOps streamOps(ioCtx, context);
#else
    AsyncStreamOps streamOps(context);
#endif

    const std::string filename = "stream_test_medium.txt";
    const size_t chunkSize = 4096;

    std::cout << "Reading " << filename << " in " << chunkSize
              << "-byte chunks..." << std::endl;

    size_t totalBytesRead = 0;
    int chunkCount = 0;
    std::mutex mtx;
    bool completed = false;
    bool success = false;

    auto start = std::chrono::high_resolution_clock::now();

    streamOps.asyncStreamRead(
        filename, chunkSize,
        [&](AsyncResult<std::string> result) {
            std::lock_guard<std::mutex> lock(mtx);
            if (result.success) {
                totalBytesRead += result.value.size();
                chunkCount++;
            }
        },
        [&](AsyncResult<void> result) {
            std::lock_guard<std::mutex> lock(mtx);
            success = result.success;
            if (!result.success) {
                std::cerr << "  Read error: " << result.error_message
                          << std::endl;
            }
            completed = true;
        });

    // Wait for completion
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        if (completed) break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (success) {
        std::cout << "  Read " << chunkCount << " chunks" << std::endl;
        std::cout << "  Total bytes: " << totalBytesRead << std::endl;
        std::cout << "  Time: " << duration.count() << "ms" << std::endl;

        auto fileSize = fs::file_size(filename);
        std::cout << "  File size match: "
                  << (totalBytesRead == fileSize ? "YES" : "NO") << std::endl;
    } else {
        std::cerr << "  Streaming read failed" << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates async streaming write using AsyncStreamOps
 */
void demonstrateStreamWrite() {
    std::cout << "\n=== Async Streaming Write ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncStreamOps streamOps(ioCtx, context);
#else
    AsyncStreamOps streamOps(context);
#endif

    const std::string filename = "stream_output.txt";
    const size_t chunkSize = 2048;

    std::string fullData;
    for (int i = 0; i < 3000; ++i) {
        fullData += "Stream write line " + std::to_string(i) +
                    ": Data for chunked writing demonstration.\n";
    }

    std::cout << "Writing " << fullData.size() << " bytes in " << chunkSize
              << "-byte chunks..." << std::endl;

    bool completed = false;
    bool success = false;
    std::mutex mtx;

    auto start = std::chrono::high_resolution_clock::now();

    std::span<const char> dataSpan(fullData.data(), fullData.size());
    streamOps.asyncStreamWrite(
        filename, dataSpan, chunkSize,
        [&](AsyncResult<void> result) {
            std::lock_guard<std::mutex> lock(mtx);
            success = result.success;
            if (!result.success) {
                std::cerr << "  Write error: " << result.error_message
                          << std::endl;
            }
            completed = true;
        });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        if (completed) break;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (success) {
        auto writtenSize = fs::file_size(filename);
        std::cout << "  Written size: " << writtenSize << " bytes" << std::endl;
        std::cout << "  Time: " << duration.count() << "ms" << std::endl;
        std::cout << "  Data integrity: "
                  << (writtenSize == fullData.size() ? "PASSED" : "CHECK")
                  << std::endl;
    } else {
        std::cerr << "  Streaming write failed" << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates chunk size impact on performance
 */
void demonstrateChunkSizeComparison() {
    std::cout << "\n=== Chunk Size Performance Comparison ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();

    const std::string filename = "stream_test_large.bin";
    std::vector<size_t> chunkSizes = {1024, 4096, 16384, 65536};

    std::cout << "Reading " << filename << " with different chunk sizes:"
              << std::endl;

    for (size_t chunkSize : chunkSizes) {
#ifdef ATOM_USE_ASIO
        asio::io_context ioCtx;
        auto wg = asio::make_work_guard(ioCtx);
        std::jthread ioThread([&]() { ioCtx.run(); });
        AsyncStreamOps streamOps(ioCtx, context);
#else
        AsyncStreamOps streamOps(context);
#endif

        size_t totalBytes = 0;
        int chunks = 0;
        bool completed = false;
        bool success = false;
        std::mutex mtx;

        auto start = std::chrono::high_resolution_clock::now();

        streamOps.asyncStreamRead(
            filename, chunkSize,
            [&](AsyncResult<std::string> result) {
                std::lock_guard<std::mutex> lock(mtx);
                if (result.success) {
                    totalBytes += result.value.size();
                    chunks++;
                }
            },
            [&](AsyncResult<void> result) {
                std::lock_guard<std::mutex> lock(mtx);
                success = result.success;
                completed = true;
            });

        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            std::lock_guard<std::mutex> lock(mtx);
            if (completed) break;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        if (success) {
            std::cout << "  Chunk " << (chunkSize / 1024) << " KB: "
                      << chunks << " chunks, " << duration.count() << " us"
                      << std::endl;
        }

#ifdef ATOM_USE_ASIO
        wg.reset();
        ioCtx.stop();
#endif
    }
}

/**
 * @brief Demonstrates read-process-write pipeline
 */
void demonstrateStreamPipeline() {
    std::cout << "\n=== Streaming Read-Process-Write Pipeline ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncFile fileManager(ioCtx, context);
    AsyncStreamOps streamOps(ioCtx, context);
#else
    AsyncFile fileManager(context);
    AsyncStreamOps streamOps(context);
#endif

    const std::string inputFile = "stream_test_medium.txt";
    const std::string outputFile = "stream_processed_output.txt";
    const size_t chunkSize = 8192;

    std::cout << "Processing " << inputFile << " -> " << outputFile
              << " in " << (chunkSize / 1024) << " KB chunks..." << std::endl;

    // Step 1: Read the entire file using AsyncFile
    std::cout << "\n  Step 1: Reading input file..." << std::endl;
    bool readDone = false;
    std::string fileContent;
    std::mutex mtx;

    fileManager.asyncRead(inputFile,
                          [&](AsyncResult<std::string> result) {
                              std::lock_guard<std::mutex> lock(mtx);
                              if (result.success) {
                                  fileContent = std::move(result.value);
                              } else {
                                  std::cerr << "  Read failed: "
                                            << result.error_message
                                            << std::endl;
                              }
                              readDone = true;
                          });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        if (readDone) break;
    }

    if (fileContent.empty()) {
        std::cerr << "  No content read, skipping pipeline" << std::endl;
        return;
    }
    std::cout << "  Read " << fileContent.size() << " bytes" << std::endl;

    // Step 2: Process (convert to uppercase)
    std::cout << "  Step 2: Processing data..." << std::endl;
    std::transform(fileContent.begin(), fileContent.end(), fileContent.begin(),
                   ::toupper);

    // Step 3: Write processed data in chunks using AsyncStreamOps
    std::cout << "  Step 3: Writing processed data in chunks..." << std::endl;
    bool writeDone = false;
    bool writeSuccess = false;

    std::span<const char> processedSpan(fileContent.data(),
                                        fileContent.size());
    streamOps.asyncStreamWrite(
        outputFile, processedSpan, chunkSize,
        [&](AsyncResult<void> result) {
            std::lock_guard<std::mutex> lock(mtx);
            writeSuccess = result.success;
            if (!result.success) {
                std::cerr << "  Write failed: " << result.error_message
                          << std::endl;
            }
            writeDone = true;
        });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        if (writeDone) break;
    }

    if (writeSuccess) {
        auto outputSize = fs::file_size(outputFile);
        std::cout << "  Pipeline completed: " << outputSize << " bytes written"
                  << std::endl;
    }

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Demonstrates error handling in streaming operations
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling in Streaming ===" << std::endl;

    auto context = std::make_shared<AsyncContext>();
#ifdef ATOM_USE_ASIO
    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncStreamOps streamOps(ioCtx, context);
#else
    AsyncStreamOps streamOps(context);
#endif

    std::cout << "1. Streaming read from non-existent file..." << std::endl;
    bool completed = false;
    bool gotError = false;
    std::mutex mtx;

    streamOps.asyncStreamRead(
        "non_existent_stream_file.txt", 4096,
        [](AsyncResult<std::string>) {
            // chunk callback - won't be called for non-existent file
        },
        [&](AsyncResult<void> result) {
            std::lock_guard<std::mutex> lock(mtx);
            gotError = !result.success;
            if (!result.success) {
                std::cout << "  Expected error: " << result.error_message
                          << std::endl;
            }
            completed = true;
        });

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock(mtx);
        if (completed) break;
    }

    std::cout << "  Error handled correctly: "
              << (gotError ? "YES" : "NO") << std::endl;

#ifdef ATOM_USE_ASIO
    wg.reset();
    ioCtx.stop();
#endif
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n  Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "stream_test_medium.txt", "stream_test_large.bin",
        "stream_output.txt",     "stream_processed_output.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "  Atom I/O Async Streaming Examples" << std::endl;
        std::cout << "====================================" << std::endl;

        createTestFiles();

        demonstrateStreamRead();
        demonstrateStreamWrite();
        demonstrateChunkSizeComparison();
        demonstrateStreamPipeline();
        demonstrateErrorHandling();

        cleanup();

        std::cout
            << "\n  All streaming operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "  Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
