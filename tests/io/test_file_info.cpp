#include <gtest/gtest.h>
#include "file_info.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <vector>

namespace fs = std::filesystem;

class FileInfoTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path regular_file;
    fs::path directory_path;
    fs::path symlink_path;
    fs::path hidden_file;
    fs::path non_existent_file;
    fs::path executable_file;

    void SetUp() override {
        // Create test directory structure
        test_dir = fs::temp_directory_path() / "atom_file_info_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);

        // Create a regular file
        regular_file = test_dir / "regular_file.txt";
        std::ofstream reg_file(regular_file);
        reg_file << "This is a test file content";
        reg_file.close();

        // Create a directory
        directory_path = test_dir / "test_directory";
        fs::create_directories(directory_path);

        // Create a hidden file
        hidden_file = test_dir / ".hidden_file";
        std::ofstream hidden(hidden_file);
        hidden << "This is a hidden file content";
        hidden.close();

        // Create an executable file
        executable_file = test_dir / "executable_file";
        std::ofstream exec_file(executable_file);
        exec_file << "#!/bin/bash\necho \"Hello, World!\"";
        exec_file.close();

        // Set executable permissions
        fs::permissions(executable_file,
                        fs::perms::owner_exec | fs::perms::group_exec,
                        fs::perm_options::add);

        // Create a symbolic link (where supported)
        symlink_path = test_dir / "symlink";
        try {
            fs::create_symlink(regular_file, symlink_path);
        } catch (const fs::filesystem_error&) {
            // Symlinks might not be supported on all platforms/environments
            symlink_path.clear();
        }

        non_existent_file = test_dir / "non_existent_file.txt";
    }

    void TearDown() override {
        // Clean up test directory
        try {
            fs::remove_all(test_dir);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error cleaning up: " << e.what() << std::endl;
        }
    }
};

// Test getting file info for a regular file
TEST_F(FileInfoTest, GetFileInfoRegularFile) {
    auto fileInfo = atom::io::getFileInfo(regular_file);

    // Validate basic properties
    EXPECT_EQ(fileInfo.filePath, fs::absolute(regular_file).string());
    EXPECT_EQ(fileInfo.fileName, "regular_file.txt");
    EXPECT_EQ(fileInfo.extension, ".txt");
    EXPECT_GT(fileInfo.fileSize, 0);
    EXPECT_EQ(fileInfo.fileType, "Regular file");
    EXPECT_FALSE(fileInfo.lastModifiedTime.empty());
    EXPECT_FALSE(fileInfo.permissions.empty());
    EXPECT_FALSE(fileInfo.isHidden);

    // Permissions should be in format like "rwxr-xr-x"
    EXPECT_EQ(fileInfo.permissions.length(), 9);
    for (char c : fileInfo.permissions) {
        EXPECT_TRUE(c == 'r' || c == 'w' || c == 'x' || c == '-');
    }

    // Owner should be available (but we can't predict the exact value)
    EXPECT_FALSE(fileInfo.owner.empty());

#ifndef _WIN32
    // Group should be available on Unix-like systems
    EXPECT_FALSE(fileInfo.group.empty());
#endif
}

// Test getting file info for a directory
TEST_F(FileInfoTest, GetFileInfoDirectory) {
    auto fileInfo = atom::io::getFileInfo(directory_path);

    EXPECT_EQ(fileInfo.filePath, fs::absolute(directory_path).string());
    EXPECT_EQ(fileInfo.fileName, "test_directory");
    EXPECT_EQ(fileInfo.extension, "");
    EXPECT_EQ(fileInfo.fileType, "Directory");
    EXPECT_FALSE(fileInfo.isHidden);
}

// Test getting file info for a hidden file
TEST_F(FileInfoTest, GetFileInfoHiddenFile) {
    auto fileInfo = atom::io::getFileInfo(hidden_file);

    EXPECT_EQ(fileInfo.filePath, fs::absolute(hidden_file).string());
    EXPECT_EQ(fileInfo.fileName, ".hidden_file");
#ifdef _WIN32
// On Windows, hidden attribute is set via file attributes
// We might need to manually set FILE_ATTRIBUTE_HIDDEN to make this test pass
// consistently
#else
    // On Unix-like systems, files starting with . are hidden
    EXPECT_TRUE(fileInfo.isHidden);
#endif
}

// Test getting file info for a symbolic link (where supported)
TEST_F(FileInfoTest, GetFileInfoSymlink) {
    if (symlink_path.empty()) {
        GTEST_SKIP()
            << "Symlink creation not supported on this platform/environment";
    }

    auto fileInfo = atom::io::getFileInfo(symlink_path);

    EXPECT_EQ(fileInfo.filePath, fs::absolute(symlink_path).string());
    EXPECT_EQ(fileInfo.fileName, "symlink");
    EXPECT_EQ(fileInfo.extension, "");
    EXPECT_EQ(fileInfo.fileType, "Symbolic link");

#ifndef _WIN32
    // Check symlink target on non-Windows platforms
    EXPECT_EQ(fileInfo.symlinkTarget, regular_file.string());
#endif
}

// Test getting file info for a non-existent file (should throw)
TEST_F(FileInfoTest, GetFileInfoNonExistentFile) {
    EXPECT_THROW(
        { atom::io::getFileInfo(non_existent_file); }, std::runtime_error);
}

// Test getting file info for an executable file
TEST_F(FileInfoTest, GetFileInfoExecutableFile) {
    auto fileInfo = atom::io::getFileInfo(executable_file);

    EXPECT_EQ(fileInfo.filePath, fs::absolute(executable_file).string());
    EXPECT_EQ(fileInfo.fileName, "executable_file");

    // Check if execute permission is set in the permission string
    // The permission string format is "rwxrwxrwx"
    // Position 2 is owner execute, 5 is group execute, 8 is others execute
    EXPECT_EQ(fileInfo.permissions[2],
              'x');  // Owner should have execute permission
    EXPECT_EQ(fileInfo.permissions[5],
              'x');  // Group should have execute permission
}

// Test with empty path (should throw)
TEST_F(FileInfoTest, GetFileInfoEmptyPath) {
    EXPECT_THROW({ atom::io::getFileInfo(""); }, std::invalid_argument);
}

// Test printFileInfo (should not throw, but output is hard to verify in unit
// tests)
TEST_F(FileInfoTest, PrintFileInfo) {
    auto fileInfo = atom::io::getFileInfo(regular_file);

    // Redirect cout to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    EXPECT_NO_THROW({ atom::io::printFileInfo(fileInfo); });

    // Restore cout
    std::cout.rdbuf(old);

    // Verify that output contains key elements
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("File Path:") != std::string::npos);
    EXPECT_TRUE(output.find("File Name:") != std::string::npos);
    EXPECT_TRUE(output.find("regular_file.txt") != std::string::npos);
}

// Test file operations: renaming a file
TEST_F(FileInfoTest, RenameFile) {
    fs::path new_path = test_dir / "renamed_file.txt";

    ASSERT_TRUE(fs::exists(regular_file));
    ASSERT_FALSE(fs::exists(new_path));

    EXPECT_NO_THROW({ atom::io::renameFile(regular_file, new_path); });

    EXPECT_FALSE(fs::exists(regular_file));
    EXPECT_TRUE(fs::exists(new_path));

    // Get info about renamed file
    auto fileInfo = atom::io::getFileInfo(new_path);
    EXPECT_EQ(fileInfo.fileName, "renamed_file.txt");

    // Reset for other tests
    fs::rename(new_path, regular_file);
}

// Test file operations: changing permissions
TEST_F(FileInfoTest, ChangeFilePermissions) {
    // Original permissions
    auto originalInfo = atom::io::getFileInfo(regular_file);

    // New permissions: read-write for owner only
    std::string newPermissions = "rw-------";

    EXPECT_NO_THROW(
        { atom::io::changeFilePermissions(regular_file, newPermissions); });

    // Check that permissions have changed
    auto newInfo = atom::io::getFileInfo(regular_file);
    EXPECT_EQ(newInfo.permissions, newPermissions);

// Restore to readable for tests to continue
#ifdef _WIN32
    // Windows has different permission semantics
    fs::permissions(regular_file,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
#else
    // Unix-like: restore to a default readable state
    fs::permissions(regular_file,
                    fs::perms::owner_read | fs::perms::owner_write |
                        fs::perms::group_read | fs::perms::others_read,
                    fs::perm_options::replace);
#endif
}

// Test file operations: deleting a file
TEST_F(FileInfoTest, DeleteFile) {
    // Create a temporary file for deletion
    fs::path temp_file = test_dir / "temp_to_delete.txt";
    std::ofstream temp(temp_file);
    temp << "This file will be deleted";
    temp.close();

    ASSERT_TRUE(fs::exists(temp_file));

    EXPECT_NO_THROW({ atom::io::deleteFile(temp_file); });

    EXPECT_FALSE(fs::exists(temp_file));
}

// Test file operations: trying to rename to existing file (should throw)
TEST_F(FileInfoTest, RenameToExistingFile) {
    // Create another file
    fs::path another_file = test_dir / "another_file.txt";
    std::ofstream another(another_file);
    another << "This is another file";
    another.close();

    EXPECT_THROW(
        { atom::io::renameFile(regular_file, another_file); },
        std::runtime_error);

    // Cleanup
    fs::remove(another_file);
}

// Test file operations: trying to delete non-existent file (should throw)
TEST_F(FileInfoTest, DeleteNonExistentFile) {
    EXPECT_THROW(
        { atom::io::deleteFile(non_existent_file); }, std::runtime_error);
}

// Test file operations: changing permissions of non-existent file (should
// throw)
TEST_F(FileInfoTest, ChangePermissionsNonExistentFile) {
    EXPECT_THROW(
        { atom::io::changeFilePermissions(non_existent_file, "rwxrwxrwx"); },
        std::runtime_error);
}

// Test file operations: changing permissions with invalid string (should throw
// or handle safely)
TEST_F(FileInfoTest, ChangePermissionsInvalidString) {
    EXPECT_THROW(
        { atom::io::changeFilePermissions(regular_file, "invalid"); },
        std::runtime_error);

    EXPECT_THROW(
        { atom::io::changeFilePermissions(regular_file, ""); },
        std::runtime_error);

    EXPECT_THROW(
        {
            atom::io::changeFilePermissions(regular_file,
                                            "rwxrwxrwxrwx");  // Too long
        },
        std::runtime_error);
}

// Test thread safety: concurrent operations on different files
TEST_F(FileInfoTest, ConcurrentFileOperations) {
    // Create multiple files
    std::vector<fs::path> test_files;
    for (int i = 0; i < 5; i++) {
        fs::path file =
            test_dir / ("concurrent_test_" + std::to_string(i) + ".txt");
        std::ofstream f(file);
        f << "Test content " << i;
        f.close();
        test_files.push_back(file);
    }

    // Run multiple operations concurrently
    std::vector<std::future<atom::io::FileInfo>> futures;
    for (const auto& file : test_files) {
        futures.push_back(std::async(std::launch::async, [&file]() {
            return atom::io::getFileInfo(file);
        }));
    }

    // Wait for all operations to complete
    for (auto& future : futures) {
        EXPECT_NO_THROW({
            auto info = future.get();
            EXPECT_FALSE(info.fileName.empty());
        });
    }

    // Cleanup
    for (const auto& file : test_files) {
        fs::remove(file);
    }
}

// Test large file handling
TEST_F(FileInfoTest, LargeFile) {
    // Create a moderately large file (5 MB)
    fs::path large_file = test_dir / "large_file.dat";
    std::ofstream large(large_file, std::ios::binary);

    const size_t SIZE = 5 * 1024 * 1024;  // 5 MB
    std::vector<char> buffer(SIZE, 'X');
    large.write(buffer.data(), SIZE);
    large.close();

    auto start = std::chrono::high_resolution_clock::now();
    auto fileInfo = atom::io::getFileInfo(large_file);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    // Verify correct size
    EXPECT_EQ(fileInfo.fileSize, SIZE);

    // Performance test - should be reasonably fast even for larger files
    // This is somewhat arbitrary but helps catch major performance regressions
    EXPECT_LT(duration, 1000);  // Should take less than 1 second

    // Cleanup
    fs::remove(large_file);
}

// Test handling paths with special characters
TEST_F(FileInfoTest, SpecialCharactersInPath) {
    // Create a file with special characters in the name
    std::string special_filename = "special-char_file!@#$%^&()_+.txt";
    fs::path special_file = test_dir / special_filename;

    try {
        std::ofstream file(special_file);
        file << "File with special characters in the name";
        file.close();

        EXPECT_NO_THROW({
            auto fileInfo = atom::io::getFileInfo(special_file);
            EXPECT_EQ(fileInfo.fileName, special_filename);
        });

        // Cleanup
        fs::remove(special_file);
    } catch (const std::exception& e) {
        // Some filesystems might not support all special characters
        GTEST_SKIP() << "Filesystem doesn't support the special characters: "
                     << e.what();
    }
}

// Test performance with many files
TEST_F(FileInfoTest, DISABLED_ManyFilesPerformance) {
    // This test creates many files to test performance - disabled by default
    std::vector<fs::path> many_files;
    const int FILE_COUNT = 100;

    // Create many small files
    for (int i = 0; i < FILE_COUNT; i++) {
        fs::path file = test_dir / ("perf_test_" + std::to_string(i) + ".txt");
        std::ofstream f(file);
        f << "Small test content " << i;
        f.close();
        many_files.push_back(file);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Process all files
    for (const auto& file : many_files) {
        auto fileInfo = atom::io::getFileInfo(file);
        EXPECT_FALSE(fileInfo.fileName.empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Processing " << FILE_COUNT << " files took: " << duration
              << "ms" << std::endl;

    // Performance assertion (adjust based on expected performance)
    EXPECT_LT(duration, FILE_COUNT * 10);  // Rough estimate: < 10ms per file

    // Cleanup
    for (const auto& file : many_files) {
        fs::remove(file);
    }
}
