/**
 * @file directory_traversal.cpp
 * @brief Demonstration of directory traversal and file analysis operations
 *
 * This example demonstrates:
 * - Directory walking with callbacks
 * - JSON-based directory structure export
 * - File classification by extension
 * - Executable file searching
 * - File type checking and analysis
 * - Line counting in text files
 * - File time information retrieval
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include "atom/io/core/io.hpp"

namespace fs = std::filesystem;

/**
 * @brief Creates a complex test directory structure
 */
void createTestStructure() {
    std::cout << "Creating complex test directory structure..." << std::endl;

    // Create directories
    fs::create_directories("test_analysis/src/cpp");
    fs::create_directories("test_analysis/src/python");
    fs::create_directories("test_analysis/docs");
    fs::create_directories("test_analysis/bin");
    fs::create_directories("test_analysis/data");

    // Create various file types
    std::map<std::string, std::string> testFiles = {
        {"test_analysis/src/cpp/main.cpp",
         "#include <iostream>\nint main() {\n    std::cout << \"Hello World!\" "
         "<< std::endl;\n    return 0;\n}"},
        {"test_analysis/src/cpp/utils.hpp",
         "#pragma once\n#include <string>\nclass Utils {\npublic:\n    static "
         "std::string getName();\n};"},
        {"test_analysis/src/cpp/utils.cpp",
         "#include \"utils.hpp\"\nstd::string Utils::getName() {\n    return "
         "\"Utils\";\n}"},
        {"test_analysis/src/python/script.py",
         "#!/usr/bin/env python3\nimport sys\n\ndef main():\n    print(\"Hello "
         "from Python!\")\n\nif __name__ == \"__main__\":\n    main()"},
        {"test_analysis/src/python/module.py",
         "\"\"\"A sample Python module\"\"\"\n\nclass SampleClass:\n    def "
         "__init__(self):\n        self.value = 42\n    \n    def "
         "get_value(self):\n        return self.value"},
        {"test_analysis/docs/README.md",
         "# Test Project\n\nThis is a test project for directory "
         "analysis.\n\n## Features\n- File analysis\n- Directory traversal\n- "
         "Type classification"},
        {"test_analysis/docs/manual.txt",
         "User Manual\n===========\n\nThis is a simple text manual\nwith "
         "multiple lines\nfor testing purposes."},
        {"test_analysis/data/config.json",
         "{\n  \"name\": \"test_config\",\n  \"version\": \"1.0.0\",\n  "
         "\"settings\": {\n    \"debug\": true\n  }\n}"},
        {"test_analysis/data/data.csv",
         "name,age,city\nJohn,25,New York\nJane,30,Los "
         "Angeles\nBob,35,Chicago"},
        {"test_analysis/LICENSE",
         "MIT License\n\nCopyright (c) 2024 Test Project\n\nPermission is "
         "hereby granted..."}};

    for (const auto& [path, content] : testFiles) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    std::cout << "✅ Test structure created with " << testFiles.size()
              << " files" << std::endl;
}

/**
 * @brief Demonstrates directory walking with callbacks
 */
void demonstrateDirectoryWalking() {
    std::cout << "\n=== Directory Walking with Callbacks ===" << std::endl;

    std::cout << "\n1. Walking through all files:" << std::endl;
    int fileCount = 0;
    size_t totalSize = 0;

    atom::io::fwalk("test_analysis", [&](const fs::path& path) {
        if (fs::is_regular_file(path)) {
            fileCount++;
            auto size = atom::io::getFileSize(path);
            totalSize += size;
            std::cout << "  📄 " << path << " (" << size << " bytes)"
                      << std::endl;
        } else if (fs::is_directory(path)) {
            std::cout << "  📁 " << path << "/" << std::endl;
        }
    });

    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Total files: " << fileCount << std::endl;
    std::cout << "  Total size: " << totalSize << " bytes" << std::endl;
}

/**
 * @brief Demonstrates JSON directory structure export
 */
void demonstrateJsonExport() {
    std::cout << "\n=== JSON Directory Structure Export ===" << std::endl;

    std::cout << "\nExporting directory structure to JSON..." << std::endl;
    auto jsonStructure = atom::io::jwalk("test_analysis");

    if (!jsonStructure.empty()) {
        std::cout << "✅ JSON structure generated successfully" << std::endl;
        std::cout << "JSON length: " << jsonStructure.length() << " characters"
                  << std::endl;

        // Save to file for inspection
        std::ofstream jsonFile("directory_structure.json");
        jsonFile << jsonStructure;
        jsonFile.close();
        std::cout << "📄 JSON saved to directory_structure.json" << std::endl;

        // Show first 200 characters as preview
        std::cout << "\nJSON Preview (first 200 chars):" << std::endl;
        std::cout << jsonStructure.substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "❌ Failed to generate JSON structure" << std::endl;
    }
}

/**
 * @brief Demonstrates file classification by extension
 */
void demonstrateFileClassification() {
    std::cout << "\n=== File Classification by Extension ===" << std::endl;

    auto classifiedFiles = atom::io::classifyFiles("test_analysis");

    std::cout << "\nFiles classified by extension:" << std::endl;
    for (const auto& [extension, files] : classifiedFiles) {
        std::cout << "\n"
                  << (extension.empty() ? "[no extension]" : extension) << ":"
                  << std::endl;
        for (const auto& file : files) {
            auto size = atom::io::getFileSize(file);
            std::cout << "  📄 " << file << " (" << size << " bytes)"
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates file type checking and analysis
 */
void demonstrateFileTypeAnalysis() {
    std::cout << "\n=== File Type Analysis ===" << std::endl;

    std::vector<std::string> testPaths = {
        "test_analysis", "test_analysis/src/cpp/main.cpp",
        "test_analysis/docs/README.md", "nonexistent_file.txt"};

    std::cout << "\nAnalyzing path types:" << std::endl;
    for (const auto& path : testPaths) {
        auto pathType = atom::io::checkPathType(path);
        std::string typeStr;

        switch (pathType) {
            case atom::io::PathType::NOT_EXISTS:
                typeStr = "Does not exist";
                break;
            case atom::io::PathType::REGULAR_FILE:
                typeStr = "Regular file";
                break;
            case atom::io::PathType::DIRECTORY:
                typeStr = "Directory";
                break;
            case atom::io::PathType::SYMLINK:
                typeStr = "Symbolic link";
                break;
            case atom::io::PathType::OTHER:
                typeStr = "Other type";
                break;
        }

        std::cout << "  " << path << " -> " << typeStr << std::endl;
    }
}

/**
 * @brief Demonstrates line counting in text files
 */
void demonstrateLineCounting() {
    std::cout << "\n=== Line Counting in Text Files ===" << std::endl;

    std::vector<std::string> textFiles = {
        "test_analysis/src/cpp/main.cpp", "test_analysis/src/python/script.py",
        "test_analysis/docs/README.md", "test_analysis/docs/manual.txt"};

    std::cout << "\nCounting lines in text files:" << std::endl;
    int totalLines = 0;

    for (const auto& file : textFiles) {
        auto lineCount = atom::io::countLinesInFile(file);
        if (lineCount.has_value()) {
            std::cout << "  📄 " << file << " -> " << lineCount.value()
                      << " lines" << std::endl;
            totalLines += lineCount.value();
        } else {
            std::cout << "  ❌ " << file << " -> Could not count lines"
                      << std::endl;
        }
    }

    std::cout << "\nTotal lines across all files: " << totalLines << std::endl;
}

/**
 * @brief Demonstrates file time information retrieval
 */
void demonstrateFileTimeInfo() {
    std::cout << "\n=== File Time Information ===" << std::endl;

    std::vector<std::string> files = {"test_analysis/src/cpp/main.cpp",
                                      "test_analysis/docs/README.md",
                                      "test_analysis/data/config.json"};

    std::cout << "\nFile time information:" << std::endl;
    for (const auto& file : files) {
        auto [creationTime, modificationTime] = atom::io::getFileTimes(file);
        std::cout << "  📄 " << file << std::endl;
        std::cout << "    Creation: " << creationTime << std::endl;
        std::cout << "    Modified: " << modificationTime << std::endl;
    }
}

/**
 * @brief Demonstrates executable file searching
 */
void demonstrateExecutableSearch() {
    std::cout << "\n=== Executable File Search ===" << std::endl;

    // Check if files are executable
    std::vector<std::string> filesToCheck = {
        "test_analysis/src/python/script.py", "test_analysis/src/cpp/main.cpp"};

    std::cout << "\nChecking executable status:" << std::endl;
    for (const auto& file : filesToCheck) {
        bool isExec = atom::io::isExecutableFile(file);
        std::cout << "  " << file << " -> "
                  << (isExec ? "Executable" : "Not executable") << std::endl;
    }

    // Search for executable files (this would work better with actual
    // executables)
    std::cout << "\nSearching for files containing 'script':" << std::endl;
    auto foundFiles =
        atom::io::searchExecutableFiles("test_analysis", "script");
    for (const auto& file : foundFiles) {
        std::cout << "  🔍 " << file << std::endl;
    }
}

/**
 * @brief Cleans up test structure
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up test structure..." << std::endl;

    if (fs::exists("test_analysis")) {
        fs::remove_all("test_analysis");
    }
    if (fs::exists("directory_structure.json")) {
        fs::remove("directory_structure.json");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🔍 Atom I/O Directory Traversal Examples" << std::endl;
        std::cout << "=========================================" << std::endl;

        // Setup
        createTestStructure();

        // Run demonstrations
        demonstrateDirectoryWalking();
        demonstrateJsonExport();
        demonstrateFileClassification();
        demonstrateFileTypeAnalysis();
        demonstrateLineCounting();
        demonstrateFileTimeInfo();
        demonstrateExecutableSearch();

        // Cleanup
        cleanup();

        std::cout
            << "\n🎉 All directory traversal operations completed successfully!"
            << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
