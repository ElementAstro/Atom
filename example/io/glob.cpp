#include <filesystem>
#include <fstream>
#include <iostream>

#include "atom/io/glob.hpp"

namespace fs = std::filesystem;

void demonstrateGlobFunctions() {
    // Specify a directory to search in (Make sure this folder exists)
    const std::string testDirectory =
        ".";  // Using current directory for testing

    // Create some test files for demonstration purposes
    fs::create_directory("test_dir");
    std::ofstream("test_dir/file1.txt");  // Create file1.txt
    std::ofstream("test_dir/file2.cpp");  // Create file2.cpp
    std::ofstream("test_dir/file3.md");   // Create file3.md
    std::ofstream("test_dir/file4.txt");  // Create another text file
    std::ofstream("test_dir/file5.doc");  // Create a non-matching doc file

    // Example: Using glob
    std::cout << "Using glob to find .txt files:\n";
    auto txtFiles = atom::io::glob(atom::containers::String{"test_dir/*.txt"}, false, false);
    for (const auto& file : txtFiles) {
        std::cout << " - " << file << '\n';
    }

    // Example: Using rglob (recursive glob)
    std::cout << "Using rglob to find .cpp files:\n";
    auto cppFiles = atom::io::rglob("test_dir/**/*.cpp");
    for (const auto& file : cppFiles) {
        std::cout << " - " << file << '\n';
    }

    // Example: Using glob with multiple patterns
    std::cout << "Using glob with multiple file patterns:\n";
    std::vector<std::string> patterns = {"test_dir/*.txt", "test_dir/*.md"};
    auto matchedFiles = atom::io::glob(std::vector<std::string>{"test_dir/*.txt", "test_dir/*.md"});
    for (const auto& file : matchedFiles) {
        std::cout << " - " << file << '\n';
    }

    // Clean up: Remove the test directory and its contents
    fs::remove_all("test_dir");
}

int main() {
    demonstrateGlobFunctions();
    return 0;
}
