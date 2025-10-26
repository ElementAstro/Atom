/**
 * @file glob_patterns.cpp
 * @brief Comprehensive demonstration of glob pattern matching
 *
 * This example demonstrates:
 * - Basic glob pattern matching with wildcards
 * - Recursive glob patterns using **
 * - Multiple pattern matching
 * - Directory-only filtering
 * - Pattern compilation and caching
 * - Cross-platform path handling
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include "atom/io/core/glob.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates a test directory structure for glob demonstrations
 */
void createTestStructure() {
    std::cout << "Creating test directory structure..." << std::endl;

    // Create directories
    fs::create_directories("test_glob/subdir1");
    fs::create_directories("test_glob/subdir2/nested");
    fs::create_directories("test_glob/subdir3");

    // Create various test files
    std::vector<std::string> testFiles = {
        "test_glob/file1.txt",
        "test_glob/file2.cpp",
        "test_glob/file3.md",
        "test_glob/document.doc",
        "test_glob/image.jpg",
        "test_glob/subdir1/nested_file1.txt",
        "test_glob/subdir1/nested_file2.hpp",
        "test_glob/subdir2/another_file.cpp",
        "test_glob/subdir2/nested/deep_file.txt",
        "test_glob/subdir3/readme.md"};

    for (const auto& file : testFiles) {
        std::ofstream outFile(file);
        if (outFile) {
            outFile << "Test content for " << fs::path(file).filename()
                    << std::endl;
            outFile.close();
        }
    }

    std::cout << "✅ Test structure created successfully" << std::endl;
}

/**
 * @brief Cleans up the test directory structure
 */
void cleanupTestStructure() {
    if (fs::exists("test_glob")) {
        fs::remove_all("test_glob");
        std::cout << "🧹 Test structure cleaned up" << std::endl;
    }
}

/**
 * @brief Demonstrates basic glob pattern matching
 */
void demonstrateBasicGlob() {
    std::cout << "\n=== Basic Glob Pattern Matching ===" << std::endl;

    // Find all .txt files
    std::cout << "\n1. Finding all .txt files:" << std::endl;
    auto txtFiles = atom::io::glob("test_glob/*.txt", false, false);
    for (const auto& file : txtFiles) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Find all .cpp files
    std::cout << "\n2. Finding all .cpp files:" << std::endl;
    auto cppFiles = atom::io::glob("test_glob/*.cpp", false, false);
    for (const auto& file : cppFiles) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Find files with specific pattern
    std::cout << "\n3. Finding files starting with 'file':" << std::endl;
    auto filePattern = atom::io::glob("test_glob/file*", false, false);
    for (const auto& file : filePattern) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Find files with character class
    std::cout << "\n4. Finding files with numbers (file[0-9]*):" << std::endl;
    auto numberedFiles = atom::io::glob("test_glob/file[0-9]*", false, false);
    for (const auto& file : numberedFiles) {
        std::cout << "  📄 " << file << std::endl;
    }
}

/**
 * @brief Demonstrates recursive glob patterns
 */
void demonstrateRecursiveGlob() {
    std::cout << "\n=== Recursive Glob Pattern Matching ===" << std::endl;

    // Find all .txt files recursively
    std::cout << "\n1. Finding all .txt files recursively:" << std::endl;
    auto allTxtFiles = atom::io::glob("test_glob/**/*.txt", true, false);
    for (const auto& file : allTxtFiles) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Find all files recursively
    std::cout << "\n2. Finding all files recursively:" << std::endl;
    auto allFiles = atom::io::glob("test_glob/**/*", true, false);
    for (const auto& file : allFiles) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Using rglob convenience function
    std::cout << "\n3. Using rglob for .cpp files:" << std::endl;
    auto rglob_cpp = atom::io::rglob("test_glob/**/*.cpp");
    for (const auto& file : rglob_cpp) {
        std::cout << "  📄 " << file << std::endl;
    }
}

/**
 * @brief Demonstrates directory-only filtering
 */
void demonstrateDirectoryGlob() {
    std::cout << "\n=== Directory-Only Glob Matching ===" << std::endl;

    // Find all directories
    std::cout << "\n1. Finding all directories:" << std::endl;
    auto directories = atom::io::glob("test_glob/*", false, true);
    for (const auto& dir : directories) {
        std::cout << "  📁 " << dir << std::endl;
    }

    // Find all subdirectories recursively
    std::cout << "\n2. Finding all subdirectories recursively:" << std::endl;
    auto allDirs = atom::io::glob("test_glob/**/*", true, true);
    for (const auto& dir : allDirs) {
        std::cout << "  📁 " << dir << std::endl;
    }
}

/**
 * @brief Demonstrates multiple pattern matching
 */
void demonstrateMultiplePatterns() {
    std::cout << "\n=== Multiple Pattern Matching ===" << std::endl;

    // Match multiple file types
    std::vector<std::string> patterns = {"test_glob/*.txt", "test_glob/*.md",
                                         "test_glob/*.cpp"};

    std::cout << "\nFinding files matching multiple patterns (.txt, .md, .cpp):"
              << std::endl;
    auto multipleMatches = atom::io::glob(patterns);
    for (const auto& file : multipleMatches) {
        std::cout << "  📄 " << file << std::endl;
    }
}

/**
 * @brief Demonstrates advanced glob features
 */
void demonstrateAdvancedFeatures() {
    std::cout << "\n=== Advanced Glob Features ===" << std::endl;

    // Negation patterns (if supported)
    std::cout << "\n1. Complex patterns with character classes:" << std::endl;
    auto complexPattern = atom::io::glob("test_glob/*.[ch]*", false, false);
    for (const auto& file : complexPattern) {
        std::cout << "  📄 " << file << std::endl;
    }

    // Question mark wildcard
    std::cout << "\n2. Single character wildcard (file?.*):" << std::endl;
    auto singleChar = atom::io::glob("test_glob/file?.*", false, false);
    for (const auto& file : singleChar) {
        std::cout << "  📄 " << file << std::endl;
    }
}

int main() {
    try {
        std::cout << "🔍 Atom I/O Glob Pattern Examples" << std::endl;
        std::cout << "=================================" << std::endl;

        // Setup test environment
        createTestStructure();

        // Run demonstrations
        demonstrateBasicGlob();
        demonstrateRecursiveGlob();
        demonstrateDirectoryGlob();
        demonstrateMultiplePatterns();
        demonstrateAdvancedFeatures();

        // Cleanup
        cleanupTestStructure();

        std::cout << "\n🎉 All glob pattern examples completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanupTestStructure();  // Ensure cleanup even on error
        return 1;
    }
}
