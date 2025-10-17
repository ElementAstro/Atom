/**
 * @file directory_stack.cpp
 * @brief Comprehensive demonstration of directory stack operations (pushd/popd)
 *
 * This example demonstrates:
 * - Basic pushd/popd operations
 * - Directory stack management
 * - Stack persistence (save/load)
 * - Error handling for invalid directories
 * - Stack inspection and manipulation
 * - Cross-platform directory handling
 */

#include <filesystem>
#include <iostream>
#include "atom/io/filesystem/pushd.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test directories for demonstration
 */
void createTestDirectories() {
    std::cout << "Creating test directories..." << std::endl;

    try {
        fs::create_directories("test_dirs/dir1");
        fs::create_directories("test_dirs/dir2/subdir");
        fs::create_directories("test_dirs/dir3");
        std::cout << "✅ Test directories created successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to create test directories: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Cleans up test directories
 */
void cleanupTestDirectories() {
    try {
        if (fs::exists("test_dirs")) {
            fs::remove_all("test_dirs");
            std::cout << "🧹 Test directories cleaned up" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "⚠️  Warning: Failed to cleanup test directories: "
                  << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates basic directory stack operations
 */
void demonstrateBasicOperations() {
    std::cout << "\n=== Basic Directory Stack Operations ===" << std::endl;

    atom::io::DirectoryStack dirStack;

    // Show initial state
    std::cout << "Initial directory: " << fs::current_path() << std::endl;
    std::cout << "Stack size: " << dirStack.size() << std::endl;
    std::cout << "Is empty: " << (dirStack.isEmpty() ? "Yes" : "No")
              << std::endl;

    // Push to test directories
    std::cout << "\n1. Pushing to test_dirs/dir1..." << std::endl;
    try {
        dirStack.pushd("test_dirs/dir1");
        std::cout << "✅ Changed to: " << fs::current_path() << std::endl;
        std::cout << "Stack size: " << dirStack.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to pushd: " << e.what() << std::endl;
    }

    // Push to another directory
    std::cout << "\n2. Pushing to ../dir2..." << std::endl;
    try {
        dirStack.pushd("../dir2");
        std::cout << "✅ Changed to: " << fs::current_path() << std::endl;
        std::cout << "Stack size: " << dirStack.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to pushd: " << e.what() << std::endl;
    }

    // Push to subdirectory
    std::cout << "\n3. Pushing to subdir..." << std::endl;
    try {
        dirStack.pushd("subdir");
        std::cout << "✅ Changed to: " << fs::current_path() << std::endl;
        std::cout << "Stack size: " << dirStack.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to pushd: " << e.what() << std::endl;
    }

    // Show stack contents
    std::cout << "\n4. Current stack contents:" << std::endl;
    auto stackList = dirStack.dirs();
    for (size_t i = 0; i < stackList.size(); ++i) {
        std::cout << "  [" << i << "] " << stackList[i] << std::endl;
    }

    // Peek at top
    if (!dirStack.isEmpty()) {
        auto top = dirStack.peek();
        std::cout << "Top of stack: " << top << std::endl;
    }

    // Pop back through directories
    std::cout << "\n5. Popping back through directories..." << std::endl;
    while (!dirStack.isEmpty()) {
        try {
            dirStack.popd();
            std::cout << "✅ Popped to: " << fs::current_path() << std::endl;
            std::cout << "Stack size: " << dirStack.size() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ Failed to popd: " << e.what() << std::endl;
            break;
        }
    }
}

/**
 * @brief Demonstrates stack persistence operations
 */
void demonstrateStackPersistence() {
    std::cout << "\n=== Stack Persistence Operations ===" << std::endl;

    atom::io::DirectoryStack dirStack;
    const std::string stackFile = "directory_stack.txt";

    // Build up a stack
    std::cout << "Building directory stack..." << std::endl;
    try {
        dirStack.pushd("test_dirs/dir1");
        dirStack.pushd("../dir2");
        dirStack.pushd("../dir3");

        std::cout << "Current stack size: " << dirStack.size() << std::endl;

        // Save stack to file
        std::cout << "\nSaving stack to file: " << stackFile << std::endl;
        dirStack.saveStackToFile(stackFile);
        std::cout << "✅ Stack saved successfully" << std::endl;

        // Clear the stack
        std::cout << "\nClearing current stack..." << std::endl;
        while (!dirStack.isEmpty()) {
            dirStack.popd();
        }
        std::cout << "Stack size after clearing: " << dirStack.size()
                  << std::endl;

        // Load stack from file
        std::cout << "\nLoading stack from file: " << stackFile << std::endl;
        dirStack.loadStackFromFile(stackFile);
        std::cout << "✅ Stack loaded successfully" << std::endl;
        std::cout << "Stack size after loading: " << dirStack.size()
                  << std::endl;

        // Show loaded stack contents
        std::cout << "\nLoaded stack contents:" << std::endl;
        auto stackList = dirStack.dirs();
        for (size_t i = 0; i < stackList.size(); ++i) {
            std::cout << "  [" << i << "] " << stackList[i] << std::endl;
        }

        // Clean up stack
        while (!dirStack.isEmpty()) {
            dirStack.popd();
        }

        // Clean up file
        if (fs::exists(stackFile)) {
            fs::remove(stackFile);
            std::cout << "🧹 Stack file cleaned up" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Error in stack persistence demo: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Demonstrates error handling scenarios
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Scenarios ===" << std::endl;

    atom::io::DirectoryStack dirStack;

    // Try to push to non-existent directory
    std::cout << "1. Attempting to push to non-existent directory..."
              << std::endl;
    try {
        dirStack.pushd("non_existent_directory");
        std::cout << "❌ This should not succeed!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Expected error caught: " << e.what() << std::endl;
    }

    // Try to pop from empty stack
    std::cout << "\n2. Attempting to pop from empty stack..." << std::endl;
    try {
        dirStack.popd();
        std::cout << "❌ This should not succeed!" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Expected error caught: " << e.what() << std::endl;
    }

    // Try to peek empty stack
    std::cout << "\n3. Attempting to peek empty stack..." << std::endl;
    if (dirStack.isEmpty()) {
        std::cout << "✅ Stack is empty, peek would return empty path"
                  << std::endl;
    }
}

int main() {
    try {
        std::cout << "📁 Atom I/O Directory Stack Examples" << std::endl;
        std::cout << "====================================" << std::endl;

        // Setup test environment
        createTestDirectories();

        // Store original directory to restore later
        auto originalDir = fs::current_path();

        // Run demonstrations
        demonstrateBasicOperations();
        demonstrateStackPersistence();
        demonstrateErrorHandling();

        // Restore original directory
        fs::current_path(originalDir);
        std::cout << "\n🔄 Restored to original directory: "
                  << fs::current_path() << std::endl;

        // Cleanup
        cleanupTestDirectories();

        std::cout << "\n🎉 All directory stack examples completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanupTestDirectories();
        return 1;
    }
}
