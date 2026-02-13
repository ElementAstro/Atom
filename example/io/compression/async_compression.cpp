/**
 * @file async_compression.cpp
 * @brief Demonstration of asynchronous compression and decompression operations
 *
 * This example demonstrates:
 * - SingleFileCompressor for async file compression (ASIO-only)
 * - DirectoryCompressor for async folder compression (ASIO-only)
 * - SingleFileDecompressor for async file decompression (ASIO-only)
 * - DirectoryDecompressor for async folder decompression (ASIO-only)
 * - Async ZIP operations: list, exists, remove, size (ASIO-only)
 * - Error handling in async compression
 *
 * @note All async compression classes require ASIO. Without ASIO, this example
 *       prints a message and exits.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include "atom/io/async/async_compress.hpp"
#include "atom/io/compression/compress.hpp"

namespace fs = std::filesystem;

#ifdef ATOM_USE_ASIO

using namespace atom::io::async;

/**
 * @brief Creates test files for async compression demonstration
 */
void createAsyncTestFiles() {
    std::cout << "Creating test files for async compression..." << std::endl;

    std::vector<std::pair<std::string, size_t>> testFiles = {
        {"async_comp_small.txt", 1024},
        {"async_comp_medium.txt", 50 * 1024},
        {"async_comp_large.txt", 200 * 1024}};

    for (const auto& [filename, size] : testFiles) {
        std::ofstream file(filename);
        std::string pattern = "Async compression test pattern data. ";
        size_t written = 0;
        while (written < size) {
            size_t toWrite = std::min(pattern.size(), size - written);
            file.write(pattern.c_str(), toWrite);
            written += toWrite;
        }
        file.close();
        std::cout << "  Created " << filename << " (" << size << " bytes)"
                  << std::endl;
    }

    fs::create_directories("async_comp_folder/subdir1");
    fs::create_directories("async_comp_folder/subdir2");
    fs::copy_file("async_comp_small.txt", "async_comp_folder/file1.txt",
                  fs::copy_options::overwrite_existing);
    fs::copy_file("async_comp_medium.txt",
                  "async_comp_folder/subdir1/file2.txt",
                  fs::copy_options::overwrite_existing);
    {
        std::ofstream f("async_comp_folder/subdir2/file3.txt");
        f << "Sub-directory file content for compression testing.";
        f.close();
    }

    std::cout << "  Created async_comp_folder/ with subdirectories"
              << std::endl;
}

/**
 * @brief Demonstrates SingleFileCompressor
 *
 * Constructor: SingleFileCompressor(io_context, input_file, output_file)
 * Method: start(handler) where handler is void(error_code, size_t)
 */
void demonstrateSingleFileCompressor() {
    std::cout << "\n=== SingleFileCompressor ===" << std::endl;

    const fs::path inputFile = "async_comp_medium.txt";
    const fs::path outputFile = "async_comp_medium.txt.gz";
    auto originalSize = fs::file_size(inputFile);

    std::cout << "Compressing " << inputFile << " (" << originalSize
              << " bytes)..." << std::endl;

    asio::io_context ioContext;
    auto workGuard = asio::make_work_guard(ioContext);
    std::jthread ioThread([&]() { ioContext.run(); });

    auto start = std::chrono::high_resolution_clock::now();

    SingleFileCompressor compressor(ioContext, inputFile, outputFile);
    compressor.start(
        [&](const std::error_code& ec, std::size_t bytesProcessed) {
            if (!ec) {
                auto compressedSize = fs::file_size(outputFile);
                double ratio = static_cast<double>(compressedSize) /
                               static_cast<double>(originalSize) * 100.0;
                std::cout << "  Compressed: " << compressedSize << " bytes ("
                          << ratio << "%), processed " << bytesProcessed
                          << " bytes" << std::endl;
            } else {
                std::cerr << "  Compression error: " << ec.message()
                          << std::endl;
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "  Time: " << duration.count() << "ms" << std::endl;

    workGuard.reset();
    ioContext.stop();
}

/**
 * @brief Demonstrates DirectoryCompressor
 *
 * Constructor: DirectoryCompressor(io_context, input_dir, output_file)
 * Method: start(handler) where handler is void(error_code, size_t)
 */
void demonstrateDirectoryCompressor() {
    std::cout << "\n=== DirectoryCompressor ===" << std::endl;

    const fs::path inputDir = "async_comp_folder";
    const fs::path outputFile = "async_comp_folder.gz";

    std::cout << "Compressing directory " << inputDir << "..." << std::endl;

    asio::io_context ioContext;
    auto workGuard = asio::make_work_guard(ioContext);
    std::jthread ioThread([&]() { ioContext.run(); });

    DirectoryCompressor compressor(ioContext, inputDir, outputFile);
    compressor.start(
        [&](const std::error_code& ec, std::size_t bytesProcessed) {
            if (!ec) {
                auto compressedSize = fs::file_size(outputFile);
                std::cout << "  Directory compressed: " << compressedSize
                          << " bytes, processed " << bytesProcessed
                          << " bytes" << std::endl;
            } else {
                std::cerr << "  Directory compression error: " << ec.message()
                          << std::endl;
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    workGuard.reset();
    ioContext.stop();
}

/**
 * @brief Demonstrates SingleFileDecompressor
 *
 * Constructor: SingleFileDecompressor(io_context, input_file, output_folder)
 * Method: start(handler) where handler is void(error_code, size_t)
 */
void demonstrateSingleFileDecompressor() {
    std::cout << "\n=== SingleFileDecompressor ===" << std::endl;

    const fs::path compressedFile = "async_comp_medium.txt.gz";
    const fs::path outputFolder = "async_decomp_output";

    if (!fs::exists(compressedFile)) {
        std::cout << "  No compressed file found, skipping" << std::endl;
        return;
    }

    fs::create_directories(outputFolder);
    std::cout << "Decompressing " << compressedFile << " to " << outputFolder
              << "..." << std::endl;

    asio::io_context ioContext;
    auto workGuard = asio::make_work_guard(ioContext);
    std::jthread ioThread([&]() { ioContext.run(); });

    SingleFileDecompressor decompressor(ioContext, compressedFile,
                                        outputFolder);
    decompressor.start(
        [&](const std::error_code& ec, std::size_t bytesProcessed) {
            if (!ec) {
                std::cout << "  Decompressed successfully, processed "
                          << bytesProcessed << " bytes" << std::endl;
            } else {
                std::cerr << "  Decompression error: " << ec.message()
                          << std::endl;
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    workGuard.reset();
    ioContext.stop();
}

/**
 * @brief Demonstrates DirectoryDecompressor
 *
 * Constructor: DirectoryDecompressor(io_context, input_dir, output_folder)
 * Method: start(handler) where handler is void(error_code, size_t)
 */
void demonstrateDirectoryDecompressor() {
    std::cout << "\n=== DirectoryDecompressor ===" << std::endl;

    const fs::path compressedDir = "async_comp_folder";
    const fs::path outputFolder = "async_decomp_dir_output";

    // For DirectoryDecompressor, we need a directory of .gz files
    // Let's compress a few files first then decompress them
    fs::create_directories("async_gz_files");
    {
        asio::io_context ioCtx;
        auto wg = asio::make_work_guard(ioCtx);
        std::jthread t([&]() { ioCtx.run(); });

        SingleFileCompressor comp(ioCtx, "async_comp_small.txt",
                                  "async_gz_files/small.gz");
        comp.start([](const std::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "  Prep compression error: " << ec.message()
                          << std::endl;
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        wg.reset();
        ioCtx.stop();
    }

    fs::create_directories(outputFolder);
    std::cout << "Decompressing directory of .gz files..." << std::endl;

    asio::io_context ioContext;
    auto workGuard = asio::make_work_guard(ioContext);
    std::jthread ioThread([&]() { ioContext.run(); });

    DirectoryDecompressor decompressor(ioContext, "async_gz_files",
                                       outputFolder);
    decompressor.start(
        [&](const std::error_code& ec, std::size_t bytesProcessed) {
            if (!ec) {
                int fileCount = 0;
                if (fs::exists(outputFolder)) {
                    for (const auto& entry :
                         fs::recursive_directory_iterator(outputFolder)) {
                        if (entry.is_regular_file()) {
                            fileCount++;
                        }
                    }
                }
                std::cout << "  Extracted " << fileCount << " files, processed "
                          << bytesProcessed << " bytes" << std::endl;
            } else {
                std::cerr << "  Directory decompression error: " << ec.message()
                          << std::endl;
            }
        });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    workGuard.reset();
    ioContext.stop();
}

/**
 * @brief Demonstrates async ZIP operations
 *
 * - ListFilesInZip(io_context, zip_file) -> start(), getFileList()
 * - FileExistsInZip(io_context, zip_file, file_name) -> start(), found()
 * - GetZipFileSize(io_context, zip_file) -> start(), getSizeValue()
 * - RemoveFileFromZip(io_context, zip_file, file_name) -> start(),
 * isSuccessful()
 */
void demonstrateAsyncZipOperations() {
    std::cout << "\n=== Async ZIP Operations ===" << std::endl;

    const std::string zipFile = "async_zip_test.zip";

    auto zipResult = atom::io::createZip("async_comp_folder", zipFile);
    if (!zipResult.success) {
        std::cout << "  Failed to create test ZIP: " << zipResult.error_message
                  << std::endl;
        return;
    }
    std::cout << "Created test ZIP: " << zipFile << std::endl;

    asio::io_context ioContext;
    auto workGuard = asio::make_work_guard(ioContext);
    std::jthread ioThread([&]() { ioContext.run(); });

    // 1. List files in ZIP
    std::cout << "\n1. Listing files in ZIP asynchronously..." << std::endl;
    {
        ListFilesInZip listOp(ioContext, zipFile);
        listOp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        auto files = listOp.getFileList();
        std::cout << "  Found " << files.size() << " files:" << std::endl;
        for (const auto& f : files) {
            std::cout << "    " << f << std::endl;
        }
    }

    // 2. Check file existence in ZIP
    std::cout << "\n2. Checking file existence in ZIP..." << std::endl;
    {
        FileExistsInZip existsOp(ioContext, zipFile, "file1.txt");
        existsOp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  file1.txt exists: "
                  << (existsOp.found() ? "YES" : "NO") << std::endl;
    }

    // 3. Get ZIP file size
    std::cout << "\n3. Getting ZIP file size asynchronously..." << std::endl;
    {
        GetZipFileSize sizeOp(ioContext, zipFile);
        sizeOp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  ZIP file size: " << sizeOp.getSizeValue() << " bytes"
                  << std::endl;
    }

    // 4. Remove file from ZIP
    std::cout << "\n4. Removing file from ZIP asynchronously..." << std::endl;
    {
        RemoveFileFromZip removeOp(ioContext, zipFile, "file1.txt");
        removeOp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  Removal successful: "
                  << (removeOp.isSuccessful() ? "YES" : "NO") << std::endl;
    }

    // Verify removal
    {
        FileExistsInZip existsOp(ioContext, zipFile, "file1.txt");
        existsOp.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  file1.txt after removal: "
                  << (existsOp.found() ? "still exists" : "removed") << std::endl;
    }

    workGuard.reset();
    ioContext.stop();
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n  Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "async_comp_small.txt",           "async_comp_medium.txt",
        "async_comp_large.txt",           "async_comp_medium.txt.gz",
        "async_comp_folder.gz",           "async_zip_test.zip",
        "output.gz",                      "invalid_compressed.gz",
        "invalid_output.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    for (const auto& dir : {"async_comp_folder", "async_decomp_output",
                             "async_decomp_dir_output", "async_gz_files"}) {
        if (fs::exists(dir)) {
            fs::remove_all(dir);
        }
    }

    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "  Atom I/O Async Compression Examples" << std::endl;
        std::cout << "======================================" << std::endl;

        createAsyncTestFiles();

        demonstrateSingleFileCompressor();
        demonstrateDirectoryCompressor();
        demonstrateSingleFileDecompressor();
        demonstrateDirectoryDecompressor();
        demonstrateAsyncZipOperations();

        cleanup();

        std::cout
            << "\n  All async compression operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "  Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}

#else  // !ATOM_USE_ASIO

int main() {
    std::cout << "Async compression examples require ASIO support.\n"
              << "Build with -DATOM_USE_ASIO=ON to enable.\n";
    return 0;
}

#endif  // ATOM_USE_ASIO
