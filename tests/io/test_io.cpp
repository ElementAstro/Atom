#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include "atom/io/io.hpp"

namespace fs = std::filesystem;

class IoTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path test_file;
    fs::path non_existent_path;

    void SetUp() override {
        // Create test directory with unique name to avoid conflicts
        test_dir = fs::temp_directory_path() / "atom_io_test";

        // Clean up any previous test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }

        // Create directory for tests
        fs::create_directories(test_dir);

        // Create a test file
        test_file = test_dir / "test_file.txt";
        std::ofstream file(test_file);
        file << "This is a test file for IoTest.\n";
        file << "It has multiple lines.\n";
        file << "This is the third line.\n";
        file.close();

        // Define a non-existent path
        non_existent_path = test_dir / "non_existent";
    }

    void TearDown() override {
        try {
            // Clean up test directory
            if (fs::exists(test_dir)) {
                fs::remove_all(test_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during test cleanup: " << e.what() << std::endl;
        }
    }

    // Helper method to create a file with specific content
    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
        ASSERT_TRUE(fs::exists(path));
    }

    // Helper method to read content from a file
    std::string readTestFile(const fs::path& path) {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    // Helper method to create a larger test file
    void createLargeTestFile(const fs::path& path, size_t size_kb) {
        std::ofstream file(path, std::ios::binary);

        // Use random data to avoid compression effects
        std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<int> dist(0, 255);

        std::vector<char> buffer(1024);
        for (size_t i = 0; i < size_kb; ++i) {
            // Fill buffer with random data
            for (char& c : buffer) {
                c = static_cast<char>(dist(rng));
            }
            file.write(buffer.data(), buffer.size());
        }
        file.close();
    }
};

// Test directory creation
TEST_F(IoTest, CreateDirectory) {
    fs::path new_dir = test_dir / "new_directory";

    EXPECT_FALSE(fs::exists(new_dir));
    EXPECT_TRUE(atom::io::createDirectory(new_dir));
    EXPECT_TRUE(fs::exists(new_dir));
    EXPECT_TRUE(fs::is_directory(new_dir));

    // Test with existing directory (should return false)
    EXPECT_FALSE(atom::io::createDirectory(new_dir));

    // Test with empty path
    EXPECT_FALSE(atom::io::createDirectory(""));
}

// Test recursive directory creation
TEST_F(IoTest, CreateDirectoriesRecursive) {
    fs::path base_dir = test_dir / "base";
    std::vector<std::string> subdirs = {"dir1", "dir2", "dir3/subdir"};

    EXPECT_FALSE(fs::exists(base_dir));
    EXPECT_TRUE(atom::io::createDirectoriesRecursive(base_dir, subdirs));

    // Verify directories were created
    EXPECT_TRUE(fs::exists(base_dir / "dir1"));
    EXPECT_TRUE(fs::exists(base_dir / "dir2"));
    EXPECT_TRUE(fs::exists(base_dir / "dir3/subdir"));

    // Test with custom options
    subdirs = {"dir4", "dir5"};
    atom::io::CreateDirectoriesOptions options;
    options.verbose = false;
    options.delay = 10;

    bool creation_callback_called = false;
    options.onCreate = [&creation_callback_called](std::string_view) {
        creation_callback_called = true;
    };

    EXPECT_TRUE(
        atom::io::createDirectoriesRecursive(base_dir, subdirs, options));
    EXPECT_TRUE(creation_callback_called);
    EXPECT_TRUE(fs::exists(base_dir / "dir4"));
    EXPECT_TRUE(fs::exists(base_dir / "dir5"));

    // Test with custom filter
    subdirs = {"allowed", "filtered"};
    options.filter = [](std::string_view path) { return path != "filtered"; };

    EXPECT_TRUE(
        atom::io::createDirectoriesRecursive(base_dir, subdirs, options));
    EXPECT_TRUE(fs::exists(base_dir / "allowed"));
    EXPECT_FALSE(fs::exists(base_dir / "filtered"));
}

// Test directory removal
TEST_F(IoTest, RemoveDirectory) {
    fs::path dir_to_remove = test_dir / "dir_to_remove";
    fs::create_directories(dir_to_remove);
    ASSERT_TRUE(fs::exists(dir_to_remove));

    EXPECT_TRUE(atom::io::removeDirectory(dir_to_remove));
    EXPECT_FALSE(fs::exists(dir_to_remove));

    // Test with non-existent directory
    EXPECT_TRUE(
        atom::io::removeDirectory(dir_to_remove));  // Should still return true

    // Test with empty path
    EXPECT_FALSE(atom::io::removeDirectory(""));
}

// Test recursive directory removal
TEST_F(IoTest, RemoveDirectoriesRecursive) {
    fs::path base_dir = test_dir / "base_remove";
    fs::create_directories(base_dir);

    std::vector<std::string> subdirs = {"dir1", "dir2", "dir3/subdir"};
    for (const auto& subdir : subdirs) {
        fs::create_directories(base_dir / subdir);
    }

    ASSERT_TRUE(fs::exists(base_dir / "dir3/subdir"));

    // Test removal
    EXPECT_TRUE(atom::io::removeDirectoriesRecursive(base_dir, subdirs));
    EXPECT_FALSE(fs::exists(base_dir / "dir1"));
    EXPECT_FALSE(fs::exists(base_dir / "dir2"));
    EXPECT_FALSE(fs::exists(base_dir / "dir3"));

    // Test with custom options
    fs::create_directories(base_dir / "dir4");
    fs::create_directories(base_dir / "dir5");

    subdirs = {"dir4", "dir5"};
    atom::io::CreateDirectoriesOptions options;
    options.verbose = false;

    bool deletion_callback_called = false;
    options.onDelete = [&deletion_callback_called](std::string_view) {
        deletion_callback_called = true;
    };

    EXPECT_TRUE(
        atom::io::removeDirectoriesRecursive(base_dir, subdirs, options));
    EXPECT_TRUE(deletion_callback_called);
    EXPECT_FALSE(fs::exists(base_dir / "dir4"));
    EXPECT_FALSE(fs::exists(base_dir / "dir5"));
}

// Test file copying
TEST_F(IoTest, CopyFile) {
    fs::path dest_file = test_dir / "copied_file.txt";

    EXPECT_FALSE(fs::exists(dest_file));
    EXPECT_TRUE(atom::io::copyFile(test_file, dest_file));
    EXPECT_TRUE(fs::exists(dest_file));

    // Verify content
    EXPECT_EQ(readTestFile(test_file), readTestFile(dest_file));

    // Test copying to non-existent directory
    fs::path dest_in_new_dir = test_dir / "new_dir" / "copied_file.txt";
    EXPECT_TRUE(atom::io::copyFile(test_file, dest_in_new_dir));
    EXPECT_TRUE(fs::exists(dest_in_new_dir));

    // Test with empty paths
    EXPECT_FALSE(atom::io::copyFile("", dest_file));
    EXPECT_FALSE(atom::io::copyFile(test_file, ""));

    // Test with non-existent source
    EXPECT_FALSE(atom::io::copyFile(non_existent_path, dest_file));
}

// Test file moving
TEST_F(IoTest, MoveFile) {
    fs::path source_file = test_dir / "move_source.txt";
    fs::path dest_file = test_dir / "moved_file.txt";

    // Create source file
    createTestFile(source_file, "This is a file to be moved.");

    EXPECT_TRUE(fs::exists(source_file));
    EXPECT_FALSE(fs::exists(dest_file));

    EXPECT_TRUE(atom::io::moveFile(source_file, dest_file));
    EXPECT_FALSE(fs::exists(source_file));
    EXPECT_TRUE(fs::exists(dest_file));

    // Test moving to non-existent directory
    source_file = test_dir / "move_source2.txt";
    dest_file = test_dir / "new_dir2" / "moved_file.txt";

    createTestFile(source_file, "Another file to be moved.");
    EXPECT_TRUE(atom::io::moveFile(source_file, dest_file));
    EXPECT_FALSE(fs::exists(source_file));
    EXPECT_TRUE(fs::exists(dest_file));
}

// Test file renaming
TEST_F(IoTest, RenameFile) {
    fs::path source_file = test_dir / "rename_source.txt";
    fs::path dest_file = test_dir / "renamed_file.txt";

    // Create source file
    createTestFile(source_file, "This is a file to be renamed.");

    EXPECT_TRUE(atom::io::renameFile(source_file, dest_file));
    EXPECT_FALSE(fs::exists(source_file));
    EXPECT_TRUE(fs::exists(dest_file));
}

// Test file removal
TEST_F(IoTest, RemoveFile) {
    fs::path file_to_remove = test_dir / "file_to_remove.txt";
    createTestFile(file_to_remove, "This file will be removed.");

    EXPECT_TRUE(fs::exists(file_to_remove));
    EXPECT_TRUE(atom::io::removeFile(file_to_remove));
    EXPECT_FALSE(fs::exists(file_to_remove));

    // Test with non-existent file
    EXPECT_TRUE(
        atom::io::removeFile(file_to_remove));  // Should still return true

    // Test with empty path
    EXPECT_FALSE(atom::io::removeFile(""));
}

// Test symlink creation and removal
TEST_F(IoTest, SymlinkOperations) {
// Skip this test on Windows if running without admin privileges
#ifdef _WIN32
    if (!fs::exists("C:\\Windows\\System32\\cmd.exe")) {
        GTEST_SKIP()
            << "Skipping symlink test on Windows without admin privileges";
    }
#endif

    fs::path link_path = test_dir / "test_link";

    EXPECT_FALSE(fs::exists(link_path));
    EXPECT_TRUE(atom::io::createSymlink(test_file, link_path));

    ASSERT_TRUE(fs::exists(link_path));
    EXPECT_TRUE(fs::is_symlink(link_path));

    // Test symlink removal
    EXPECT_TRUE(atom::io::removeSymlink(link_path));
    EXPECT_FALSE(fs::exists(link_path));
}

// Test file size functions
TEST_F(IoTest, FileSizeFunctions) {
    // Test fileSize function
    EXPECT_GT(atom::io::fileSize(test_file), 0);
    EXPECT_EQ(atom::io::fileSize(non_existent_path), 0);

    // Test getFileSize function
    EXPECT_GT(atom::io::getFileSize(test_file), 0);
    EXPECT_EQ(atom::io::getFileSize(non_existent_path), 0);

    // Verify both functions return the same value
    EXPECT_EQ(atom::io::fileSize(test_file), atom::io::getFileSize(test_file));
}

// Test file truncation
TEST_F(IoTest, TruncateFile) {
    fs::path truncate_file = test_dir / "truncate_file.txt";
    createTestFile(truncate_file,
                   "This is a long string that will be truncated.");

    size_t new_size = 10;

    EXPECT_TRUE(atom::io::truncateFile(truncate_file, new_size));
    EXPECT_EQ(fs::file_size(truncate_file), new_size);

    // Test with invalid size
    EXPECT_FALSE(atom::io::truncateFile(truncate_file, -1));

    // Test with non-existent file
    EXPECT_FALSE(atom::io::truncateFile(non_existent_path, 5));
}

// Test jwalk function
TEST_F(IoTest, JsonWalk) {
    // Create directory structure for testing
    fs::path walk_dir = test_dir / "walk_test";
    fs::create_directories(walk_dir / "subdir1");
    fs::create_directories(walk_dir / "subdir2");

    createTestFile(walk_dir / "file1.txt", "File 1");
    createTestFile(walk_dir / "subdir1/file2.txt", "File 2");

    std::string json_str = atom::io::jwalk(walk_dir);
    EXPECT_FALSE(json_str.empty());

    // Parse JSON and verify structure
    auto json = nlohmann::json::parse(json_str);
    EXPECT_EQ(json["path"], walk_dir.generic_string());
    EXPECT_TRUE(json.contains("directories"));
    EXPECT_TRUE(json.contains("files"));

    // Test with non-existent directory
    EXPECT_TRUE(atom::io::jwalk(non_existent_path).empty());
}

// Test fwalk function
TEST_F(IoTest, FileWalk) {
    // Create directory structure for testing
    fs::path walk_dir = test_dir / "fwalk_test";
    fs::create_directories(walk_dir / "subdir1");
    fs::create_directories(walk_dir / "subdir2");

    createTestFile(walk_dir / "file1.txt", "File 1");
    createTestFile(walk_dir / "subdir1/file2.txt", "File 2");
    createTestFile(walk_dir / "subdir2/file3.txt", "File 3");

    std::vector<fs::path> found_files;

    atom::io::fwalk(walk_dir, [&found_files](const fs::path& path) {
        if (fs::is_regular_file(path)) {
            found_files.push_back(path);
        }
    });

    EXPECT_EQ(found_files.size(), 3);
}

// Test path conversion functions
TEST_F(IoTest, PathConversionFunctions) {
    // Test Windows to Linux path conversion
    std::string win_path = "C:\\Users\\test\\Documents\\file.txt";
    std::string linux_path = atom::io::convertToLinuxPath(win_path);
    EXPECT_EQ(linux_path, "c:/Users/test/Documents/file.txt");

    // Test Linux to Windows path conversion
    std::string win_path2 =
        atom::io::convertToWindowsPath("/home/user/Documents/file.txt");
#ifdef _WIN32
    EXPECT_EQ(win_path2, "\\home\\user\\Documents\\file.txt");
#else
    EXPECT_EQ(win_path2, "\\home\\user\\Documents\\file.txt");
#endif

    // Test normPath function
    std::string path_with_dots = "../test/../folder/./file.txt";
    std::string normalized = atom::io::normPath(path_with_dots);
    EXPECT_NE(normalized, path_with_dots);

    // Test with absolute paths
    std::string abs_path = "/home/user/../user/./Documents";
    normalized = atom::io::normPath(abs_path);
    EXPECT_NE(normalized, abs_path);
}

// Test folder and file name validation
TEST_F(IoTest, NameValidation) {
    // Test folder name validation
    EXPECT_TRUE(atom::io::isFolderNameValid("valid_folder"));
    EXPECT_TRUE(atom::io::isFolderNameValid("valid folder with spaces"));
    EXPECT_FALSE(atom::io::isFolderNameValid(""));

// Invalid characters depend on platform
#ifdef _WIN32
    EXPECT_FALSE(atom::io::isFolderNameValid("folder?with:invalid*chars"));
    EXPECT_FALSE(atom::io::isFolderNameValid("folder/with/slashes"));
#else
    EXPECT_FALSE(atom::io::isFolderNameValid("folder/with/slashes"));
#endif

    // Test file name validation
    EXPECT_TRUE(atom::io::isFileNameValid("valid_file.txt"));
    EXPECT_TRUE(atom::io::isFileNameValid("valid file with spaces.doc"));
    EXPECT_FALSE(atom::io::isFileNameValid(""));

// Invalid characters depend on platform
#ifdef _WIN32
    EXPECT_FALSE(atom::io::isFileNameValid("file?with:invalid*chars.txt"));
    EXPECT_FALSE(atom::io::isFileNameValid("file/with/slashes.txt"));
#else
    EXPECT_FALSE(atom::io::isFileNameValid("file/with/slashes.txt"));
#endif
}

// Test existence checking functions
TEST_F(IoTest, ExistenceChecking) {
    // Test folder existence
    EXPECT_TRUE(atom::io::isFolderExists(test_dir));
    EXPECT_FALSE(atom::io::isFolderExists(non_existent_path));
    EXPECT_FALSE(atom::io::isFolderExists(test_file));

    // Test file existence
    EXPECT_TRUE(atom::io::isFileExists(test_file));
    EXPECT_FALSE(atom::io::isFileExists(non_existent_path));
    EXPECT_FALSE(atom::io::isFileExists(test_dir));

    // Test folder emptiness
    fs::path empty_dir = test_dir / "empty_dir";
    fs::create_directories(empty_dir);

    EXPECT_TRUE(atom::io::isFolderEmpty(empty_dir));
    EXPECT_FALSE(atom::io::isFolderEmpty(test_dir));
    EXPECT_FALSE(atom::io::isFolderEmpty(non_existent_path));
}

// Test absolute path checking
TEST_F(IoTest, AbsolutePathChecking) {
    EXPECT_TRUE(atom::io::isAbsolutePath(test_dir));
    EXPECT_FALSE(atom::io::isAbsolutePath("relative/path"));
}

// Test working directory changing
TEST_F(IoTest, ChangeWorkingDirectory) {
    fs::path original_path = fs::current_path();

    EXPECT_TRUE(atom::io::changeWorkingDirectory(test_dir));
    EXPECT_EQ(fs::current_path(), test_dir);

    // Test with non-existent directory
    EXPECT_FALSE(atom::io::changeWorkingDirectory(non_existent_path));

    // Restore original working directory
    fs::current_path(original_path);
}

// Test file time functions
TEST_F(IoTest, FileTimes) {
    auto times = atom::io::getFileTimes(test_file);

    // We expect a creation time and modification time
    // Creation time might not be available on all platforms
    EXPECT_FALSE(times.second.empty());

    // Test with non-existent file
    auto nonexistent_times = atom::io::getFileTimes(non_existent_path);
    EXPECT_TRUE(nonexistent_times.first.empty());
    EXPECT_TRUE(nonexistent_times.second.empty());
}

// Test file type checking in folder
TEST_F(IoTest, CheckFileTypeInFolder) {
    // Create files with different extensions
    createTestFile(test_dir / "test1.txt", "Text file");
    createTestFile(test_dir / "test2.txt", "Another text file");
    createTestFile(test_dir / "image.jpg", "JPEG data");
    createTestFile(test_dir / "doc.pdf", "PDF data");

    // Test with PATH option
    std::vector<std::string> extensions = {".txt"};
    auto files = atom::io::checkFileTypeInFolder(test_dir, extensions,
                                                 atom::io::FileOption::PATH);
    EXPECT_EQ(files.size(), 2);

    // Test with NAME option
    files = atom::io::checkFileTypeInFolder(test_dir, extensions,
                                            atom::io::FileOption::NAME);
    EXPECT_EQ(files.size(), 2);
    for (const auto& file : files) {
        EXPECT_TRUE(file == "test1.txt" || file == "test2.txt");
    }

    // Test with multiple extensions
    extensions = {".txt", ".pdf"};
    files = atom::io::checkFileTypeInFolder(test_dir, extensions,
                                            atom::io::FileOption::NAME);
    EXPECT_EQ(files.size(), 3);
}

// Test executable file checking
TEST_F(IoTest, ExecutableFileChecking) {
#ifdef _WIN32
    // On Windows, create a .bat file which is considered executable
    fs::path exec_file = test_dir / "test.bat";
    createTestFile(exec_file, "@echo Hello World");

    EXPECT_TRUE(atom::io::isExecutableFile(test_dir / "test", ".bat"));
#else
    // On Unix, create a file and make it executable
    fs::path exec_file = test_dir / "test_exec";
    createTestFile(exec_file, "#!/bin/sh\necho Hello World");
    fs::permissions(exec_file, fs::perms::owner_exec, fs::perm_options::add);

    EXPECT_TRUE(atom::io::isExecutableFile(exec_file));
#endif

    // Test with non-existent file
    EXPECT_FALSE(atom::io::isExecutableFile(non_existent_path));
}

// Test chunk size calculation
TEST_F(IoTest, ChunkSizeCalculation) {
    EXPECT_EQ(atom::io::calculateChunkSize(1000, 10), 100);
    EXPECT_EQ(atom::io::calculateChunkSize(1001, 10), 101);
    EXPECT_EQ(atom::io::calculateChunkSize(1000, 0),
              1000);  // Ensure no division by zero
}

// Test file splitting and merging
TEST_F(IoTest, FileSplittingAndMerging) {
    fs::path large_file = test_dir / "large_file.bin";
    size_t file_size_kb = 100;  // 100KB
    createLargeTestFile(large_file, file_size_kb);

    // Test splitFile
    atom::io::splitFile(large_file, 20 * 1024);  // 20KB chunks

    // Check if part files were created
    EXPECT_TRUE(fs::exists(fs::path(large_file.string() + ".part0")));
    EXPECT_TRUE(fs::exists(fs::path(large_file.string() + ".part1")));
    EXPECT_TRUE(fs::exists(fs::path(large_file.string() + ".part2")));
    EXPECT_TRUE(fs::exists(fs::path(large_file.string() + ".part3")));
    EXPECT_TRUE(fs::exists(fs::path(large_file.string() + ".part4")));

    // Prepare for merging
    fs::path merged_file = test_dir / "merged_file.bin";
    std::vector<std::string> part_files;
    for (int i = 0; i < 5; i++) {
        part_files.push_back(large_file.string() + ".part" + std::to_string(i));
    }

    // Test mergeFiles
    atom::io::mergeFiles(merged_file, part_files);
    EXPECT_TRUE(fs::exists(merged_file));
    EXPECT_EQ(fs::file_size(merged_file), fs::file_size(large_file));

    // Compare content
    std::vector<char> original_content(file_size_kb * 1024);
    std::vector<char> merged_content(file_size_kb * 1024);

    std::ifstream original_file(large_file, std::ios::binary);
    std::ifstream merged_file_stream(merged_file, std::ios::binary);

    original_file.read(original_content.data(), original_content.size());
    merged_file_stream.read(merged_content.data(), merged_content.size());

    EXPECT_EQ(std::memcmp(original_content.data(), merged_content.data(),
                          file_size_kb * 1024),
              0);
}

// Test quick split and merge
TEST_F(IoTest, QuickSplitAndMerge) {
    fs::path large_file = test_dir / "quick_file.bin";
    size_t file_size_kb = 50;  // 50KB
    createLargeTestFile(large_file, file_size_kb);

    // Test quickSplit
    int num_chunks = 5;
    atom::io::quickSplit(large_file, num_chunks);

    // Check if part files were created
    for (int i = 0; i < num_chunks; i++) {
        EXPECT_TRUE(fs::exists(
            fs::path(large_file.string() + ".part" + std::to_string(i))));
    }

    // Test quickMerge
    fs::path merged_file = test_dir / "quick_merged.bin";
    atom::io::quickMerge(merged_file, large_file.string(), num_chunks);

    EXPECT_TRUE(fs::exists(merged_file));
    EXPECT_EQ(fs::file_size(merged_file), fs::file_size(large_file));
}

// Test path type checking
TEST_F(IoTest, CheckPathType) {
    EXPECT_EQ(atom::io::checkPathType(test_dir), atom::io::PathType::DIRECTORY);
    EXPECT_EQ(atom::io::checkPathType(test_file),
              atom::io::PathType::REGULAR_FILE);
    EXPECT_EQ(atom::io::checkPathType(non_existent_path),
              atom::io::PathType::NOT_EXISTS);

// Create and test symlink if possible
#ifndef _WIN32
    fs::path link_path = test_dir / "sym_link";
    fs::create_symlink(test_file, link_path);
    EXPECT_EQ(atom::io::checkPathType(link_path), atom::io::PathType::SYMLINK);
#endif
}

// Test line counting
TEST_F(IoTest, CountLinesInFile) {
    // Our test file has 3 lines
    auto line_count = atom::io::countLinesInFile(test_file);
    EXPECT_TRUE(line_count.has_value());
    EXPECT_EQ(line_count.value(), 3);

    // Test with non-existent file
    EXPECT_FALSE(atom::io::countLinesInFile(non_existent_path).has_value());

    // Test with directory (should fail)
    EXPECT_FALSE(atom::io::countLinesInFile(test_dir).has_value());
}

// Test executable file searching
TEST_F(IoTest, SearchExecutableFiles) {
#ifdef _WIN32
    // On Windows, create some batch files
    createTestFile(test_dir / "test_cmd.bat", "@echo test");
    createTestFile(test_dir / "other.bat", "@echo other");
    createTestFile(test_dir / "not_executable.txt", "text");

    auto found_files = atom::io::searchExecutableFiles(test_dir, "test");
    EXPECT_EQ(found_files.size(), 1);
#else
    // On Unix, create some executable files
    fs::path exec1 = test_dir / "test_exec";
    fs::path exec2 = test_dir / "other_exec";
    fs::path not_exec = test_dir / "not_executable.txt";

    createTestFile(exec1, "#!/bin/sh\necho test");
    createTestFile(exec2, "#!/bin/sh\necho other");
    createTestFile(not_exec, "text");

    fs::permissions(exec1, fs::perms::owner_exec, fs::perm_options::add);
    fs::permissions(exec2, fs::perms::owner_exec, fs::perm_options::add);

    auto found_files = atom::io::searchExecutableFiles(test_dir, "test");
    EXPECT_GE(found_files.size(), 1);
#endif

    // Test with non-existent directory
    found_files = atom::io::searchExecutableFiles(non_existent_path, "test");
    EXPECT_EQ(found_files.size(), 0);
}
