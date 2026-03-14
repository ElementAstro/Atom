/*
 * test_io_main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_io_main.cpp
 * @brief Integration tests for atom::io core module
 *
 * Verifies that combined file operations (create, query, split, merge, cleanup)
 * work correctly together in an end-to-end workflow.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "atom/io/core/io.hpp"

#include "atom/io/core/io.hpp"

namespace fs = std::filesystem;

class IoIntegrationTest : public ::testing::Test {
protected:
    fs::path test_dir;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "atom_io_integration_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        try {
            if (fs::exists(test_dir)) {
                fs::remove_all(test_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Cleanup error: " << e.what() << std::endl;
        }
    }

    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
        ASSERT_TRUE(fs::exists(path));
    }
};

// End-to-end workflow: create files, query, split, merge, cleanup
TEST_F(IoIntegrationTest, FileLifecycleWorkflow) {
    // 1. Create sample files
    fs::path sample_dir = test_dir / "sample_dir";
    ASSERT_TRUE(atom::io::createDirectory(sample_dir));

    createTestFile(sample_dir / "file1.txt", "Contents of file 1.\n");
    createTestFile(sample_dir / "file2.txt", "Contents of file 2.\n");
    createTestFile(sample_dir / "file3.txt", "Contents of file 3.\n");

    // 2. Verify folder and file existence
    EXPECT_TRUE(atom::io::isFolderExists(sample_dir));

    std::vector<fs::path> filenames = {
        sample_dir / "file1.txt", sample_dir / "file2.txt",
        sample_dir / "file3.txt"};

    for (const auto& filename : filenames) {
        EXPECT_TRUE(atom::io::isFileExists(filename));
    }

    // 3. Get file sizes
    for (const auto& filename : filenames) {
        EXPECT_GT(atom::io::fileSize(filename), 0u);
    }

    // 4. Split a file
    fs::path file_to_split = sample_dir / "file1.txt";
    size_t chunk_size = 10;
    atom::io::splitFile(file_to_split, chunk_size);

    // Verify split files exist
    bool found_parts = false;
    for (int i = 0; i < 5; ++i) {
        fs::path part = fs::path(file_to_split.string() + ".part" +
                                 std::to_string(i));
        if (fs::exists(part)) {
            found_parts = true;
        }
    }
    EXPECT_TRUE(found_parts);

    // 5. Merge split files
    fs::path merged_file = sample_dir / "merged_file1.txt";
    std::vector<std::string> part_files;
    for (int i = 0; fs::exists(fs::path(file_to_split.string() + ".part" +
                                        std::to_string(i)));
         ++i) {
        part_files.push_back(file_to_split.string() + ".part" +
                             std::to_string(i));
    }
    ASSERT_FALSE(part_files.empty());

    atom::io::mergeFiles(merged_file, part_files);
    EXPECT_TRUE(fs::exists(merged_file));
    EXPECT_EQ(fs::file_size(merged_file), fs::file_size(file_to_split));

    // 6. Copy and move operations
    fs::path copied = sample_dir / "file1_copy.txt";
    EXPECT_TRUE(atom::io::copyFile(filenames[0], copied));
    EXPECT_TRUE(fs::exists(copied));

    fs::path moved = sample_dir / "file1_moved.txt";
    EXPECT_TRUE(atom::io::moveFile(copied, moved));
    EXPECT_FALSE(fs::exists(copied));
    EXPECT_TRUE(fs::exists(moved));

    // 7. Cleanup
    EXPECT_TRUE(atom::io::removeDirectory(sample_dir));
    EXPECT_FALSE(fs::exists(sample_dir));
}

// End-to-end workflow: directory operations
TEST_F(IoIntegrationTest, DirectoryLifecycleWorkflow) {
    fs::path base = test_dir / "dir_lifecycle";
    std::vector<std::string> subdirs = {"alpha", "beta", "gamma/delta"};

    // Create recursive
    EXPECT_TRUE(atom::io::createDirectoriesRecursive(base, subdirs));
    for (const auto& sub : subdirs) {
        EXPECT_TRUE(fs::exists(base / sub));
    }

    // Verify folder is not empty
    EXPECT_FALSE(atom::io::isFolderEmpty(base));

    // Walk and verify
    std::string json = atom::io::jwalk(base);
    EXPECT_FALSE(json.empty());

    std::vector<fs::path> walked_paths;
    atom::io::fwalk(base, [&walked_paths](const fs::path& p) {
        walked_paths.push_back(p);
    });
    EXPECT_GE(walked_paths.size(), 3u);

    // Remove recursive
    EXPECT_TRUE(atom::io::removeDirectoriesRecursive(base, subdirs));
}

// End-to-end workflow: file classification
TEST_F(IoIntegrationTest, FileClassificationWorkflow) {
    createTestFile(test_dir / "doc1.txt", "text1");
    createTestFile(test_dir / "doc2.txt", "text2");
    createTestFile(test_dir / "img.jpg", "jpeg");
    createTestFile(test_dir / "code.cpp", "cpp");
    createTestFile(test_dir / "data.json", "json");

    // Classify
    auto classified = atom::io::classifyFiles(test_dir);
    EXPECT_TRUE(classified.contains(".txt"));
    EXPECT_EQ(classified[".txt"].size(), 2u);
    EXPECT_TRUE(classified.contains(".jpg"));
    EXPECT_TRUE(classified.contains(".cpp"));
    EXPECT_TRUE(classified.contains(".json"));

    // Check file types in folder
    std::vector<std::string> exts = {".txt", ".cpp"};
    auto found = atom::io::checkFileTypeInFolder(test_dir, exts,
                                                 atom::io::FileOption::NAME);
    EXPECT_EQ(found.size(), 3u);
}
