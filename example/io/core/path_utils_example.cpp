/**
 * @file path_utils_example.cpp
 * @brief Comprehensive demonstration of atom::io::path_utils functionality
 *
 * @details This example demonstrates:
 * - Path validation for security and format compliance
 * - File and folder name validation
 * - Permission validation
 * - Platform-specific validation (Windows reserved names, invalid characters)
 * - Path traversal detection
 *
 * @level Beginner
 * @prerequisites Basic understanding of filesystem operations
 * @related_examples file_operations.cpp, directory_traversal.cpp
 *
 * @note path_utils provides centralized path validation utilities
 *
 * @author Atom I/O Examples
 * @date 2024
 */

#include "atom/io/core/path_utils.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace atom::io::detail;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void printSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

void printResult(const std::string& description, bool result) {
    std::cout << "  " << description << ": "
              << (result ? "✓ Valid" : "✗ Invalid") << std::endl;
}

// ============================================================================
// PATH VALIDATION EXAMPLES
// ============================================================================

/**
 * @brief Demonstrates basic path validation
 *
 * Shows how to validate paths for security and format compliance.
 */
void basicPathValidationExample() {
    printSeparator("Basic Path Validation");

    std::cout << "Testing various paths for validity:\n" << std::endl;

    // Valid paths
    printResult("Normal relative path 'documents/file.txt'",
                validatePath("documents/file.txt"));
    printResult("Normal absolute path '/home/user/file.txt'",
                validatePath("/home/user/file.txt"));
    printResult("Current directory '.'", validatePath("."));

    // Invalid paths
    printResult("Empty path ''", validatePath(""));
    printResult("Path with null byte 'file\\0name.txt'",
                validatePath(std::string("file\0name.txt", 14)));

    // Path traversal (may be flagged as suspicious)
    printResult("Path with '..' traversal '../parent/file.txt'",
                validatePath("../parent/file.txt"));

#ifdef _WIN32
    // Windows-specific tests
    std::cout << "\nWindows-specific path tests:" << std::endl;
    printResult("Path with invalid char 'file<name>.txt'",
                validatePath("file<name>.txt"));
    printResult("Path with pipe 'file|name.txt'",
                validatePath("file|name.txt"));
    printResult("Reserved name 'CON'", validatePath("CON"));
    printResult("Reserved name with extension 'NUL.txt'",
                validatePath("NUL.txt"));
#endif
}

/**
 * @brief Demonstrates folder name validation
 *
 * Shows how to validate individual folder names (not full paths).
 */
void folderNameValidationExample() {
    printSeparator("Folder Name Validation");

    std::cout << "Testing folder names for validity:\n" << std::endl;

    // Valid folder names
    printResult("Simple name 'my_folder'", isFolderNameValid("my_folder"));
    printResult("Name with spaces 'My Folder'", isFolderNameValid("My Folder"));
    printResult("Name with numbers 'folder123'",
                isFolderNameValid("folder123"));
    printResult("Name with dots 'folder.backup'",
                isFolderNameValid("folder.backup"));
    printResult("Name with hyphen 'my-folder'", isFolderNameValid("my-folder"));

    // Invalid folder names
    printResult("Empty name ''", isFolderNameValid(""));

#ifdef _WIN32
    printResult("Name with colon 'folder:name'",
                isFolderNameValid("folder:name"));
    printResult("Name with asterisk 'folder*name'",
                isFolderNameValid("folder*name"));
#else
    printResult("Name with slash 'folder/name'",
                isFolderNameValid("folder/name"));
#endif
}

/**
 * @brief Demonstrates file name validation
 *
 * Shows how to validate individual file names (not full paths).
 */
void fileNameValidationExample() {
    printSeparator("File Name Validation");

    std::cout << "Testing file names for validity:\n" << std::endl;

    // Valid file names
    printResult("Simple name 'document.txt'", isFileNameValid("document.txt"));
    printResult("Name with spaces 'My Document.pdf'",
                isFileNameValid("My Document.pdf"));
    printResult("Name with multiple dots 'file.backup.txt'",
                isFileNameValid("file.backup.txt"));
    printResult("Name with underscore 'my_file.cpp'",
                isFileNameValid("my_file.cpp"));

    // Invalid file names
    printResult("Empty name ''", isFileNameValid(""));

#ifdef _WIN32
    printResult("Name with less-than 'file<1>.txt'",
                isFileNameValid("file<1>.txt"));
    printResult("Name with greater-than 'file>1.txt'",
                isFileNameValid("file>1.txt"));
    printResult("Name with question mark 'file?.txt'",
                isFileNameValid("file?.txt"));
#else
    printResult("Name with slash 'file/name.txt'",
                isFileNameValid("file/name.txt"));
#endif
}

/**
 * @brief Demonstrates permission validation
 *
 * Shows how to validate file permissions for read/write operations.
 */
void permissionValidationExample() {
    printSeparator("Permission Validation");

    std::cout << "Testing file permissions:\n" << std::endl;

    // Create a temporary file for testing
    fs::path tempDir = fs::temp_directory_path();
    fs::path testFile = tempDir / "path_utils_test.txt";

    // Create the test file
    {
        std::ofstream ofs(testFile);
        ofs << "Test content";
    }

    std::cout << "Test file: " << testFile << std::endl;

    // Test read permission
    printResult("Read permission on temp file",
                validatePermissions(testFile.string(), false));

    // Test write permission
    printResult("Write permission on temp file",
                validatePermissions(testFile.string(), true));

    // Test non-existent file
    printResult("Permission on non-existent file",
                validatePermissions("/nonexistent/file.txt", false));

    // Cleanup
    fs::remove(testFile);
    std::cout << "\nTest file cleaned up." << std::endl;
}

/**
 * @brief Demonstrates path validation for directory stack operations
 *
 * Shows how to validate paths for directory stack (pushd/popd) operations.
 */
void directoryStackPathValidationExample() {
    printSeparator("Directory Stack Path Validation");

    std::cout << "Testing paths for directory stack operations:\n" << std::endl;

    // Valid paths
    printResult("Current directory '.'", isValidPath(fs::path(".")));
    printResult("Temp directory", isValidPath(fs::temp_directory_path()));

    // Test with actual existing directory
    fs::path homeDir;
#ifdef _WIN32
    homeDir = fs::path(std::getenv("USERPROFILE") ? std::getenv("USERPROFILE")
                                                  : "C:\\");
#else
    homeDir = fs::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp");
#endif
    printResult("Home directory", isValidPath(homeDir));

    // Invalid paths
    printResult("Non-existent path '/definitely/not/exists'",
                isValidPath(fs::path("/definitely/not/exists")));
}

/**
 * @brief Demonstrates practical use cases for path validation
 *
 * Shows real-world scenarios where path validation is important.
 */
void practicalUseCasesExample() {
    printSeparator("Practical Use Cases");

    std::cout << "Scenario 1: User input validation\n" << std::endl;

    // Simulate user input
    std::vector<std::string> userInputs = {
        "documents/report.pdf",
        "../../../etc/passwd",  // Path traversal attempt
        "normal_file.txt",
        "",                  // Empty input
        "file\0hidden.txt",  // Null byte injection
    };

    for (const auto& input : userInputs) {
        bool isValid = validatePath(input);
        std::cout << "  Input: '" << input.substr(0, 30)
                  << (input.length() > 30 ? "..." : "") << "' -> "
                  << (isValid ? "ACCEPTED" : "REJECTED") << std::endl;
    }

    std::cout << "\nScenario 2: Batch file processing validation\n"
              << std::endl;

    std::vector<std::string> filenames = {
        "report_2024.pdf",  "data.csv", "image.png",
        "file:invalid.txt",  // Invalid on Windows
        "valid_file.doc",
    };

    int validCount = 0;
    int invalidCount = 0;

    for (const auto& filename : filenames) {
        if (isFileNameValid(filename)) {
            validCount++;
            std::cout << "  ✓ " << filename << std::endl;
        } else {
            invalidCount++;
            std::cout << "  ✗ " << filename << " (invalid)" << std::endl;
        }
    }

    std::cout << "\n  Summary: " << validCount << " valid, " << invalidCount
              << " invalid" << std::endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main() {
    std::cout << "=================================================="
              << std::endl;
    std::cout << "  Atom I/O Path Utilities Examples" << std::endl;
    std::cout << "=================================================="
              << std::endl;

#ifdef _WIN32
    std::cout << "Platform: Windows" << std::endl;
#else
    std::cout << "Platform: Unix/Linux" << std::endl;
#endif

    try {
        // Basic validation examples
        basicPathValidationExample();
        folderNameValidationExample();
        fileNameValidationExample();

        // Permission validation
        permissionValidationExample();

        // Directory stack path validation
        directoryStackPathValidationExample();

        // Practical use cases
        practicalUseCasesExample();

        std::cout << "\n=================================================="
                  << std::endl;
        std::cout << "  All examples completed successfully!" << std::endl;
        std::cout << "=================================================="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
