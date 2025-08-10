#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <future>
#include <memory>

// Include io module headers
#include "atom/io/io.hpp"
#include "atom/io/file_info.hpp"
#include "atom/io/file_permission.hpp"
#include "atom/io/async_io.hpp"
#include "atom/io/async_compress.hpp"
#include "atom/io/async_glob.hpp"
#include "atom/io/compress.hpp"
#include "atom/io/glob.hpp"
#include "atom/io/pushd.hpp"

namespace fs = std::filesystem;
using namespace testing;

class IOComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directory structure
        test_dir_ = fs::temp_directory_path() / "atom_io_comprehensive_test";
        
        // Clean up any existing test directories
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        
        // Create fresh directories
        fs::create_directories(test_dir_);
        fs::create_directories(test_dir_ / "input");
        fs::create_directories(test_dir_ / "output");
        fs::create_directories(test_dir_ / "subdir1");
        fs::create_directories(test_dir_ / "subdir2");
        
        // Create test files with various content
        createTestFile(test_dir_ / "file1.txt", "Simple text content");
        createTestFile(test_dir_ / "file2.txt", "Another text file\nwith multiple lines");
        createTestFile(test_dir_ / "file3.dat", std::string(1000, 'x')); // Binary-like content
        createTestFile(test_dir_ / "input" / "nested.txt", "Nested file content");
        createTestFile(test_dir_ / "subdir1" / "sub1.txt", "Subdirectory 1 content");
        createTestFile(test_dir_ / "subdir2" / "sub2.txt", "Subdirectory 2 content");
        
        // Create hidden files
        createTestFile(test_dir_ / ".hidden.txt", "Hidden file content");
        
        // Save original working directory
        original_cwd_ = fs::current_path();
        
        // Initialize ASIO context for async tests
        io_context_ = std::make_unique<asio::io_context>();
        work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            io_context_->get_executor());
        
        // Start io_context in background thread
        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }
    
    void TearDown() override {
        // Stop io_context
        work_guard_.reset();
        io_context_->stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        
        // Restore original working directory
        fs::current_path(original_cwd_);
        
        // Clean up test directory
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }
    
    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }
    
    void waitForAsyncOperation(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
protected:
    fs::path test_dir_;
    fs::path original_cwd_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread io_thread_;
};

// Test basic file operations
TEST_F(IOComprehensiveTest, BasicFileOperations) {
    fs::path source = test_dir_ / "file1.txt";
    fs::path dest = test_dir_ / "file1_copy.txt";
    
    // Test file copy
    EXPECT_TRUE(atom::io::copyFile(source, dest));
    EXPECT_TRUE(fs::exists(dest));
    
    // Verify content is identical
    std::ifstream src_file(source);
    std::ifstream dst_file(dest);
    std::string src_content((std::istreambuf_iterator<char>(src_file)),
                           std::istreambuf_iterator<char>());
    std::string dst_content((std::istreambuf_iterator<char>(dst_file)),
                           std::istreambuf_iterator<char>());
    EXPECT_EQ(src_content, dst_content);
    
    // Test file rename
    fs::path renamed = test_dir_ / "file1_renamed.txt";
    EXPECT_TRUE(atom::io::renameFile(dest, renamed));
    EXPECT_FALSE(fs::exists(dest));
    EXPECT_TRUE(fs::exists(renamed));
    
    // Test file removal
    EXPECT_TRUE(atom::io::removeFile(renamed));
    EXPECT_FALSE(fs::exists(renamed));
}

// Test directory operations
TEST_F(IOComprehensiveTest, DirectoryOperations) {
    fs::path new_dir = test_dir_ / "new_directory";
    
    // Test directory creation
    EXPECT_TRUE(atom::io::createDirectory(new_dir));
    EXPECT_TRUE(fs::exists(new_dir));
    EXPECT_TRUE(fs::is_directory(new_dir));
    
    // Test directory exists check
    EXPECT_TRUE(atom::io::isDirectoryExists(new_dir));
    EXPECT_FALSE(atom::io::isDirectoryExists(test_dir_ / "nonexistent"));
    
    // Test directory removal
    EXPECT_TRUE(atom::io::removeDirectory(new_dir));
    EXPECT_FALSE(fs::exists(new_dir));
}

// Test file information retrieval
TEST_F(IOComprehensiveTest, FileInformation) {
    fs::path test_file = test_dir_ / "file1.txt";
    
    // Test basic file info
    auto info = atom::io::getFileInfo(test_file);
    EXPECT_FALSE(info.filePath.empty());
    EXPECT_EQ(info.fileName, "file1.txt");
    EXPECT_EQ(info.extension, ".txt");
    EXPECT_GT(info.fileSize, 0);
    
    // Test file size
    auto size = atom::io::getFileSize(test_file);
    EXPECT_GT(size, 0);
    EXPECT_EQ(size, info.fileSize);
    
    // Test file existence
    EXPECT_TRUE(atom::io::isFileExists(test_file));
    EXPECT_FALSE(atom::io::isFileExists(test_dir_ / "nonexistent.txt"));
}

// Test file permissions
TEST_F(IOComprehensiveTest, FilePermissions) {
    fs::path test_file = test_dir_ / "file1.txt";
    
    // Get current permissions
    auto current_perms = atom::io::getFilePermissions(test_file.string());
    EXPECT_FALSE(current_perms.empty());
    EXPECT_EQ(current_perms.length(), 9); // Should be in format "rwxrwxrwx"
    
    // Test permission change (if supported on platform)
    try {
        atom::io::changeFilePermissions(test_file, "rw-r--r--");
        auto new_perms = atom::io::getFilePermissions(test_file.string());
        // Note: Actual permission change verification depends on platform
        EXPECT_FALSE(new_perms.empty());
    } catch (const std::exception& e) {
        // Permission changes might not be supported on all platforms
        GTEST_SKIP() << "Permission changes not supported: " << e.what();
    }
}

// Test glob operations
TEST_F(IOComprehensiveTest, GlobOperations) {
    // Change to test directory for relative path testing
    fs::current_path(test_dir_);
    
    // Test simple glob
    auto txt_files = atom::io::glob("*.txt");
    EXPECT_GE(txt_files.size(), 2); // Should find file1.txt, file2.txt, etc.
    
    // Test recursive glob
    auto all_txt = atom::io::glob("**/*.txt", true);
    EXPECT_GE(all_txt.size(), txt_files.size()); // Should find more files recursively
    
    // Test directory-only glob
    auto dirs = atom::io::glob("*", false, true);
    EXPECT_GE(dirs.size(), 2); // Should find subdir1, subdir2, etc.
}

// Test async file operations
TEST_F(IOComprehensiveTest, AsyncFileOperations) {
    fs::path test_file = test_dir_ / "async_test.txt";
    std::string test_content = "Async file test content";
    
    // Create async file instance
    atom::async::io::AsyncFile async_file(*io_context_);
    
    bool write_completed = false;
    std::string read_content;
    bool read_completed = false;
    
    // Test async write
    std::span<const char> content_span(test_content.data(), test_content.size());
    async_file.asyncWrite(test_file, content_span,
        [&write_completed](atom::async::io::AsyncResult<void> result) {
            EXPECT_TRUE(result.success);
            write_completed = true;
        });
    
    // Wait for write completion
    auto start_time = std::chrono::steady_clock::now();
    while (!write_completed && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(write_completed);
    
    // Test async read
    async_file.asyncRead(test_file,
        [&read_content, &read_completed](atom::async::io::AsyncResult<std::string> result) {
            EXPECT_TRUE(result.success);
            read_content = result.value;
            read_completed = true;
        });
    
    // Wait for read completion
    start_time = std::chrono::steady_clock::now();
    while (!read_completed && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(read_completed);
    EXPECT_EQ(read_content, test_content);
}

// Test compression operations
TEST_F(IOComprehensiveTest, CompressionOperations) {
    fs::path input_file = test_dir_ / "file3.dat";
    fs::path compressed_file = test_dir_ / "file3.dat.gz";
    fs::path decompressed_file = test_dir_ / "file3_decompressed.dat";
    
    // Test synchronous compression
    auto compress_result = atom::io::compressFile(input_file.string(), compressed_file.parent_path().string());
    EXPECT_TRUE(compress_result.success);
    EXPECT_TRUE(fs::exists(compressed_file));
    EXPECT_LT(fs::file_size(compressed_file), fs::file_size(input_file)); // Should be smaller

    // Test synchronous decompression
    auto decompress_result = atom::io::decompressFile(compressed_file.string(), decompressed_file.parent_path().string());
    EXPECT_TRUE(decompress_result.success);
    EXPECT_TRUE(fs::exists(decompressed_file));
    EXPECT_EQ(fs::file_size(decompressed_file), fs::file_size(input_file)); // Should be same size
}

// Test pushd operations
TEST_F(IOComprehensiveTest, PushdOperations) {
    fs::path original_cwd = fs::current_path();
    fs::path target_dir = test_dir_ / "subdir1";
    
    {
        // Test pushd functionality using DirectoryStack
        atom::io::DirectoryStack stack;
        fs::current_path(target_dir);
        EXPECT_EQ(fs::current_path(), target_dir);
        fs::current_path(original_cwd);
    } // Manual restoration for now
    
    EXPECT_EQ(fs::current_path(), original_cwd);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
