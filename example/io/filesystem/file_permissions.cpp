/**
 * @file file_permissions.cpp
 * @brief Comprehensive demonstration of file permission operations
 *
 * This example demonstrates:
 * - Reading and displaying file permissions
 * - Changing file permissions with validation
 * - Comparing file and process permissions
 * - Cross-platform permission handling
 * - Permission string formatting and parsing
 * - Error handling for permission operations
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "atom/io/filesystem/file_permission.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates test files with different initial permissions
 */
void createTestFiles() {
    std::cout << "Creating test files for permission demonstrations..."
              << std::endl;

    std::vector<std::pair<std::string, std::string>> testFiles = {
        {"perm_test_read.txt", "This is a read-only test file."},
        {"perm_test_write.txt", "This is a writable test file."},
        {"perm_test_exec.txt",
         "#!/bin/bash\necho 'This is an executable test file'"},
        {"perm_test_full.txt", "This file will have full permissions."}};

    for (const auto& [filename, content] : testFiles) {
        std::ofstream file(filename);
        file << content;
        file.close();
        std::cout << "  📄 Created " << filename << std::endl;
    }

    std::cout << "✅ Created " << testFiles.size() << " test files"
              << std::endl;
}

/**
 * @brief Demonstrates reading and displaying file permissions
 */
void demonstrateReadingPermissions() {
    std::cout << "\n=== Reading File Permissions ===" << std::endl;

    std::vector<std::string> testFiles = {
        "perm_test_read.txt", "perm_test_write.txt", "perm_test_exec.txt",
        "perm_test_full.txt"};

    std::cout << "Current file permissions:" << std::endl;
    std::cout << "File                    | Permissions" << std::endl;
    std::cout << "------------------------|------------" << std::endl;

    for (const auto& file : testFiles) {
        if (fs::exists(file)) {
            auto permissions = atom::io::getFilePermissions(file);
            if (!permissions.empty()) {
                std::cout << std::left << std::setw(23) << file << " | "
                          << permissions << std::endl;
            } else {
                std::cout << std::left << std::setw(23) << file
                          << " | Error reading permissions" << std::endl;
            }
        }
    }

    // Show current process permissions
    std::cout << "\nCurrent process permissions: "
              << atom::io::getSelfPermissions() << std::endl;
}

/**
 * @brief Demonstrates changing file permissions
 */
void demonstrateChangingPermissions() {
    std::cout << "\n=== Changing File Permissions ===" << std::endl;

    const std::string testFile = "perm_test_write.txt";

    if (!fs::exists(testFile)) {
        std::cout << "❌ Test file not found: " << testFile << std::endl;
        return;
    }

    // Show original permissions
    auto originalPerms = atom::io::getFilePermissions(testFile);
    std::cout << "Original permissions for " << testFile << ": "
              << originalPerms << std::endl;

    // Test different permission combinations
    std::vector<std::pair<std::string, std::string>> permissionTests = {
        {"rw-r--r--", "Read/write for owner, read-only for group and others"},
        {"rwxr-xr-x",
         "Full permissions for owner, read/execute for group and others"},
        {"r--r--r--", "Read-only for everyone"},
        {"rwxrwxrwx", "Full permissions for everyone"}};

    for (const auto& [permString, description] : permissionTests) {
        std::cout << "\nSetting permissions to: " << permString << " ("
                  << description << ")" << std::endl;

        try {
            atom::io::changeFilePermissions(testFile, permString);

            // Verify the change
            auto newPerms = atom::io::getFilePermissions(testFile);
            if (newPerms == permString) {
                std::cout << "✅ Permissions changed successfully: " << newPerms
                          << std::endl;
            } else {
                std::cout << "⚠️  Permissions changed but may differ: "
                          << newPerms << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "❌ Failed to change permissions: " << e.what()
                      << std::endl;
        }
    }

    // Restore original permissions if possible
    if (!originalPerms.empty()) {
        try {
            atom::io::changeFilePermissions(testFile, originalPerms);
            std::cout << "\n🔄 Restored original permissions: " << originalPerms
                      << std::endl;
        } catch (const std::exception& e) {
            std::cout << "\n⚠️  Could not restore original permissions: "
                      << e.what() << std::endl;
        }
    }
}

/**
 * @brief Demonstrates permission comparison operations
 */
void demonstratePermissionComparison() {
    std::cout << "\n=== Permission Comparison ===" << std::endl;

    std::vector<std::string> testFiles = {
        "perm_test_read.txt", "perm_test_write.txt", "perm_test_exec.txt",
        "perm_test_full.txt"};

    std::cout << "Comparing file permissions with current process permissions:"
              << std::endl;

    for (const auto& file : testFiles) {
        if (fs::exists(file)) {
            auto comparison = atom::io::compareFileAndSelfPermissions(file);

            if (comparison.has_value()) {
                std::string result =
                    comparison.value()
                        ? "Process has equal or greater permissions"
                        : "Process has lesser permissions";

                std::cout << "  📄 " << file << ": " << result << std::endl;
            } else {
                std::cout << "  ❌ " << file << ": Error comparing permissions"
                          << std::endl;
            }
        }
    }
}

/**
 * @brief Demonstrates error handling for permission operations
 */
void demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling ===" << std::endl;

    // Test with non-existent file
    std::cout << "1. Testing with non-existent file..." << std::endl;
    auto nonExistentPerms =
        atom::io::getFilePermissions("non_existent_file.txt");
    if (nonExistentPerms.empty()) {
        std::cout << "✅ Correctly handled non-existent file (empty "
                     "permissions returned)"
                  << std::endl;
    } else {
        std::cout << "❌ Unexpected result for non-existent file" << std::endl;
    }

    // Test with invalid permission string
    std::cout << "\n2. Testing with invalid permission string..." << std::endl;
    try {
        atom::io::changeFilePermissions("perm_test_read.txt",
                                        "invalid_permissions");
        std::cout
            << "❌ Should have thrown an exception for invalid permissions"
            << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ Correctly caught invalid argument: " << e.what()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Caught exception: " << e.what() << std::endl;
    }

    // Test with wrong length permission string
    std::cout << "\n3. Testing with wrong length permission string..."
              << std::endl;
    try {
        atom::io::changeFilePermissions("perm_test_read.txt", "rwx");
        std::cout << "❌ Should have thrown an exception for wrong length"
                  << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ Correctly caught invalid length: " << e.what()
                  << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✅ Caught exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrates cross-platform permission handling
 */
void demonstrateCrossPlatformHandling() {
    std::cout << "\n=== Cross-Platform Permission Handling ===" << std::endl;

    const std::string testFile = "perm_test_full.txt";

    if (!fs::exists(testFile)) {
        std::cout << "❌ Test file not found: " << testFile << std::endl;
        return;
    }

#ifdef _WIN32
    std::cout << "Running on Windows platform" << std::endl;
    std::cout
        << "Note: Windows has different permission model than Unix-like systems"
        << std::endl;
#else
    std::cout << "Running on Unix-like platform" << std::endl;
    std::cout << "Full Unix permission model available" << std::endl;
#endif

    // Test platform-specific permission operations
    auto currentPerms = atom::io::getFilePermissions(testFile);
    std::cout << "Current permissions: " << currentPerms << std::endl;

    // Try to set executable permissions
    std::cout << "\nTesting executable permission setting..." << std::endl;
    try {
        atom::io::changeFilePermissions(testFile, "rwxr--r--");
        auto newPerms = atom::io::getFilePermissions(testFile);
        std::cout << "✅ Set executable permissions: " << newPerms << std::endl;

        // Restore
        if (!currentPerms.empty()) {
            atom::io::changeFilePermissions(testFile, currentPerms);
        }
    } catch (const std::exception& e) {
        std::cout << "⚠️  Platform may not support this operation: " << e.what()
                  << std::endl;
    }
}

/**
 * @brief Demonstrates permission validation and formatting
 */
void demonstratePermissionValidation() {
    std::cout << "\n=== Permission Validation and Formatting ===" << std::endl;

    std::vector<std::string> permissionStrings = {
        "rwxrwxrwx",  // Valid: full permissions
        "r--r--r--",  // Valid: read-only
        "rw-rw-rw-",  // Valid: read-write
        "rwxr-xr-x",  // Valid: typical executable
        "rwxrwxrwx",  // Valid: all permissions
        "invalid",    // Invalid: wrong format
        "rwxrwxrw",   // Invalid: wrong length
        "abc123def",  // Invalid: wrong characters
        ""            // Invalid: empty
    };

    const std::string testFile = "perm_test_read.txt";

    std::cout << "Testing permission string validation:" << std::endl;
    std::cout << "Permission String | Result" << std::endl;
    std::cout << "------------------|--------" << std::endl;

    for (const auto& permString : permissionStrings) {
        try {
            atom::io::changeFilePermissions(testFile, permString);
            std::cout << std::left << std::setw(17) << permString
                      << " | ✅ Valid" << std::endl;

            // Restore to a known state
            atom::io::changeFilePermissions(testFile, "rw-r--r--");

        } catch (const std::invalid_argument& e) {
            std::cout << std::left << std::setw(17) << permString
                      << " | ❌ Invalid format" << std::endl;
        } catch (const std::exception& e) {
            std::cout << std::left << std::setw(17) << permString
                      << " | ⚠️  Error: " << e.what() << std::endl;
        }
    }
}

/**
 * @brief Cleans up test files
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test files..." << std::endl;

    std::vector<std::string> filesToRemove = {
        "perm_test_read.txt", "perm_test_write.txt", "perm_test_exec.txt",
        "perm_test_full.txt"};

    for (const auto& file : filesToRemove) {
        if (fs::exists(file)) {
            fs::remove(file);
        }
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🔐 Atom I/O File Permissions Examples" << std::endl;
        std::cout << "=====================================" << std::endl;

        // Setup
        createTestFiles();

        // Run demonstrations
        demonstrateReadingPermissions();
        demonstrateChangingPermissions();
        demonstratePermissionComparison();
        demonstrateErrorHandling();
        demonstrateCrossPlatformHandling();
        demonstratePermissionValidation();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All file permission operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
