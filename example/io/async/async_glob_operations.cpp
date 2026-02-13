/**
 * @file async_glob_operations.cpp
 * @brief Demonstration of asynchronous glob pattern matching operations
 *
 * This example demonstrates:
 * - Asynchronous glob pattern matching with callbacks
 * - Synchronous glob operations (glob_sync)
 * - Recursive pattern matching
 * - Directory-only filtering
 * - Pattern filtering with filter()
 * - Error handling in async glob operations
 *
 * @note AsyncGlob requires ASIO. Without ASIO, this example prints a message
 *       and exits.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <vector>
#include "atom/io/async/async_glob.hpp"

namespace fs = std::filesystem;

#ifdef ATOM_USE_ASIO

using namespace atom::io;

/**
 * @brief Creates a complex directory structure for glob testing
 */
void createGlobTestStructure() {
    std::cout << "Creating test structure for async glob operations..."
              << std::endl;

    fs::create_directories("async_glob_test/src/cpp");
    fs::create_directories("async_glob_test/src/python");
    fs::create_directories("async_glob_test/src/javascript");
    fs::create_directories("async_glob_test/docs/api");
    fs::create_directories("async_glob_test/docs/user");
    fs::create_directories("async_glob_test/tests/unit");
    fs::create_directories("async_glob_test/tests/integration");
    fs::create_directories("async_glob_test/build/debug");
    fs::create_directories("async_glob_test/build/release");

    std::vector<std::string> testFiles = {
        "async_glob_test/main.cpp",
        "async_glob_test/utils.hpp",
        "async_glob_test/config.json",
        "async_glob_test/README.md",
        "async_glob_test/src/cpp/parser.cpp",
        "async_glob_test/src/cpp/parser.hpp",
        "async_glob_test/src/cpp/lexer.cpp",
        "async_glob_test/src/python/script.py",
        "async_glob_test/src/python/module.py",
        "async_glob_test/src/python/__init__.py",
        "async_glob_test/src/javascript/app.js",
        "async_glob_test/src/javascript/utils.js",
        "async_glob_test/docs/api/reference.md",
        "async_glob_test/docs/api/examples.md",
        "async_glob_test/docs/user/guide.md",
        "async_glob_test/docs/user/tutorial.md",
        "async_glob_test/tests/unit/test_parser.cpp",
        "async_glob_test/tests/unit/test_lexer.cpp",
        "async_glob_test/tests/integration/test_full.cpp",
        "async_glob_test/build/debug/main.o",
        "async_glob_test/build/release/main.o"};

    for (const auto& file : testFiles) {
        std::ofstream outFile(file);
        outFile << "Test content for " << fs::path(file).filename()
                << std::endl;
        outFile.close();
    }

    std::cout << "  Created test structure with " << testFiles.size()
              << " files" << std::endl;
}

/**
 * @brief Demonstrates async glob with callback
 */
void demonstrateAsyncGlob() {
    std::cout << "\n=== Async Glob with Callbacks ===" << std::endl;

    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncGlob asyncGlob(ioCtx);

    // Find all .cpp files recursively
    std::cout << "\n1. Finding all .cpp files asynchronously..." << std::endl;
    std::promise<std::vector<fs::path>> cppPromise;

    auto start = std::chrono::high_resolution_clock::now();
    asyncGlob.glob(
        "async_glob_test/**/*.cpp",
        [&](std::vector<fs::path> results) {
            cppPromise.set_value(std::move(results));
        },
        true, false);

    auto cppFiles = cppPromise.get_future().get();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "  Found " << cppFiles.size() << " .cpp files in "
              << duration.count() << " us" << std::endl;
    for (const auto& file : cppFiles) {
        std::cout << "    " << file << std::endl;
    }

    // Find all Python files
    std::cout << "\n2. Finding all .py files asynchronously..." << std::endl;
    std::promise<std::vector<fs::path>> pyPromise;

    asyncGlob.glob(
        "async_glob_test/**/*.py",
        [&](std::vector<fs::path> results) {
            pyPromise.set_value(std::move(results));
        },
        true, false);

    auto pyFiles = pyPromise.get_future().get();
    std::cout << "  Found " << pyFiles.size() << " .py files" << std::endl;
    for (const auto& file : pyFiles) {
        std::cout << "    " << file << std::endl;
    }

    wg.reset();
    ioCtx.stop();
}

/**
 * @brief Demonstrates synchronous glob (glob_sync)
 */
void demonstrateSyncGlob() {
    std::cout << "\n=== Synchronous Glob ===" << std::endl;

    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncGlob asyncGlob(ioCtx);

    // Find all .md files synchronously
    std::cout << "Finding all .md files synchronously..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    auto mdFiles =
        asyncGlob.glob_sync("async_glob_test/**/*.md", true, false);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "  Found " << mdFiles.size() << " .md files in "
              << duration.count() << " us" << std::endl;
    for (const auto& file : mdFiles) {
        std::cout << "    " << file << std::endl;
    }

    wg.reset();
    ioCtx.stop();
}

/**
 * @brief Demonstrates directory-only glob
 */
void demonstrateDirectoryGlob() {
    std::cout << "\n=== Directory-Only Glob ===" << std::endl;

    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncGlob asyncGlob(ioCtx);

    std::cout << "Finding all directories..." << std::endl;
    std::promise<std::vector<fs::path>> dirPromise;

    asyncGlob.glob(
        "async_glob_test/**/*",
        [&](std::vector<fs::path> results) {
            dirPromise.set_value(std::move(results));
        },
        true, true);

    auto directories = dirPromise.get_future().get();
    std::cout << "  Found " << directories.size() << " directories"
              << std::endl;
    for (const auto& dir : directories) {
        std::cout << "    " << dir << std::endl;
    }

    wg.reset();
    ioCtx.stop();
}

/**
 * @brief Demonstrates filter functionality
 */
void demonstrateFilter() {
    std::cout << "\n=== Pattern Filter ===" << std::endl;

    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncGlob asyncGlob(ioCtx);

    // Get all files, then filter
    auto allFiles =
        asyncGlob.glob_sync("async_glob_test/**/*", true, false);

    std::cout << "Total files: " << allFiles.size() << std::endl;

    // Filter for .cpp files
    auto cppOnly = asyncGlob.filter(
        std::span<const fs::path>(allFiles.data(), allFiles.size()), "*.cpp");
    std::cout << "  Filtered *.cpp: " << cppOnly.size() << " files"
              << std::endl;

    // Filter for .js files
    auto jsOnly = asyncGlob.filter(
        std::span<const fs::path>(allFiles.data(), allFiles.size()), "*.js");
    std::cout << "  Filtered *.js: " << jsOnly.size() << " files" << std::endl;

    wg.reset();
    ioCtx.stop();
}

/**
 * @brief Demonstrates error handling in async glob
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    asio::io_context ioCtx;
    auto wg = asio::make_work_guard(ioCtx);
    std::jthread ioThread([&]() { ioCtx.run(); });
    AsyncGlob asyncGlob(ioCtx);

    // Glob non-existent directory
    std::cout << "Testing glob on non-existent directory..." << std::endl;
    std::promise<std::vector<fs::path>> errorPromise;

    asyncGlob.glob(
        "non_existent_dir/**/*",
        [&](std::vector<fs::path> results) {
            errorPromise.set_value(std::move(results));
        },
        true, false);

    auto errorResults = errorPromise.get_future().get();
    std::cout << "  Graceful handling: Found " << errorResults.size()
              << " items (expected 0)" << std::endl;

    wg.reset();
    ioCtx.stop();
}

/**
 * @brief Cleans up test structure
 */
void cleanup() {
    std::cout << "\nCleaning up test structure..." << std::endl;
    if (fs::exists("async_glob_test")) {
        fs::remove_all("async_glob_test");
    }
    std::cout << "  Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "Atom I/O Async Glob Operations Examples" << std::endl;
        std::cout << "=======================================" << std::endl;

        createGlobTestStructure();

        demonstrateAsyncGlob();
        demonstrateSyncGlob();
        demonstrateDirectoryGlob();
        demonstrateFilter();
        demonstrateErrorHandling();

        cleanup();

        std::cout << "\nAll async glob operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}

#else  // !ATOM_USE_ASIO

int main() {
    std::cout << "Async glob examples require ASIO support.\n"
              << "Build with -DATOM_USE_ASIO=ON to enable.\n";
    return 0;
}

#endif  // ATOM_USE_ASIO
