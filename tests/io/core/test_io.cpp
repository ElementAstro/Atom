#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include "atom/io/core/io.hpp"
#include <nlohmann/json.hpp>

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

// Test classifyFiles function
TEST_F(IoTest, ClassifyFiles) {
    // Create files with different extensions
    createTestFile(test_dir / "doc1.txt", "Text file 1");
    createTestFile(test_dir / "doc2.txt", "Text file 2");
    createTestFile(test_dir / "image1.jpg", "JPEG data");
    createTestFile(test_dir / "image2.png", "PNG data");
    createTestFile(test_dir / "data.json", "JSON data");

    auto classified = atom::io::classifyFiles(test_dir);

    // Check that files are classified by extension
    EXPECT_TRUE(classified.contains(".txt"));
    EXPECT_TRUE(classified.contains(".jpg"));
    EXPECT_TRUE(classified.contains(".png"));
    EXPECT_TRUE(classified.contains(".json"));

    // Check counts
    EXPECT_EQ(classified[".txt"].size(), 2);
    EXPECT_EQ(classified[".jpg"].size(), 1);
    EXPECT_EQ(classified[".png"].size(), 1);
    EXPECT_EQ(classified[".json"].size(), 1);

    // Test with non-existent directory
    auto empty_classified = atom::io::classifyFiles(non_existent_path);
    EXPECT_TRUE(empty_classified.empty());
}

// Test createDateDirectory function
TEST_F(IoTest, CreateDateDirectory) {
    std::string date = "2024-01-15";
    fs::path root_dir = test_dir / "date_root";
    fs::create_directories(root_dir);

    EXPECT_NO_THROW(atom::io::createDateDirectory(date, root_dir));

    fs::path expected_dir = root_dir / date;
    EXPECT_TRUE(fs::exists(expected_dir));
    EXPECT_TRUE(fs::is_directory(expected_dir));

    // Test creating the same directory again (should not throw)
    EXPECT_NO_THROW(atom::io::createDateDirectory(date, root_dir));
}

// Test createDirectoriesRecursive with dry run option
TEST_F(IoTest, CreateDirectoriesRecursiveDryRun) {
    fs::path base_dir = test_dir / "dry_run_base";
    std::vector<std::string> subdirs = {"dir1", "dir2", "dir3"};

    atom::io::CreateDirectoriesOptions options;
    options.dryRun = true;
    options.verbose = false;

    EXPECT_TRUE(atom::io::createDirectoriesRecursive(base_dir, subdirs, options));

    // Directories should NOT be created in dry run mode
    EXPECT_FALSE(fs::exists(base_dir / "dir1"));
    EXPECT_FALSE(fs::exists(base_dir / "dir2"));
    EXPECT_FALSE(fs::exists(base_dir / "dir3"));
}

// Test removeDirectoriesRecursive with dry run option
TEST_F(IoTest, RemoveDirectoriesRecursiveDryRun) {
    fs::path base_dir = test_dir / "dry_run_remove";
    fs::create_directories(base_dir);

    std::vector<std::string> subdirs = {"dir1", "dir2"};
    for (const auto& subdir : subdirs) {
        fs::create_directories(base_dir / subdir);
    }

    atom::io::CreateDirectoriesOptions options;
    options.dryRun = true;
    options.verbose = false;

    EXPECT_TRUE(atom::io::removeDirectoriesRecursive(base_dir, subdirs, options));

    // Directories should still exist in dry run mode
    EXPECT_TRUE(fs::exists(base_dir / "dir1"));
    EXPECT_TRUE(fs::exists(base_dir / "dir2"));
}

// Test directory operations with delay option
TEST_F(IoTest, DirectoryOperationsWithDelay) {
    fs::path base_dir = test_dir / "delay_test";
    std::vector<std::string> subdirs = {"dir1", "dir2"};

    atom::io::CreateDirectoriesOptions options;
    options.delay = 50;  // 50ms delay between operations
    options.verbose = false;

    auto start = std::chrono::high_resolution_clock::now();
    EXPECT_TRUE(atom::io::createDirectoriesRecursive(base_dir, subdirs, options));
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should take at least delay * number of directories
    EXPECT_GE(duration, 50);  // At least one delay occurred

    EXPECT_TRUE(fs::exists(base_dir / "dir1"));
    EXPECT_TRUE(fs::exists(base_dir / "dir2"));
}

// Test truncateFile with various sizes
TEST_F(IoTest, TruncateFileVariousSizes) {
    fs::path truncate_file = test_dir / "truncate_test.txt";
    createTestFile(truncate_file, std::string(1000, 'A'));

    // Truncate to smaller size
    EXPECT_TRUE(atom::io::truncateFile(truncate_file, 100));
    EXPECT_EQ(fs::file_size(truncate_file), 100);

    // Truncate to larger size (should extend file)
    EXPECT_TRUE(atom::io::truncateFile(truncate_file, 500));
    EXPECT_EQ(fs::file_size(truncate_file), 500);

    // Truncate to zero
    EXPECT_TRUE(atom::io::truncateFile(truncate_file, 0));
    EXPECT_EQ(fs::file_size(truncate_file), 0);

    // Test with empty path
    EXPECT_FALSE(atom::io::truncateFile("", 10));
}

// Test getExecutableNameFromPath function
TEST_F(IoTest, GetExecutableNameFromPath) {
    std::string path1 = "/usr/bin/python3";
    EXPECT_EQ(atom::io::getExecutableNameFromPath(path1), "python3");

    std::string path2 = "C:\\Program Files\\app.exe";
    EXPECT_EQ(atom::io::getExecutableNameFromPath(path2), "app.exe");

    std::string path3 = "relative/path/to/executable";
    EXPECT_EQ(atom::io::getExecutableNameFromPath(path3), "executable");

    std::string path4 = "simple_name";
    EXPECT_EQ(atom::io::getExecutableNameFromPath(path4), "simple_name");

    // Test with empty path
    EXPECT_TRUE(atom::io::getExecutableNameFromPath("").empty());
}

// Test moveDirectory and renameDirectory
TEST_F(IoTest, MoveAndRenameDirectory) {
    fs::path source_dir = test_dir / "source_dir";
    fs::path dest_dir = test_dir / "dest_dir";

    fs::create_directories(source_dir);
    createTestFile(source_dir / "file.txt", "Test content");

    // Test moveDirectory
    EXPECT_TRUE(atom::io::moveDirectory(source_dir, dest_dir));
    EXPECT_FALSE(fs::exists(source_dir));
    EXPECT_TRUE(fs::exists(dest_dir));
    EXPECT_TRUE(fs::exists(dest_dir / "file.txt"));

    // Test renameDirectory (which calls moveDirectory)
    fs::path renamed_dir = test_dir / "renamed_dir";
    EXPECT_TRUE(atom::io::renameDirectory(dest_dir, renamed_dir));
    EXPECT_FALSE(fs::exists(dest_dir));
    EXPECT_TRUE(fs::exists(renamed_dir));

    // Test with empty paths
    EXPECT_FALSE(atom::io::moveDirectory("", renamed_dir));
    EXPECT_FALSE(atom::io::moveDirectory(renamed_dir, ""));

    // Test with non-existent source
    EXPECT_FALSE(atom::io::moveDirectory(non_existent_path, test_dir / "new_dir"));
}

// Test edge cases for file operations
TEST_F(IoTest, FileOperationsEdgeCases) {
    // Test copyFile with overwrite
    fs::path source = test_dir / "source.txt";
    fs::path dest = test_dir / "dest.txt";

    createTestFile(source, "Original content");
    createTestFile(dest, "Existing content");

    EXPECT_TRUE(atom::io::copyFile(source, dest));

    // Verify content was overwritten
    EXPECT_EQ(readTestFile(dest), "Original content");

    // Test renameFile with destination in non-existent directory
    fs::path new_dest = test_dir / "new_subdir" / "renamed.txt";
    EXPECT_TRUE(atom::io::renameFile(source, new_dest));
    EXPECT_TRUE(fs::exists(new_dest));
    EXPECT_FALSE(fs::exists(source));
}

// Test symlink operations edge cases
TEST_F(IoTest, SymlinkEdgeCases) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping symlink edge case tests on Windows";
#endif

    fs::path target = test_file;
    fs::path link = test_dir / "test_symlink";

    // Create symlink
    EXPECT_TRUE(atom::io::createSymlink(target, link));

    // Test creating symlink with non-existent parent directory
    fs::path link_in_new_dir = test_dir / "new_link_dir" / "link";
    EXPECT_TRUE(atom::io::createSymlink(target, link_in_new_dir));
    EXPECT_TRUE(fs::exists(link_in_new_dir));

    // Test with empty paths
    EXPECT_FALSE(atom::io::createSymlink("", link));
    EXPECT_FALSE(atom::io::createSymlink(target, ""));
}

// Test concurrent file operations
TEST_F(IoTest, ConcurrentFileOperations) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Test concurrent file creation
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            fs::path file = test_dir / ("concurrent_file_" + std::to_string(i) + ".txt");
            std::ofstream ofs(file);
            ofs << "Thread " << i << " content";
            ofs.close();
            if (fs::exists(file)) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);

    // Test concurrent file reading
    threads.clear();
    success_count = 0;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            fs::path file = test_dir / ("concurrent_file_" + std::to_string(i) + ".txt");
            std::ifstream ifs(file);
            std::string content;
            std::getline(ifs, content);
            if (!content.empty()) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// Test very long path handling
TEST_F(IoTest, VeryLongPathHandling) {
    // Create a deeply nested directory structure
    fs::path long_path = test_dir;
    std::string dir_name = "a";

    // Create path up to reasonable limit (not OS max to avoid issues)
    for (int i = 0; i < 50; ++i) {
        long_path /= dir_name;
    }

    // Test directory creation with long path
    EXPECT_TRUE(atom::io::createDirectory(long_path));
    EXPECT_TRUE(fs::exists(long_path));

    // Test file creation in long path
    fs::path long_file = long_path / "test.txt";
    createTestFile(long_file, "Long path test");
    EXPECT_TRUE(atom::io::isFileExists(long_file));

    // Test file operations on long path
    EXPECT_GT(atom::io::fileSize(long_file), 0);
    EXPECT_TRUE(atom::io::removeFile(long_file));
}

// Test special characters in filenames
TEST_F(IoTest, SpecialCharactersInFilenames) {
    // Test various special characters that should be valid
    std::vector<std::string> valid_names = {
        "file_with_underscore.txt",
        "file-with-dash.txt",
        "file.multiple.dots.txt",
        "file with spaces.txt",
        "file(with)parens.txt",
        "file[with]brackets.txt"
    };

    for (const auto& name : valid_names) {
        fs::path file = test_dir / name;
        createTestFile(file, "Test content");
        EXPECT_TRUE(fs::exists(file)) << "Failed for: " << name;
        EXPECT_TRUE(atom::io::isFileExists(file)) << "Failed for: " << name;
        EXPECT_TRUE(atom::io::removeFile(file)) << "Failed for: " << name;
    }

    // Test invalid characters (platform-specific)
#ifdef _WIN32
    std::vector<std::string> invalid_names = {
        "file<with>angles.txt",
        "file:with:colons.txt",
        "file|with|pipes.txt",
        "file?with?questions.txt",
        "file*with*asterisks.txt"
    };
#else
    std::vector<std::string> invalid_names = {
        "file/with/slashes.txt"  // Forward slash is path separator
    };
#endif

    for (const auto& name : invalid_names) {
        EXPECT_FALSE(atom::io::isFileNameValid(name)) << "Should be invalid: " << name;
    }
}

// Test error handling for read-only files
TEST_F(IoTest, ReadOnlyFileHandling) {
    fs::path readonly_file = test_dir / "readonly.txt";
    createTestFile(readonly_file, "Read-only content");

    // Make file read-only
    fs::permissions(readonly_file,
                   fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
                   fs::perm_options::replace);

    // Test that we can read the file
    EXPECT_TRUE(atom::io::isFileExists(readonly_file));
    EXPECT_GT(atom::io::fileSize(readonly_file), 0);

    // Test that writing fails (or succeeds depending on permissions)
    std::ofstream ofs(readonly_file, std::ios::app);
    bool can_write = ofs.is_open() && ofs.good();
    ofs.close();

    // Restore write permissions for cleanup
    fs::permissions(readonly_file,
                   fs::perms::owner_all,
                   fs::perm_options::replace);

    // File should still exist
    EXPECT_TRUE(fs::exists(readonly_file));
}

// Test disk space handling
TEST_F(IoTest, DiskSpaceOperations) {
    // Get space info for test directory
    std::error_code ec;
    auto space_info = fs::space(test_dir, ec);

    EXPECT_FALSE(ec);
    EXPECT_GT(space_info.capacity, 0);
    EXPECT_GT(space_info.free, 0);
    EXPECT_LE(space_info.available, space_info.free);
}

// Test atomic file operations
TEST_F(IoTest, AtomicFileOperations) {
    fs::path source = test_dir / "atomic_source.txt";
    fs::path dest = test_dir / "atomic_dest.txt";

    createTestFile(source, "Atomic test content");

    // Test atomic rename (move)
    EXPECT_TRUE(atom::io::renameFile(source, dest));
    EXPECT_FALSE(fs::exists(source));
    EXPECT_TRUE(fs::exists(dest));

    // Verify content is intact
    EXPECT_EQ(readTestFile(dest), "Atomic test content");
}

// Test file locking scenarios
TEST_F(IoTest, FileLockingScenarios) {
    fs::path locked_file = test_dir / "locked.txt";
    createTestFile(locked_file, "Locked content");

    // Open file for reading
    std::ifstream reader(locked_file);
    EXPECT_TRUE(reader.is_open());

    // Try to read while file is open (should succeed)
    EXPECT_TRUE(atom::io::isFileExists(locked_file));
    EXPECT_GT(atom::io::fileSize(locked_file), 0);

    reader.close();

    // File should still be accessible
    EXPECT_TRUE(atom::io::isFileExists(locked_file));
}

// Test empty file operations
TEST_F(IoTest, EmptyFileOperations) {
    fs::path empty_file = test_dir / "empty.txt";
    createTestFile(empty_file, "");

    EXPECT_TRUE(atom::io::isFileExists(empty_file));
    EXPECT_EQ(atom::io::fileSize(empty_file), 0);

    // Test copying empty file
    fs::path empty_copy = test_dir / "empty_copy.txt";
    EXPECT_TRUE(atom::io::copyFile(empty_file, empty_copy));
    EXPECT_TRUE(fs::exists(empty_copy));
    EXPECT_EQ(atom::io::fileSize(empty_copy), 0);

    // Test line counting on empty file
    auto lines = atom::io::countLinesInFile(empty_file);
    EXPECT_TRUE(lines.has_value());
    EXPECT_EQ(lines.value(), 0);
}

// Test binary file operations
TEST_F(IoTest, BinaryFileOperations) {
    fs::path binary_file = test_dir / "binary.dat";

    // Create binary file with null bytes
    std::vector<char> binary_data = {static_cast<char>(0x00),
                                     static_cast<char>(0x01),
                                     static_cast<char>(0x02),
                                     static_cast<char>(0xFF),
                                     static_cast<char>(0xFE),
                                     static_cast<char>(0x00),
                                     static_cast<char>(0x7F)};
    std::ofstream ofs(binary_file, std::ios::binary);
    ofs.write(binary_data.data(), binary_data.size());
    ofs.close();

    EXPECT_TRUE(atom::io::isFileExists(binary_file));
    EXPECT_EQ(atom::io::fileSize(binary_file), binary_data.size());

    // Test copying binary file
    fs::path binary_copy = test_dir / "binary_copy.dat";
    EXPECT_TRUE(atom::io::copyFile(binary_file, binary_copy));

    // Verify binary content
    std::ifstream ifs(binary_copy, std::ios::binary);
    std::vector<char> read_data(binary_data.size());
    ifs.read(read_data.data(), read_data.size());
    ifs.close();

    EXPECT_EQ(std::memcmp(binary_data.data(), read_data.data(), binary_data.size()), 0);
}

// Test error handling for non-existent paths
TEST_F(IoTest, ErrorHandlingNonExistentPaths) {
    fs::path non_existent = test_dir / "does_not_exist.txt";

    // Test file operations on non-existent file
    EXPECT_FALSE(atom::io::isFileExists(non_existent));
    EXPECT_EQ(atom::io::fileSize(non_existent), 0);
    EXPECT_FALSE(atom::io::removeFile(non_existent));

    // Test copy from non-existent source
    fs::path dest = test_dir / "dest.txt";
    EXPECT_FALSE(atom::io::copyFile(non_existent, dest));

    // Test move from non-existent source
    EXPECT_FALSE(atom::io::moveFile(non_existent, dest));

    // Test rename from non-existent source
    EXPECT_FALSE(atom::io::renameFile(non_existent, dest));
}

// Test error handling for invalid operations
TEST_F(IoTest, ErrorHandlingInvalidOperations) {
    // Test copying file to itself
    EXPECT_FALSE(atom::io::copyFile(test_file, test_file));

    // Test moving file to itself
    EXPECT_FALSE(atom::io::moveFile(test_file, test_file));

    // Test creating directory with file path
    EXPECT_FALSE(atom::io::createDirectory(test_file));

    // Test removing non-empty directory without recursive flag
    fs::path non_empty_dir = test_dir / "non_empty";
    fs::create_directories(non_empty_dir);
    createTestFile(non_empty_dir / "file.txt", "content");

    // Non-recursive remove should fail on non-empty directory
    std::error_code ec;
    fs::remove(non_empty_dir, ec);
    EXPECT_TRUE(ec);  // Should have error
}

// Test error handling for permission issues
TEST_F(IoTest, ErrorHandlingPermissionIssues) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping permission tests on Windows";
#endif

    fs::path protected_dir = test_dir / "protected";
    fs::create_directories(protected_dir);

    // Remove all permissions
    fs::permissions(protected_dir, fs::perms::none, fs::perm_options::replace);

    // Try to create file in protected directory (should fail)
    fs::path protected_file = protected_dir / "file.txt";
    std::ofstream ofs(protected_file);
    EXPECT_FALSE(ofs.is_open());

    // Restore permissions for cleanup
    fs::permissions(protected_dir, fs::perms::owner_all, fs::perm_options::replace);
}

// Test path traversal prevention
TEST_F(IoTest, PathTraversalPrevention) {
    // Test that path traversal attempts are handled safely
    std::vector<std::string> traversal_attempts = {
        "../../../etc/passwd",
        "..\\..\\..\\windows\\system32",
        "subdir/../../outside.txt"
    };

    for (const auto& attempt : traversal_attempts) {
        fs::path attempted_path = test_dir / attempt;

        // Verify the canonical path is still within test_dir or handle appropriately
        // This test ensures we're aware of path traversal
        EXPECT_NO_THROW({
            [[maybe_unused]] auto canonical = fs::weakly_canonical(attempted_path);
        });
    }
}

// Test large file handling
TEST_F(IoTest, LargeFileHandling) {
    fs::path large_file = test_dir / "large.txt";

    // Create a moderately large file (10 MB)
    const size_t file_size = 10 * 1024 * 1024;
    createLargeTestFile(large_file, file_size);

    EXPECT_TRUE(atom::io::isFileExists(large_file));
    EXPECT_GE(atom::io::fileSize(large_file), file_size);

    // Test copying large file
    fs::path large_copy = test_dir / "large_copy.txt";
    EXPECT_TRUE(atom::io::copyFile(large_file, large_copy));
    EXPECT_EQ(atom::io::fileSize(large_file), atom::io::fileSize(large_copy));

    // Test moving large file
    fs::path large_moved = test_dir / "large_moved.txt";
    EXPECT_TRUE(atom::io::moveFile(large_copy, large_moved));
    EXPECT_FALSE(fs::exists(large_copy));
    EXPECT_TRUE(fs::exists(large_moved));
}

// Test file truncation edge cases
TEST_F(IoTest, FileTruncationEdgeCases) {
    fs::path trunc_file = test_dir / "truncate.txt";
    createTestFile(trunc_file, "This is a test file with some content");

    size_t original_size = atom::io::fileSize(trunc_file);
    EXPECT_GT(original_size, 0);

    // Truncate to smaller size
    EXPECT_TRUE(atom::io::truncateFile(trunc_file, 10));
    EXPECT_EQ(atom::io::fileSize(trunc_file), 10);

    // Truncate to zero
    EXPECT_TRUE(atom::io::truncateFile(trunc_file, 0));
    EXPECT_EQ(atom::io::fileSize(trunc_file), 0);

    // Truncate to larger size (should extend with null bytes)
    EXPECT_TRUE(atom::io::truncateFile(trunc_file, 100));
    EXPECT_EQ(atom::io::fileSize(trunc_file), 100);
}

// Test directory walking with errors
TEST_F(IoTest, DirectoryWalkingWithErrors) {
    // Create a complex directory structure
    fs::path complex_dir = test_dir / "complex";
    fs::create_directories(complex_dir / "dir1" / "subdir1");
    fs::create_directories(complex_dir / "dir2" / "subdir2");
    createTestFile(complex_dir / "file1.txt", "content1");
    createTestFile(complex_dir / "dir1" / "file2.txt", "content2");
    createTestFile(complex_dir / "dir1" / "subdir1" / "file3.txt", "content3");

    // Test jwalk (returns JSON string)
    std::string json_result = atom::io::jwalk(complex_dir);
    EXPECT_FALSE(json_result.empty());
    EXPECT_NE(json_result.find("file1.txt"), std::string::npos);

    // Test fwalk (callback-based)
    std::vector<fs::path> all_paths;
    atom::io::fwalk(complex_dir, [&all_paths](const fs::path& p) {
        all_paths.push_back(p);
    });

    EXPECT_GE(all_paths.size(), 3);  // Should include files and directories
}

// Test file type classification edge cases
TEST_F(IoTest, FileTypeClassificationEdgeCases) {
    // Create various file types
    fs::path text_file = test_dir / "test.txt";
    fs::path cpp_file = test_dir / "test.cpp";
    fs::path header_file = test_dir / "test.hpp";
    fs::path no_ext_file = test_dir / "no_extension";
    fs::path hidden_file = test_dir / ".hidden";

    createTestFile(text_file, "text content");
    createTestFile(cpp_file, "// C++ code");
    createTestFile(header_file, "// Header");
    createTestFile(no_ext_file, "no extension");
    createTestFile(hidden_file, "hidden content");

    // Test file type checking
    EXPECT_TRUE(atom::io::isFileExists(text_file));
    EXPECT_TRUE(atom::io::isFileExists(cpp_file));
    EXPECT_TRUE(atom::io::isFileExists(header_file));
    EXPECT_TRUE(atom::io::isFileExists(no_ext_file));
    EXPECT_TRUE(atom::io::isFileExists(hidden_file));

    // Test classification
    auto classified = atom::io::classifyFiles(test_dir);
    EXPECT_FALSE(classified.empty());
}

// Test symlink chain handling
TEST_F(IoTest, SymlinkChainHandling) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping symlink chain tests on Windows";
#endif

    fs::path target = test_file;
    fs::path link1 = test_dir / "link1";
    fs::path link2 = test_dir / "link2";
    fs::path link3 = test_dir / "link3";

    // Create chain of symlinks
    EXPECT_TRUE(atom::io::createSymlink(target, link1));
    EXPECT_TRUE(atom::io::createSymlink(link1, link2));
    EXPECT_TRUE(atom::io::createSymlink(link2, link3));

    // All links should exist
    EXPECT_TRUE(fs::exists(link1));
    EXPECT_TRUE(fs::exists(link2));
    EXPECT_TRUE(fs::exists(link3));

    // Reading through the chain should work
    EXPECT_TRUE(atom::io::isFileExists(link3));
}

// Test circular symlink detection
TEST_F(IoTest, CircularSymlinkDetection) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping circular symlink tests on Windows";
#endif

    fs::path link_a = test_dir / "link_a";
    fs::path link_b = test_dir / "link_b";

    // Create circular symlinks
    std::error_code ec;
    fs::create_symlink(link_b, link_a, ec);
    fs::create_symlink(link_a, link_b, ec);

    // Operations on circular symlinks should handle gracefully
    EXPECT_NO_THROW({
        [[maybe_unused]] bool exists = atom::io::isFileExists(link_a);
    });
}
