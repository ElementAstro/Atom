/**
 * @file basic_file_operations.cpp
 * @brief Comprehensive demonstration of basic file and directory operations
 *
 * This example demonstrates:
 * - File creation, copying, moving, and deletion
 * - Directory creation, moving, and removal
 * - File existence and validation checks
 * - File size operations and metadata
 * - Symbolic link operations
 * - Cross-platform path handling
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include "atom/io/core/io.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test files for demonstration
 */
void createTestFiles() {
    std::cout << "Creating test files..." << std::endl;

    // Create test content
    std::ofstream file1("test_file1.txt");
    file1 << "This is test file 1\nWith multiple lines\nFor testing purposes";
    file1.close();

    std::ofstream file2("test_file2.txt");
    file2 << "This is test file 2\nWith different content\nFor copy/move "
             "operations";
    file2.close();

    std::cout << "✅ Test files created" << std::endl;
}

/**
 * @brief Demonstrates basic file operations
 */
void demonstrateFileOperations() {
    std::cout << "\n=== Basic File Operations ===" << std::endl;

    // File existence checks
    std::cout << "\n1. File existence checks:" << std::endl;
    std::cout << "test_file1.txt exists: "
              << (atom::io::isFileExists("test_file1.txt") ? "Yes" : "No")
              << std::endl;
    std::cout << "nonexistent.txt exists: "
              << (atom::io::isFileExists("nonexistent.txt") ? "Yes" : "No")
              << std::endl;

    // File size operations
    std::cout << "\n2. File size operations:" << std::endl;
    auto size1 = atom::io::fileSize("test_file1.txt");
    auto size2 = atom::io::getFileSize("test_file2.txt");
    std::cout << "test_file1.txt size: " << size1 << " bytes" << std::endl;
    std::cout << "test_file2.txt size: " << size2 << " bytes" << std::endl;

    // File copying
    std::cout << "\n3. File copying:" << std::endl;
    if (atom::io::copyFile("test_file1.txt", "copied_file.txt")) {
        std::cout << "✅ File copied successfully" << std::endl;
        std::cout << "Copied file size: "
                  << atom::io::fileSize("copied_file.txt") << " bytes"
                  << std::endl;
    } else {
        std::cout << "❌ File copy failed" << std::endl;
    }

    // File moving/renaming
    std::cout << "\n4. File moving/renaming:" << std::endl;
    if (atom::io::moveFile("test_file2.txt", "moved_file.txt")) {
        std::cout << "✅ File moved successfully" << std::endl;
        std::cout << "Original file exists: "
                  << (atom::io::isFileExists("test_file2.txt") ? "Yes" : "No")
                  << std::endl;
        std::cout << "Moved file exists: "
                  << (atom::io::isFileExists("moved_file.txt") ? "Yes" : "No")
                  << std::endl;
    } else {
        std::cout << "❌ File move failed" << std::endl;
    }

    // File renaming (alternative method)
    std::cout << "\n5. File renaming:" << std::endl;
    if (atom::io::renameFile("moved_file.txt", "renamed_file.txt")) {
        std::cout << "✅ File renamed successfully" << std::endl;
    } else {
        std::cout << "❌ File rename failed" << std::endl;
    }
}

/**
 * @brief Demonstrates directory operations
 */
void demonstrateDirectoryOperations() {
    std::cout << "\n=== Directory Operations ===" << std::endl;

    // Directory creation
    std::cout << "\n1. Directory creation:" << std::endl;
    if (atom::io::createDirectory("test_dir")) {
        std::cout << "✅ Directory created successfully" << std::endl;
    } else {
        std::cout << "❌ Directory creation failed" << std::endl;
    }

    // Directory existence check
    std::cout << "\n2. Directory existence check:" << std::endl;
    std::cout << "test_dir exists: "
              << (atom::io::isFolderExists("test_dir") ? "Yes" : "No")
              << std::endl;
    std::cout << "test_dir is empty: "
              << (atom::io::isFolderEmpty("test_dir") ? "Yes" : "No")
              << std::endl;

    // Recursive directory creation
    std::cout << "\n3. Recursive directory creation:" << std::endl;
    std::vector<std::string> subdirs = {"subdir1", "subdir2", "subdir3/nested"};
    if (atom::io::createDirectoriesRecursive("test_dir", subdirs)) {
        std::cout << "✅ Recursive directories created successfully"
                  << std::endl;
    } else {
        std::cout << "❌ Recursive directory creation failed" << std::endl;
    }

    // Check if directory is still empty
    std::cout << "test_dir is empty after subdirs: "
              << (atom::io::isFolderEmpty("test_dir") ? "Yes" : "No")
              << std::endl;

    // Directory moving
    std::cout << "\n4. Directory moving:" << std::endl;
    if (atom::io::moveDirectory("test_dir", "moved_test_dir")) {
        std::cout << "✅ Directory moved successfully" << std::endl;
        std::cout << "Original dir exists: "
                  << (atom::io::isFolderExists("test_dir") ? "Yes" : "No")
                  << std::endl;
        std::cout << "Moved dir exists: "
                  << (atom::io::isFolderExists("moved_test_dir") ? "Yes" : "No")
                  << std::endl;
    } else {
        std::cout << "❌ Directory move failed" << std::endl;
    }
}

/**
 * @brief Demonstrates path utilities
 */
void demonstratePathUtilities() {
    std::cout << "\n=== Path Utilities ===" << std::endl;

    // Path validation
    std::cout << "\n1. Path validation:" << std::endl;
    std::cout << "Valid file name 'test.txt': "
              << (atom::io::isFileNameValid("test.txt") ? "Yes" : "No")
              << std::endl;
    std::cout << "Valid file name 'test<>file.txt': "
              << (atom::io::isFileNameValid("test<>file.txt") ? "Yes" : "No")
              << std::endl;
    std::cout << "Valid folder name 'my_folder': "
              << (atom::io::isFolderNameValid("my_folder") ? "Yes" : "No")
              << std::endl;

    // Absolute path checking
    std::cout << "\n2. Absolute path checking:" << std::endl;
    std::cout << "Is 'test_file1.txt' absolute: "
              << (atom::io::isAbsolutePath("test_file1.txt") ? "Yes" : "No")
              << std::endl;
    auto currentPath = fs::current_path();
    std::cout << "Is current path absolute: "
              << (atom::io::isAbsolutePath(currentPath) ? "Yes" : "No")
              << std::endl;

    // Path conversion
    std::cout << "\n3. Path conversion:" << std::endl;
    std::string windowsPath = "C:\\Users\\test\\file.txt";
    std::string linuxPath = "/home/user/file.txt";

    std::cout << "Windows to Linux: " << windowsPath << " -> "
              << atom::io::convertToLinuxPath(windowsPath) << std::endl;
    std::cout << "Linux to Windows: " << linuxPath << " -> "
              << atom::io::convertToWindowsPath(linuxPath) << std::endl;

    // Path normalization
    std::cout << "\n4. Path normalization:" << std::endl;
    std::string messyPath = "./test/../test_file1.txt";
    std::cout << "Normalized path: " << messyPath << " -> "
              << atom::io::normPath(messyPath) << std::endl;
}

/**
 * @brief Demonstrates symbolic link operations
 */
void demonstrateSymlinkOperations() {
    std::cout << "\n=== Symbolic Link Operations ===" << std::endl;

    // Create symbolic link
    std::cout << "\n1. Creating symbolic link:" << std::endl;
    if (atom::io::createSymlink("test_file1.txt", "symlink_to_file.txt")) {
        std::cout << "✅ Symbolic link created successfully" << std::endl;

        // Check if symlink exists
        std::cout << "Symlink exists: "
                  << (atom::io::isFileExists("symlink_to_file.txt") ? "Yes"
                                                                    : "No")
                  << std::endl;

        // Get file size through symlink
        auto symlinkSize = atom::io::fileSize("symlink_to_file.txt");
        auto originalSize = atom::io::fileSize("test_file1.txt");
        std::cout << "Original file size: " << originalSize << " bytes"
                  << std::endl;
        std::cout << "Symlink target size: " << symlinkSize << " bytes"
                  << std::endl;

    } else {
        std::cout << "❌ Symbolic link creation failed (may not be supported "
                     "on this platform)"
                  << std::endl;
    }
}

/**
 * @brief Cleans up test files and directories
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files and directories..." << std::endl;

    // Remove files
    atom::io::removeFile("test_file1.txt");
    atom::io::removeFile("copied_file.txt");
    atom::io::removeFile("renamed_file.txt");
    atom::io::removeSymlink("symlink_to_file.txt");

    // Remove directories
    atom::io::removeDirectory("moved_test_dir");

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "📁 Atom I/O Basic File Operations Examples" << std::endl;
        std::cout << "===========================================" << std::endl;

        // Setup
        createTestFiles();

        // Run demonstrations
        demonstrateFileOperations();
        demonstrateDirectoryOperations();
        demonstratePathUtilities();
        demonstrateSymlinkOperations();

        // Cleanup
        cleanup();

        std::cout << "\n🎉 All basic file operations completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        return 1;
    }
}
