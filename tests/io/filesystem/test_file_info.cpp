#include <gtest/gtest.h>
#include "atom/io/core/io.hpp"
#include "atom/io/filesystem/file_info.hpp"
#include "atom/io/filesystem/file_permission.hpp"

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

// Test getFileInfo with files of different sizes
TEST_F(FileInfoTest, FilesOfDifferentSizes) {
    // Empty file
    fs::path empty_file = test_dir / "empty.txt";
    std::ofstream(empty_file).close();
    auto info = atom::io::getFileInfo(empty_file);
    EXPECT_EQ(info.fileSize, 0);

    // Small file
    fs::path small_file = test_dir / "small.txt";
    std::ofstream small(small_file);
    small << "Small";
    small.close();
    info = atom::io::getFileInfo(small_file);
    EXPECT_EQ(info.fileSize, 5);

    // Larger file
    fs::path large_file = test_dir / "large.txt";
    std::ofstream large(large_file);
    large << std::string(10000, 'X');
    large.close();
    info = atom::io::getFileInfo(large_file);
    EXPECT_EQ(info.fileSize, 10000);
}

// Test getFileInfo with files without extensions
TEST_F(FileInfoTest, FilesWithoutExtension) {
    fs::path no_ext = test_dir / "README";
    std::ofstream file(no_ext);
    file << "No extension file";
    file.close();

    auto info = atom::io::getFileInfo(no_ext);
    EXPECT_EQ(info.fileName, "README");
    EXPECT_EQ(info.extension, "");
    EXPECT_EQ(info.fileType, "Regular file");
}

// Test getFileInfo with files with multiple dots
TEST_F(FileInfoTest, FilesWithMultipleDots) {
    fs::path multi_dot = test_dir / "archive.tar.gz";
    std::ofstream file(multi_dot);
    file << "Archive file";
    file.close();

    auto info = atom::io::getFileInfo(multi_dot);
    EXPECT_EQ(info.fileName, "archive.tar.gz");
    EXPECT_EQ(info.extension, ".gz");  // Only last extension
}

// Test deleteFile with read-only file
TEST_F(FileInfoTest, DeleteReadOnlyFile) {
    fs::path readonly_file = test_dir / "readonly.txt";
    std::ofstream file(readonly_file);
    file << "Read-only content";
    file.close();

    // Make file read-only
    fs::permissions(readonly_file, fs::perms::owner_read,
                    fs::perm_options::replace);

    // Deletion should still work (or throw appropriate exception)
    // Behavior may vary by platform
#ifdef _WIN32
    // On Windows, read-only files can typically be deleted
    EXPECT_NO_THROW(atom::io::deleteFile(readonly_file));
#else
    // On Unix, read-only files in writable directories can be deleted
    EXPECT_NO_THROW(atom::io::deleteFile(readonly_file));
#endif
}

// Test printFileInfo with various file types
TEST_F(FileInfoTest, PrintFileInfoVariousTypes) {
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    // Print regular file info
    auto reg_info = atom::io::getFileInfo(regular_file);
    EXPECT_NO_THROW(atom::io::printFileInfo(reg_info));

    // Print directory info
    auto dir_info = atom::io::getFileInfo(directory_path);
    EXPECT_NO_THROW(atom::io::printFileInfo(dir_info));

    // Print hidden file info
    auto hidden_info = atom::io::getFileInfo(hidden_file);
    EXPECT_NO_THROW(atom::io::printFileInfo(hidden_info));

    std::cout.rdbuf(old);

    // Verify output contains expected information
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("File Path:") != std::string::npos);
    EXPECT_TRUE(output.find("File Name:") != std::string::npos);
    EXPECT_TRUE(output.find("File Size:") != std::string::npos);
}

// Test renameFile with same source and destination
TEST_F(FileInfoTest, RenameSameSourceDest) {
    // Renaming to the same path should either succeed or handle gracefully
    EXPECT_NO_THROW(atom::io::renameFile(regular_file, regular_file));
    EXPECT_TRUE(fs::exists(regular_file));
}

// Test changeFilePermissions with various permission strings
TEST_F(FileInfoTest, ChangePermissionsVariousStrings) {
    // Test all read permissions
    EXPECT_NO_THROW(atom::io::changeFilePermissions(regular_file, "r--r--r--"));
    auto info = atom::io::getFileInfo(regular_file);
    EXPECT_EQ(info.permissions, "r--r--r--");

    // Test all write permissions
    EXPECT_NO_THROW(atom::io::changeFilePermissions(regular_file, "-w--w--w-"));
    info = atom::io::getFileInfo(regular_file);
    EXPECT_EQ(info.permissions, "-w--w--w-");

    // Test all execute permissions
    EXPECT_NO_THROW(atom::io::changeFilePermissions(regular_file, "--x--x--x"));
    info = atom::io::getFileInfo(regular_file);
    EXPECT_EQ(info.permissions, "--x--x--x");

    // Test mixed permissions
    EXPECT_NO_THROW(atom::io::changeFilePermissions(regular_file, "rwxr-xr--"));
    info = atom::io::getFileInfo(regular_file);
    EXPECT_EQ(info.permissions, "rwxr-xr--");

    // Restore readable permissions for cleanup
    fs::permissions(regular_file,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
}

// Test getFileInfo with very long filenames
TEST_F(FileInfoTest, VeryLongFilename) {
    // Create a file with a long name (but within filesystem limits)
    std::string long_name(200, 'a');
    long_name += ".txt";
    fs::path long_file = test_dir / long_name;

    try {
        std::ofstream file(long_file);
        file << "Long filename test";
        file.close();

        auto info = atom::io::getFileInfo(long_file);
        EXPECT_EQ(info.fileName, long_name);
        EXPECT_EQ(info.extension, ".txt");

        fs::remove(long_file);
    } catch (const std::exception& e) {
        // Some filesystems may not support such long names
        GTEST_SKIP() << "Filesystem doesn't support long filenames: "
                     << e.what();
    }
}

// Test file operations with Unicode filenames
TEST_F(FileInfoTest, UnicodeFilenames) {
    // Create a file with Unicode characters in the name
    std::string unicode_name = "test_文件_файл_αρχείο.txt";
    fs::path unicode_file = test_dir / unicode_name;

    try {
        std::ofstream file(unicode_file);
        file << "Unicode filename test";
        file.close();

        auto info = atom::io::getFileInfo(unicode_file);
        EXPECT_EQ(info.fileName, unicode_name);
        EXPECT_TRUE(fs::exists(unicode_file));

        // Test deletion
        EXPECT_NO_THROW(atom::io::deleteFile(unicode_file));
        EXPECT_FALSE(fs::exists(unicode_file));
    } catch (const std::exception& e) {
        // Some filesystems may not support Unicode
        GTEST_SKIP() << "Filesystem doesn't support Unicode filenames: "
                     << e.what();
    }
}

// Test getFileInfo error handling with invalid paths
TEST_F(FileInfoTest, InvalidPathHandling) {
    // Test with null characters (if filesystem allows)
    // Test with very long path
    std::string very_long_path = test_dir.string();
    for (int i = 0; i < 100; i++) {
        very_long_path += "/very_long_directory_name_" + std::to_string(i);
    }

    EXPECT_THROW(atom::io::getFileInfo(very_long_path), std::runtime_error);
}

// Test concurrent file info retrieval
TEST_F(FileInfoTest, ConcurrentGetFileInfo) {
    // Create multiple test files
    std::vector<fs::path> test_files;
    for (int i = 0; i < 10; i++) {
        fs::path file = test_dir / ("concurrent_" + std::to_string(i) + ".txt");
        std::ofstream f(file);
        f << "Concurrent test " << i;
        f.close();
        test_files.push_back(file);
    }

    // Get file info concurrently
    std::vector<std::future<atom::io::FileInfo>> futures;
    for (const auto& file : test_files) {
        futures.push_back(std::async(std::launch::async, [&file]() {
            return atom::io::getFileInfo(file);
        }));
    }

    // Verify all operations completed successfully
    for (size_t i = 0; i < futures.size(); i++) {
        EXPECT_NO_THROW({
            auto info = futures[i].get();
            EXPECT_FALSE(info.fileName.empty());
            EXPECT_GT(info.fileSize, 0);
        });
    }

    // Cleanup
    for (const auto& file : test_files) {
        fs::remove(file);
    }
}

// Test file info for very large files
TEST_F(FileInfoTest, VeryLargeFileInfo) {
    fs::path large_file = test_dir / "large_file.dat";

    // Create a large file (100 MB)
    std::ofstream ofs(large_file, std::ios::binary);
    std::vector<char> buffer(1024 * 1024, 'X');  // 1 MB buffer
    for (int i = 0; i < 100; ++i) {
        ofs.write(buffer.data(), buffer.size());
    }
    ofs.close();

    auto fileInfo = atom::io::getFileInfo(large_file);
    EXPECT_GT(fileInfo.fileSize, 100 * 1024 * 1024);
    EXPECT_EQ(fileInfo.fileType, "Regular file");
}

// Test file info for files with special characters
TEST_F(FileInfoTest, SpecialCharactersInFilename) {
    std::vector<std::string> special_names = {
        "file with spaces.txt", "file(with)parens.txt",
        "file[with]brackets.txt", "file-with-dashes.txt"};

    for (const auto& name : special_names) {
        fs::path special_file = test_dir / name;
        std::ofstream ofs(special_file);
        ofs << "Special file content";
        ofs.close();

        auto fileInfo = atom::io::getFileInfo(special_file);
        EXPECT_EQ(fileInfo.fileName, name);
        EXPECT_GT(fileInfo.fileSize, 0);
    }
}

// Test file info for empty files
TEST_F(FileInfoTest, EmptyFileInfo) {
    fs::path empty_file = test_dir / "empty.txt";
    std::ofstream(empty_file).close();

    auto fileInfo = atom::io::getFileInfo(empty_file);
    EXPECT_EQ(fileInfo.fileSize, 0);
    EXPECT_EQ(fileInfo.fileType, "Regular file");
    EXPECT_FALSE(fileInfo.permissions.empty());
}

// Test file info for binary files
TEST_F(FileInfoTest, BinaryFileInfo) {
    fs::path binary_file = test_dir / "binary.dat";
    std::ofstream ofs(binary_file, std::ios::binary);
    std::vector<unsigned char> binary_data = {0x00, 0xFF, 0x7F, 0x80};
    ofs.write(reinterpret_cast<const char*>(binary_data.data()),
              binary_data.size());
    ofs.close();

    auto fileInfo = atom::io::getFileInfo(binary_file);
    EXPECT_EQ(fileInfo.fileSize, binary_data.size());
    EXPECT_EQ(fileInfo.extension, ".dat");
}

// Test concurrent file info retrieval
TEST_F(FileInfoTest, ConcurrentFileInfoRetrieval) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            auto fileInfo = atom::io::getFileInfo(regular_file);
            if (fileInfo.fileSize > 0) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// Test file info with different file extensions
TEST_F(FileInfoTest, DifferentFileExtensions) {
    std::vector<std::string> extensions = {".txt", ".cpp", ".hpp",
                                           ".dat", ".log", ""};

    for (const auto& ext : extensions) {
        fs::path file = test_dir / ("testfile" + ext);
        std::ofstream(file) << "Test content";

        auto fileInfo = atom::io::getFileInfo(file);
        EXPECT_EQ(fileInfo.extension, ext);
    }
}

// Test printFileInfo doesn't crash
TEST_F(FileInfoTest, PrintFileInfoNoCrash) {
    auto fileInfo = atom::io::getFileInfo(regular_file);

    // Redirect cout to capture output
    std::stringstream buffer;
    std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

    EXPECT_NO_THROW(atom::io::printFileInfo(fileInfo));

    // Restore cout
    std::cout.rdbuf(old);

    // Verify some output was produced
    EXPECT_FALSE(buffer.str().empty());
}

// Test deleteFile function
TEST_F(FileInfoTest, DeleteFileFunction) {
    fs::path temp_file = test_dir / "to_delete.txt";
    std::ofstream(temp_file) << "Delete me";

    EXPECT_TRUE(fs::exists(temp_file));

    EXPECT_NO_THROW(atom::io::deleteFile(temp_file));
    EXPECT_FALSE(fs::exists(temp_file));

    // Try deleting non-existent file
    EXPECT_THROW(atom::io::deleteFile(temp_file), std::runtime_error);
}
