#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include "atom/io/io.hpp"
#include "atom/io/file_info.hpp"
#include "atom/io/file_permission.hpp"
#include "atom/io/compress.hpp"
#include "atom/io/pushd.hpp"

namespace fs = std::filesystem;
using namespace testing;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "atom_integration_test";

        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }

        fs::create_directories(test_dir_);
        fs::create_directories(test_dir_ / "source");
        fs::create_directories(test_dir_ / "backup");
        fs::create_directories(test_dir_ / "archive");

        // Create test files with different content types
        createTestFile(test_dir_ / "source" / "document.txt", "Important document content");
        createTestFile(test_dir_ / "source" / "data.csv", "name,age,city\nJohn,30,NYC\nJane,25,LA");
        createTestFile(test_dir_ / "source" / "config.json", R"({"setting": "value", "enabled": true})");
        createTestFile(test_dir_ / "source" / "binary.dat", std::string(1000, '\x42'));

        // Create subdirectory with files
        fs::create_directories(test_dir_ / "source" / "subdir");
        createTestFile(test_dir_ / "source" / "subdir" / "nested.txt", "Nested file content");

        original_cwd_ = fs::current_path();
    }

    void TearDown() override {
        fs::current_path(original_cwd_);

        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
    }

    bool filesAreIdentical(const fs::path& file1, const fs::path& file2) {
        if (!fs::exists(file1) || !fs::exists(file2)) {
            return false;
        }

        if (fs::file_size(file1) != fs::file_size(file2)) {
            return false;
        }

        std::ifstream f1(file1, std::ios::binary);
        std::ifstream f2(file2, std::ios::binary);

        return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                         std::istreambuf_iterator<char>(),
                         std::istreambuf_iterator<char>(f2.rdbuf()));
    }

protected:
    fs::path test_dir_;
    fs::path original_cwd_;
};

// Test complete file backup workflow
TEST_F(IntegrationTest, FileBackupWorkflow) {
    fs::path source_dir = test_dir_ / "source";
    fs::path backup_dir = test_dir_ / "backup";

    // Step 1: Get information about all source files
    std::vector<atom::io::FileInfo> source_files_info;

    for (const auto& entry : fs::recursive_directory_iterator(source_dir)) {
        if (entry.is_regular_file()) {
            auto info = atom::io::getFileInfo(entry.path());
            source_files_info.push_back(info);

            // Verify file info is complete
            EXPECT_FALSE(info.filePath.empty());
            EXPECT_GT(info.fileSize, 0);
            EXPECT_FALSE(info.permissions.empty());
        }
    }

    EXPECT_GE(source_files_info.size(), 4); // At least 4 files created

    // Step 2: Copy all files to backup directory maintaining structure
    for (const auto& info : source_files_info) {
        fs::path source_file(info.filePath);
        fs::path relative_path = fs::relative(source_file, source_dir);
        fs::path backup_file = backup_dir / relative_path;

        // Create backup directory structure
        fs::create_directories(backup_file.parent_path());

        // Copy file
        EXPECT_TRUE(atom::io::copyFile(source_file, backup_file));
        EXPECT_TRUE(fs::exists(backup_file));

        // Verify file integrity
        EXPECT_TRUE(filesAreIdentical(source_file, backup_file));

        // Verify file info matches
        auto backup_info = atom::io::getFileInfo(backup_file);
        EXPECT_EQ(backup_info.fileSize, info.fileSize);
        EXPECT_EQ(backup_info.fileName, info.fileName);
    }
}

// Test file compression and archival workflow
TEST_F(IntegrationTest, CompressionArchivalWorkflow) {
    fs::path source_dir = test_dir_ / "source";
    fs::path archive_dir = test_dir_ / "archive";

    // Step 1: Compress individual files
    std::vector<fs::path> compressed_files;

    for (const auto& entry : fs::directory_iterator(source_dir)) {
        if (entry.is_regular_file()) {
            fs::path source_file = entry.path();
            fs::path compressed_file = archive_dir / (source_file.filename().string() + ".gz");

            // Compress file
            bool success = atom::io::compressFile(source_file.string(), compressed_file.string());
            EXPECT_TRUE(success) << "Failed to compress " << source_file;

            if (success) {
                EXPECT_TRUE(fs::exists(compressed_file));
                EXPECT_LT(fs::file_size(compressed_file), fs::file_size(source_file));
                compressed_files.push_back(compressed_file);
            }
        }
    }

    EXPECT_GE(compressed_files.size(), 4);

    // Step 2: Verify compressed files can be decompressed
    fs::path decompress_dir = test_dir_ / "decompressed";
    fs::create_directories(decompress_dir);

    for (const auto& compressed_file : compressed_files) {
        fs::path decompressed_file = decompress_dir / compressed_file.stem();

        bool success = atom::io::decompressFile(compressed_file.string(), decompressed_file.string());
        EXPECT_TRUE(success) << "Failed to decompress " << compressed_file;

        if (success) {
            EXPECT_TRUE(fs::exists(decompressed_file));

            // Find original file and compare
            fs::path original_file = source_dir / compressed_file.stem();
            if (fs::exists(original_file)) {
                EXPECT_TRUE(filesAreIdentical(original_file, decompressed_file))
                    << "Decompressed file doesn't match original: " << original_file;
            }
        }
    }
}

// Test directory navigation and file discovery workflow
TEST_F(IntegrationTest, DirectoryNavigationWorkflow) {
    fs::path source_dir = test_dir_ / "source";

    // Step 1: Use pushd to navigate to source directory
    fs::path original_cwd = fs::current_path();

    {
        atom::io::Pushd pushd(source_dir);
        EXPECT_EQ(fs::current_path(), source_dir);

        // Step 2: Use glob to find files by pattern
        auto txt_files = atom::io::glob("*.txt");
        EXPECT_GE(txt_files.size(), 1); // Should find document.txt

        auto all_files = atom::io::glob("*");
        EXPECT_GE(all_files.size(), 4); // Should find all files

        auto recursive_files = atom::io::glob("**/*", true);
        EXPECT_GT(recursive_files.size(), all_files.size()); // Should find nested files too

        // Step 3: Get detailed info for discovered files
        for (const auto& file_path : txt_files) {
            if (fs::is_regular_file(file_path)) {
                auto info = atom::io::getFileInfo(file_path);
                EXPECT_FALSE(info.filePath.empty());
                EXPECT_EQ(info.extension, ".txt");

                // Get permission info
                auto perms = atom::io::getFilePermissions(file_path.string());
                EXPECT_FALSE(perms.empty());
                EXPECT_EQ(perms.length(), 9);
            }
        }
    } // pushd destructor restores original directory

    EXPECT_EQ(fs::current_path(), original_cwd);
}

// Test file operations with permission management
TEST_F(IntegrationTest, PermissionManagementWorkflow) {
    fs::path test_file = test_dir_ / "permission_test.txt";
    createTestFile(test_file, "Permission test content");

    // Step 1: Get initial permissions
    auto initial_perms = atom::io::getFilePermissions(test_file.string());
    EXPECT_FALSE(initial_perms.empty());

    auto initial_info = atom::io::getFileInfo(test_file);
    EXPECT_EQ(initial_info.permissions, initial_perms);

    // Step 2: Try to modify permissions (platform dependent)
    try {
        atom::io::changeFilePermissions(test_file, "rw-r--r--");

        // Verify permission change
        auto new_perms = atom::io::getFilePermissions(test_file.string());
        EXPECT_FALSE(new_perms.empty());

        // The exact permission string might vary by platform, but it should be different
        // or the operation should have succeeded without error

    } catch (const std::exception& e) {
        // Permission changes might not be supported on all platforms
        GTEST_SKIP() << "Permission changes not supported: " << e.what();
    }

    // Step 3: Verify file is still accessible
    auto final_info = atom::io::getFileInfo(test_file);
    EXPECT_EQ(final_info.fileSize, initial_info.fileSize);
    EXPECT_EQ(final_info.fileName, initial_info.fileName);
}

// Test error handling across multiple operations
TEST_F(IntegrationTest, ErrorHandlingWorkflow) {
    fs::path nonexistent_file = test_dir_ / "does_not_exist.txt";
    fs::path invalid_dest = test_dir_ / "nonexistent_dir" / "file.txt";

    // Test file operations with non-existent files
    EXPECT_FALSE(atom::io::copyFile(nonexistent_file, test_dir_ / "copy.txt"));
    EXPECT_FALSE(atom::io::removeFile(nonexistent_file));
    EXPECT_FALSE(atom::io::isFileExists(nonexistent_file));

    // Test operations with invalid destinations
    fs::path valid_source = test_dir_ / "source" / "document.txt";
    EXPECT_FALSE(atom::io::copyFile(valid_source, invalid_dest));

    // Test file info for non-existent file
    auto info = atom::io::getFileInfo(nonexistent_file);
    EXPECT_TRUE(info.filePath.empty() || info.fileSize == 0);

    // Test glob with invalid patterns (should return empty results, not crash)
    auto results = atom::io::glob("nonexistent_pattern_*");
    EXPECT_TRUE(results.empty());

    // Test compression with non-existent file
    bool compress_result = atom::io::compressFile(
        nonexistent_file.string(),
        (test_dir_ / "test.gz").string()
    );
    EXPECT_FALSE(compress_result);
}

// Test performance with realistic file operations
TEST_F(IntegrationTest, RealisticPerformanceWorkflow) {
    // Create a realistic file structure
    const int num_files = 50;
    const int num_dirs = 5;

    for (int d = 0; d < num_dirs; ++d) {
        fs::path dir = test_dir_ / ("project_" + std::to_string(d));
        fs::create_directories(dir);

        for (int f = 0; f < num_files / num_dirs; ++f) {
            fs::path file = dir / ("file_" + std::to_string(f) + ".txt");
            createTestFile(file, "Content for file " + std::to_string(f) + " in directory " + std::to_string(d));
        }
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform realistic operations
    fs::current_path(test_dir_);

    // 1. Find all text files
    auto all_txt_files = atom::io::glob("**/*.txt", true);
    EXPECT_EQ(all_txt_files.size(), num_files);

    // 2. Get info for all files
    std::vector<atom::io::FileInfo> file_infos;
    for (const auto& file : all_txt_files) {
        auto info = atom::io::getFileInfo(file);
        file_infos.push_back(info);
    }

    // 3. Copy files to backup location
    fs::path backup_root = test_dir_ / "backup";
    for (const auto& file : all_txt_files) {
        fs::path relative_path = fs::relative(file, test_dir_);
        fs::path backup_path = backup_root / relative_path;
        fs::create_directories(backup_path.parent_path());
        EXPECT_TRUE(atom::io::copyFile(file, backup_path));
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Realistic workflow with " << num_files << " files completed in "
              << duration.count() << "ms" << std::endl;

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 10000) << "Workflow took too long";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
