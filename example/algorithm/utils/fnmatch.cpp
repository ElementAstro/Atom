/**
 * @file fnmatch.cpp
 * @brief Comprehensive example demonstrating filename matching utilities
 *
 * This example shows how to:
 * - Use glob patterns for filename matching
 * - Filter file lists with single and multiple patterns
 * - Handle different wildcard patterns and special characters
 * - Demonstrate case-sensitive and case-insensitive matching
 * - Show performance characteristics with large file lists
 * - Handle edge cases and complex patterns
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/fnmatch.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates basic filename matching patterns
 */
void demonstrateBasicPatternMatching() {
    printHeader("Basic Filename Pattern Matching");

    try {
        std::cout << "Testing various glob patterns:\n\n";

        // Test cases with different patterns
        std::vector<std::tuple<std::string, std::string, bool, std::string>>
            testCases = {
                {"*.txt", "document.txt", true, "Simple wildcard"},
                {"*.txt", "image.png", false, "Extension mismatch"},
                {"doc*", "document.txt", true, "Prefix wildcard"},
                {"*ment*", "document.txt", true, "Middle wildcard"},
                {"test?.txt", "test1.txt", true, "Single character wildcard"},
                {"test?.txt", "test10.txt", false,
                 "Multiple characters vs single"},
                {"[abc]*.txt", "a_file.txt", true, "Character class"},
                {"[abc]*.txt", "d_file.txt", false, "Character class mismatch"},
                {"[0-9]*.log", "1_debug.log", true, "Numeric range"},
                {"[!0-9]*.log", "a_debug.log", true, "Negated character class"},
                {"[!0-9]*.log", "1_debug.log", false, "Negated class mismatch"},
                {"", "", true, "Empty pattern and string"},
                {"*", "anything", true, "Match everything"},
                {"exact", "exact", true, "Exact match"},
                {"exact", "Exact", false, "Case sensitivity"}};

        for (const auto& [pattern, filename, expected, description] :
             testCases) {
            bool result = fnmatch(pattern, filename);
            std::cout << std::left << std::setw(25) << description << ": ";
            std::cout << "\"" << pattern << "\" vs \"" << filename << "\" -> ";
            std::cout << (result ? "MATCH" : "NO MATCH");
            std::cout << " " << (result == expected ? "✓" : "✗") << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic pattern matching: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates file filtering with patterns
 */
void demonstrateFileFiltering() {
    printHeader("File Filtering with Patterns");

    try {
        // Sample file list
        std::vector<std::string> filenames = {
            "document.txt", "image.png",  "notes.txt",     "photo.jpg",
            "readme.md",    "config.xml", "data.csv",      "script.py",
            "test1.log",    "test2.log",  "backup.tar.gz", "archive.zip"};

        std::cout << "Sample file list:\n";
        for (const auto& file : filenames) {
            std::cout << "  " << file << "\n";
        }

        // Test single pattern filtering
        std::cout << "\nSingle pattern filtering:\n";
        std::vector<std::string> singlePatterns = {"*.txt", "*.log", "test*",
                                                   "*.*"};

        for (const auto& pattern : singlePatterns) {
            bool anyMatch = filter(filenames, pattern);
            std::cout << "  Pattern \"" << pattern
                      << "\": " << (anyMatch ? "Has matches" : "No matches")
                      << "\n";
        }

        // Test multiple pattern filtering
        std::cout << "\nMultiple pattern filtering:\n";
        std::vector<std::vector<std::string>> multiPatterns = {
            {"*.txt", "*.md"},
            {"*.log", "*.csv"},
            {"test*", "config*"},
            {"*.jpg", "*.png", "*.gif"}};

        for (const auto& patterns : multiPatterns) {
            std::vector<std::string> matchedFiles = filter(filenames, patterns);

            std::cout << "  Patterns: ";
            for (const auto& pat : patterns) {
                std::cout << "\"" << pat << "\" ";
            }
            std::cout << "\n  Matches: ";
            if (matchedFiles.empty()) {
                std::cout << "(none)";
            } else {
                for (const auto& file : matchedFiles) {
                    std::cout << file << " ";
                }
            }
            std::cout << "\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in file filtering demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates pattern translation to regex
 */
void demonstratePatternTranslation() {
    printHeader("Pattern Translation to Regex");

    try {
        std::cout << "Converting glob patterns to regular expressions:\n\n";

        std::vector<std::string> patterns = {
            "*.txt",
            "test?.log",
            "[abc]*.dat",
            "[0-9][0-9]*.tmp",
            "[!.]*.conf",
            "backup_[0-9][0-9][0-9][0-9]_[0-9][0-9]_[0-9][0-9].tar.gz",
            "**/config.xml",
            "src/**/*.cpp"};

        for (const auto& pattern : patterns) {
            auto result = translate(pattern);

            std::cout << "Pattern: \"" << pattern << "\"\n";
            if (result.has_value()) {
                std::cout << "  Regex: \"" << result.value() << "\"\n";
                std::cout << "  Status: ✓ Success\n";
            } else {
                std::cout << "  Status: ✗ Translation failed\n";
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in pattern translation: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates performance characteristics
 */
void demonstratePerformanceCharacteristics() {
    printHeader("Performance Characteristics");

    try {
        std::cout << "Performance analysis with large file lists:\n\n";

        // Generate large file list
        std::vector<std::string> largeFileList;
        for (int i = 0; i < 10000; ++i) {
            largeFileList.push_back("file_" + std::to_string(i) + ".txt");
            largeFileList.push_back("data_" + std::to_string(i) + ".csv");
            largeFileList.push_back("log_" + std::to_string(i) + ".log");
        }

        std::cout << "Testing with " << largeFileList.size() << " files\n";

        // Test different patterns
        std::vector<std::string> testPatterns = {"*.txt", "file_*",
                                                 "*_[0-9]*.csv"};

        for (const auto& pattern : testPatterns) {
            auto start = std::chrono::high_resolution_clock::now();

            int matchCount = 0;
            for (const auto& filename : largeFileList) {
                if (fnmatch(pattern, filename)) {
                    matchCount++;
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);

            std::cout << "  Pattern \"" << pattern << "\":\n";
            std::cout << "    Matches: " << matchCount << "\n";
            std::cout << "    Time: " << duration.count() << " μs\n";
            std::cout << "    Rate: " << std::fixed << std::setprecision(2)
                      << (static_cast<double>(largeFileList.size()) /
                          duration.count() * 1000)
                      << " files/ms\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in performance demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive filename matching
 */
int main() {
    std::cout
        << "=== Atom Filename Matching Utilities Comprehensive Example ===\n";
    std::cout << "Demonstrating glob pattern matching and file filtering...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicPatternMatching();
        demonstrateFileFiltering();
        demonstratePatternTranslation();
        demonstratePerformanceCharacteristics();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Filename Matching Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The filename matching utilities provide:\n";
        std::cout << "  ✓ Glob pattern matching with wildcards and character "
                     "classes\n";
        std::cout << "  ✓ File filtering with single and multiple patterns\n";
        std::cout << "  ✓ Pattern translation to regular expressions\n";
        std::cout << "  ✓ High-performance matching for large file lists\n";
        std::cout << "  ✓ Support for complex patterns and edge cases\n";
        std::cout << "  ✓ Cross-platform filename matching capabilities\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in filename matching example: "
                  << e.what() << "\n";
        return 1;
    }
}
