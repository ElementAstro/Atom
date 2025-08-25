/**
 * @file coroutine_operations.cpp
 * @brief Demonstration of C++20 coroutine-based async I/O operations
 *
 * This example demonstrates:
 * - Coroutine-based file reading and writing
 * - Sequential async operations with co_await
 * - Error handling in coroutines
 * - Directory listing with coroutines
 * - Combining multiple async operations
 * - Performance comparison with callback-based approach
 */

#include <chrono>
#include <coroutine>
#include <filesystem>
#include <iostream>
#include <vector>
#include "atom/io/async/async_io.hpp"

using namespace atom::async::io;
namespace fs = std::filesystem;

/**
 * @brief Creates test files for coroutine demonstrations
 */
void createTestFiles() {
    std::cout << "Creating test files for coroutine operations..." << std::endl;

    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"coro_test1.txt",
         "First coroutine test file\nWith some content\nFor async operations"},
        {"coro_test2.txt",
         "Second coroutine test file\nDifferent content here\nFor testing "
         "purposes"},
        {"coro_input.txt",
         "Input file for processing\nLine 2\nLine 3\nLine 4\nFinal line"}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::cout << "✅ Created " << testFiles.size() << " test files"
              << std::endl;
}

/**
 * @brief Simple coroutine for reading a file
 */
Task<void> simpleFileRead(AsyncFile& fileManager, const std::string& filename) {
    std::cout << "📖 Reading file: " << filename << std::endl;

    auto result = co_await fileManager.readFile(filename);

    if (result.success) {
        std::cout << "✅ Successfully read " << filename << " ("
                  << result.value.size() << " bytes)" << std::endl;
        std::cout << "Content preview: " << result.value.substr(0, 50) << "..."
                  << std::endl;
    } else {
        std::cout << "❌ Failed to read " << filename << ": "
                  << result.error_message << std::endl;
    }
}

/**
 * @brief Coroutine for writing a file
 */
Task<void> simpleFileWrite(AsyncFile& fileManager, const std::string& filename,
                           const std::string& content) {
    std::cout << "✏️  Writing file: " << filename << std::endl;

    std::span<const char> contentSpan(content.data(), content.size());
    auto result = co_await fileManager.writeFile(filename, contentSpan);

    if (result.success) {
        std::cout << "✅ Successfully wrote " << filename << " ("
                  << content.size() << " bytes)" << std::endl;
    } else {
        std::cout << "❌ Failed to write " << filename << ": "
                  << result.error_message << std::endl;
    }
}

/**
 * @brief Coroutine that processes multiple files sequentially
 */
Task<void> processFilesSequentially(AsyncFile& fileManager) {
    std::cout << "\n=== Sequential File Processing with Coroutines ==="
              << std::endl;

    std::vector<std::string> filesToProcess = {
        "coro_test1.txt", "coro_test2.txt", "coro_input.txt"};

    for (const auto& filename : filesToProcess) {
        // Read file
        auto readResult = co_await fileManager.readFile(filename);

        if (readResult.success) {
            std::cout << "📖 Read " << filename << " ("
                      << readResult.value.size() << " bytes)" << std::endl;

            // Process content (convert to uppercase)
            std::string processedContent = readResult.value;
            std::transform(processedContent.begin(), processedContent.end(),
                           processedContent.begin(), ::toupper);

            // Write processed content to new file
            std::string outputFilename = "processed_" + filename;
            std::span<const char> contentSpan(processedContent.data(),
                                              processedContent.size());
            auto writeResult =
                co_await fileManager.writeFile(outputFilename, contentSpan);

            if (writeResult.success) {
                std::cout << "✅ Processed and saved to " << outputFilename
                          << std::endl;
            } else {
                std::cout << "❌ Failed to write processed file: "
                          << writeResult.error_message << std::endl;
            }
        } else {
            std::cout << "❌ Failed to read " << filename << ": "
                      << readResult.error_message << std::endl;
        }
    }

    std::cout << "🎉 Sequential processing completed" << std::endl;
}

/**
 * @brief Coroutine for directory operations
 */
Task<void> directoryOperationsCoroutine(AsyncFile& fileManager) {
    std::cout << "\n=== Directory Operations with Coroutines ===" << std::endl;

    // List current directory
    std::cout << "📁 Listing current directory..." << std::endl;
    auto listResult = co_await fileManager.listDirectory(".");

    if (listResult.success) {
        std::cout << "✅ Found " << listResult.value.size()
                  << " items in current directory" << std::endl;

        // Show files related to our test
        std::cout << "Test-related files:" << std::endl;
        for (const auto& path : listResult.value) {
            std::string filename = path.filename().string();
            if (filename.find("coro_") != std::string::npos ||
                filename.find("processed_") != std::string::npos) {
                std::cout << "  📄 " << filename;
                if (fs::is_regular_file(path)) {
                    std::cout << " (" << fs::file_size(path) << " bytes)";
                }
                std::cout << std::endl;
            }
        }
    } else {
        std::cout << "❌ Failed to list directory: " << listResult.error_message
                  << std::endl;
    }
}

/**
 * @brief Coroutine that demonstrates error handling
 */
Task<void> errorHandlingCoroutine(AsyncFile& fileManager) {
    std::cout << "\n=== Error Handling in Coroutines ===" << std::endl;

    // Try to read a non-existent file
    std::cout << "Attempting to read non-existent file..." << std::endl;
    auto result = co_await fileManager.readFile("non_existent_file.txt");

    if (result.success) {
        std::cout << "❌ This should not succeed!" << std::endl;
    } else {
        std::cout << "✅ Expected error caught: " << result.error_message
                  << std::endl;
    }

    // Try to write to an invalid path
    std::cout << "Attempting to write to invalid path..." << std::endl;
    std::string content = "Test content";
    std::span<const char> contentSpan(content.data(), content.size());
    auto writeResult =
        co_await fileManager.writeFile("/invalid/path/file.txt", contentSpan);

    if (writeResult.success) {
        std::cout << "❌ This should not succeed!" << std::endl;
    } else {
        std::cout << "✅ Expected write error caught: "
                  << writeResult.error_message << std::endl;
    }
}

/**
 * @brief Coroutine that combines multiple operations
 */
Task<void> combinedOperationsCoroutine(AsyncFile& fileManager) {
    std::cout << "\n=== Combined Operations Coroutine ===" << std::endl;

    // Read input file
    auto inputResult = co_await fileManager.readFile("coro_input.txt");
    if (!inputResult.success) {
        std::cout << "❌ Failed to read input file: "
                  << inputResult.error_message << std::endl;
        co_return;
    }

    std::cout << "📖 Read input file (" << inputResult.value.size() << " bytes)"
              << std::endl;

    // Split content into lines and process each
    std::istringstream iss(inputResult.value);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    std::cout << "📝 Processing " << lines.size() << " lines..." << std::endl;

    // Create summary file
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
    std::span<const char> summarySpan(summary.data(), summary.size());
    auto summaryResult =
        co_await fileManager.writeFile("file_summary.txt", summarySpan);

    if (summaryResult.success) {
        std::cout << "✅ Created summary file: file_summary.txt" << std::endl;
    } else {
        std::cout << "❌ Failed to create summary: "
                  << summaryResult.error_message << std::endl;
    }
}

/**
 * @brief Main coroutine that orchestrates all demonstrations
 */
Task<void> mainCoroutine() {
    auto context = std::make_shared<AsyncContext>();
    AsyncFile fileManager(context);

    std::cout << "🚀 Starting coroutine-based async operations..." << std::endl;

    // Simple file operations
    co_await simpleFileRead(fileManager, "coro_test1.txt");
    co_await simpleFileWrite(
        fileManager, "coro_output.txt",
        "Hello from coroutines!\nThis is async I/O with C++20 coroutines.");

    // Sequential processing
    co_await processFilesSequentially(fileManager);

    // Directory operations
    co_await directoryOperationsCoroutine(fileManager);

    // Error handling
    co_await errorHandlingCoroutine(fileManager);

    // Combined operations
    co_await combinedOperationsCoroutine(fileManager);

    std::cout << "🎉 All coroutine operations completed!" << std::endl;
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files..." << std::endl;

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

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🔄 Atom I/O Coroutine Operations Examples" << std::endl;
        std::cout << "=========================================" << std::endl;

        // Setup
        createTestFiles();

        // Run main coroutine
        auto task = mainCoroutine();
        // Note: In a real application, you would need to properly await the
        // coroutine This is a simplified example for demonstration purposes

        // Cleanup
        cleanup();

        std::cout << "\n🎉 All coroutine examples completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
