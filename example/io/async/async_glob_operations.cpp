/**
 * @file async_glob_operations.cpp
 * @brief Demonstration of asynchronous glob pattern matching operations
 *
 * This example demonstrates:
 * - Asynchronous glob pattern matching with callbacks
 * - Coroutine-based glob operations
 * - Recursive pattern matching
 * - Directory-only filtering
 * - Performance comparison between sync and async glob
 * - Error handling in async glob operations
 */

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <vector>
#include "atom/io/async/async_glob.hpp"

using namespace atom::io;
namespace fs = std::filesystem;

/**
 * @brief Creates a complex directory structure for glob testing
 */
void createGlobTestStructure() {
    std::cout << "Creating test structure for async glob operations..."
              << std::endl;

    // Create directories
    fs::create_directories("async_glob_test/src/cpp");
    fs::create_directories("async_glob_test/src/python");
    fs::create_directories("async_glob_test/src/javascript");
    fs::create_directories("async_glob_test/docs/api");
    fs::create_directories("async_glob_test/docs/user");
    fs::create_directories("async_glob_test/tests/unit");
    fs::create_directories("async_glob_test/tests/integration");
    fs::create_directories("async_glob_test/build/debug");
    fs::create_directories("async_glob_test/build/release");

    // Create various test files
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

    std::cout << "✅ Created test structure with " << testFiles.size()
              << " files" << std::endl;
}

/**
 * @brief Demonstrates basic async glob operations with callbacks
 */
void demonstrateBasicAsyncGlob() {
    std::cout << "\n=== Basic Async Glob Operations ===" << std::endl;

    AsyncGlob asyncGlob;

    // Find all .cpp files
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

    std::cout << "✅ Found " << cppFiles.size() << " .cpp files in "
              << duration.count() << "μs" << std::endl;
    for (const auto& file : cppFiles) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Find all Python files
    std::cout << "\n2. Finding all .py files asynchronously..." << std::endl;
    std::promise<std::vector<fs::path>> pyPromise;

    start = std::chrono::high_resolution_clock::now();
    asyncGlob.glob(
        "async_glob_test/**/*.py",
        [&](std::vector<fs::path> results) {
            pyPromise.set_value(std::move(results));
        },
        true, false);

    auto pyFiles = pyPromise.get_future().get();
    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "✅ Found " << pyFiles.size() << " .py files in "
              << duration.count() << "μs" << std::endl;
    for (const auto& file : pyFiles) {
        std::cout << "  📄 " << file << std::endl;
    }
}

/**
 * @brief Demonstrates directory-only async glob operations
 */
void demonstrateDirectoryGlob() {
    std::cout << "\n=== Directory-Only Async Glob ===" << std::endl;

    AsyncGlob asyncGlob;

    // Find all directories
    std::cout << "Finding all directories asynchronously..." << std::endl;
    std::promise<std::vector<fs::path>> dirPromise;

    auto start = std::chrono::high_resolution_clock::now();
    asyncGlob.glob(
        "async_glob_test/**/*",
        [&](std::vector<fs::path> results) {
            dirPromise.set_value(std::move(results));
        },
        true, true);  // recursive=true, dironly=true

    auto directories = dirPromise.get_future().get();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "✅ Found " << directories.size() << " directories in "
              << duration.count() << "μs" << std::endl;
    for (const auto& dir : directories) {
        std::cout << "  📁 " << dir << std::endl;
    }
}

/**
 * @brief Demonstrates coroutine-based async glob operations
 */
void demonstrateCoroutineGlob() {
    std::cout << "\n=== Coroutine-Based Async Glob ===" << std::endl;

    AsyncGlob asyncGlob;

    // Note: This is a simplified example. In a real implementation,
    // you would need proper coroutine support and awaitable types.

    std::cout << "Using coroutine-style async glob (simulated)..." << std::endl;

    // Simulate coroutine-based operation
    std::promise<std::vector<fs::path>> coroPromise;

    auto start = std::chrono::high_resolution_clock::now();
    asyncGlob.glob(
        "async_glob_test/**/*.md",
        [&](std::vector<fs::path> results) {
            coroPromise.set_value(std::move(results));
        },
        true, false);

    auto mdFiles = coroPromise.get_future().get();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "✅ Found " << mdFiles.size() << " .md files in "
              << duration.count() << "μs" << std::endl;
    for (const auto& file : mdFiles) {
        std::cout << "  📄 " << file << std::endl;
    }
}

/**
 * @brief Demonstrates multiple pattern matching
 */
void demonstrateMultiplePatterns() {
    std::cout << "\n=== Multiple Pattern Async Glob ===" << std::endl;

    AsyncGlob asyncGlob;

    std::vector<std::string> patterns = {
        "async_glob_test/**/*.cpp", "async_glob_test/**/*.hpp",
        "async_glob_test/**/*.py", "async_glob_test/**/*.js"};

    std::cout << "Searching for multiple file types asynchronously..."
              << std::endl;

    // Process each pattern asynchronously
    std::vector<std::future<std::vector<fs::path>>> futures;
    std::vector<std::promise<std::vector<fs::path>>> promises(patterns.size());

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < patterns.size(); ++i) {
        futures.push_back(promises[i].get_future());

        asyncGlob.glob(
            patterns[i],
            [&promises, i](std::vector<fs::path> results) {
                promises[i].set_value(std::move(results));
            },
            true, false);
    }

    // Collect all results
    std::vector<fs::path> allResults;
    for (size_t i = 0; i < futures.size(); ++i) {
        auto results = futures[i].get();
        std::cout << "Pattern '" << patterns[i] << "': " << results.size()
                  << " matches" << std::endl;
        allResults.insert(allResults.end(), results.begin(), results.end());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "✅ Total matches: " << allResults.size() << " files in "
              << duration.count() << "μs" << std::endl;
}

/**
 * @brief Demonstrates performance comparison between sync and async glob
 */
void demonstratePerformanceComparison() {
    std::cout << "\n=== Performance Comparison ===" << std::endl;

    const std::string pattern = "async_glob_test/**/*";

    // Synchronous glob
    std::cout << "1. Synchronous glob..." << std::endl;
    auto syncStart = std::chrono::high_resolution_clock::now();
    auto syncResults = atom::io::glob(pattern, true, false);
    auto syncEnd = std::chrono::high_resolution_clock::now();
    auto syncDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        syncEnd - syncStart);

    std::cout << "✅ Sync: Found " << syncResults.size() << " items in "
              << syncDuration.count() << "μs" << std::endl;

    // Asynchronous glob
    std::cout << "\n2. Asynchronous glob..." << std::endl;
    AsyncGlob asyncGlob;
    std::promise<std::vector<fs::path>> asyncPromise;

    auto asyncStart = std::chrono::high_resolution_clock::now();
    asyncGlob.glob(
        pattern,
        [&](std::vector<fs::path> results) {
            asyncPromise.set_value(std::move(results));
        },
        true, false);

    auto asyncResults = asyncPromise.get_future().get();
    auto asyncEnd = std::chrono::high_resolution_clock::now();
    auto asyncDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        asyncEnd - asyncStart);

    std::cout << "✅ Async: Found " << asyncResults.size() << " items in "
              << asyncDuration.count() << "μs" << std::endl;

    // Compare results
    std::cout << "\nPerformance comparison:" << std::endl;
    std::cout << "  Sync time:  " << syncDuration.count() << "μs" << std::endl;
    std::cout << "  Async time: " << asyncDuration.count() << "μs" << std::endl;

    if (asyncDuration < syncDuration) {
        auto improvement = ((syncDuration - asyncDuration).count() * 100.0) /
                           syncDuration.count();
        std::cout << "  🚀 Async is " << improvement << "% faster" << std::endl;
    } else {
        auto overhead = ((asyncDuration - syncDuration).count() * 100.0) /
                        syncDuration.count();
        std::cout << "  ⚠️  Async has " << overhead
                  << "% overhead (expected for small datasets)" << std::endl;
    }
}

/**
 * @brief Demonstrates error handling in async glob
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling in Async Glob ===" << std::endl;

    AsyncGlob asyncGlob;

    // Try to glob a non-existent directory
    std::cout << "Testing glob on non-existent directory..." << std::endl;
    std::promise<std::vector<fs::path>> errorPromise;

    asyncGlob.glob(
        "non_existent_dir/**/*",
        [&](std::vector<fs::path> results) {
            errorPromise.set_value(std::move(results));
        },
        true, false);

    auto errorResults = errorPromise.get_future().get();
    std::cout << "✅ Graceful handling: Found " << errorResults.size()
              << " items (expected 0)" << std::endl;

    // Try invalid pattern
    std::cout << "\nTesting with complex pattern..." << std::endl;
    std::promise<std::vector<fs::path>> complexPromise;

    asyncGlob.glob(
        "async_glob_test/**/[abc]*.{cpp,hpp}",
        [&](std::vector<fs::path> results) {
            complexPromise.set_value(std::move(results));
        },
        true, false);

    auto complexResults = complexPromise.get_future().get();
    std::cout << "✅ Complex pattern handled: Found " << complexResults.size()
              << " items" << std::endl;
}

/**
 * @brief Cleans up test structure
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test structure..." << std::endl;

    if (fs::exists("async_glob_test")) {
        fs::remove_all("async_glob_test");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🔍 Atom I/O Async Glob Operations Examples" << std::endl;
        std::cout << "===========================================" << std::endl;

        // Setup
        createGlobTestStructure();

        // Run demonstrations
        demonstrateBasicAsyncGlob();
        demonstrateDirectoryGlob();
        demonstrateCoroutineGlob();
        demonstrateMultiplePatterns();
        demonstratePerformanceComparison();
        demonstrateErrorHandling();

        // Cleanup
        cleanup();

        std::cout << "\n🎉 All async glob operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
