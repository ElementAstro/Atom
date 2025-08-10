#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

// Test basic file operations without complex dependencies
namespace fs = std::filesystem;

class BasicFunctionalityTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "atom_basic_test";
        
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
        
        fs::create_directories(test_dir_);
        
        // Create test files
        createTestFile(test_dir_ / "test1.txt", "Test file 1 content");
        createTestFile(test_dir_ / "test2.txt", "Test file 2 content");
        createTestFile(test_dir_ / "data.csv", "name,age\nJohn,30\nJane,25");
        
        // Create subdirectory
        fs::create_directories(test_dir_ / "subdir");
        createTestFile(test_dir_ / "subdir" / "nested.txt", "Nested content");
    }
    
    void TearDown() override {
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
};

// Test basic file system operations
TEST_F(BasicFunctionalityTest, BasicFileOperations) {
    fs::path source = test_dir_ / "test1.txt";
    fs::path dest = test_dir_ / "test1_copy.txt";
    
    // Test file copy using standard library
    EXPECT_TRUE(fs::exists(source));
    
    std::error_code ec;
    fs::copy_file(source, dest, ec);
    EXPECT_FALSE(ec) << "Copy failed: " << ec.message();
    EXPECT_TRUE(fs::exists(dest));
    
    // Verify content is identical
    EXPECT_TRUE(filesAreIdentical(source, dest));
    
    // Test file rename
    fs::path renamed = test_dir_ / "test1_renamed.txt";
    fs::rename(dest, renamed, ec);
    EXPECT_FALSE(ec) << "Rename failed: " << ec.message();
    EXPECT_FALSE(fs::exists(dest));
    EXPECT_TRUE(fs::exists(renamed));
    
    // Test file removal
    fs::remove(renamed, ec);
    EXPECT_FALSE(ec) << "Remove failed: " << ec.message();
    EXPECT_FALSE(fs::exists(renamed));
}

// Test directory operations
TEST_F(BasicFunctionalityTest, DirectoryOperations) {
    fs::path new_dir = test_dir_ / "new_directory";
    
    // Test directory creation
    std::error_code ec;
    fs::create_directory(new_dir, ec);
    EXPECT_FALSE(ec) << "Directory creation failed: " << ec.message();
    EXPECT_TRUE(fs::exists(new_dir));
    EXPECT_TRUE(fs::is_directory(new_dir));
    
    // Test directory listing
    int file_count = 0;
    for (const auto& entry : fs::directory_iterator(test_dir_)) {
        if (entry.is_regular_file()) {
            file_count++;
        }
    }
    EXPECT_GE(file_count, 3); // Should have at least 3 files
    
    // Test recursive directory iteration
    int total_files = 0;
    for (const auto& entry : fs::recursive_directory_iterator(test_dir_)) {
        if (entry.is_regular_file()) {
            total_files++;
        }
    }
    EXPECT_GT(total_files, file_count); // Should find more files recursively
    
    // Test directory removal
    fs::remove(new_dir, ec);
    EXPECT_FALSE(ec) << "Directory removal failed: " << ec.message();
    EXPECT_FALSE(fs::exists(new_dir));
}

// Test file information retrieval
TEST_F(BasicFunctionalityTest, FileInformation) {
    fs::path test_file = test_dir_ / "test1.txt";
    
    EXPECT_TRUE(fs::exists(test_file));
    EXPECT_TRUE(fs::is_regular_file(test_file));
    EXPECT_FALSE(fs::is_directory(test_file));
    
    // Test file size
    std::error_code ec;
    auto size = fs::file_size(test_file, ec);
    EXPECT_FALSE(ec) << "File size query failed: " << ec.message();
    EXPECT_GT(size, 0);
    
    // Test file times
    auto write_time = fs::last_write_time(test_file, ec);
    EXPECT_FALSE(ec) << "Last write time query failed: " << ec.message();
    
    // Test file permissions
    auto perms = fs::status(test_file, ec).permissions();
    EXPECT_FALSE(ec) << "Permissions query failed: " << ec.message();
    
    // Test file extension
    EXPECT_EQ(test_file.extension(), ".txt");
    EXPECT_EQ(test_file.filename(), "test1.txt");
    EXPECT_EQ(test_file.stem(), "test1");
}

// Test path operations
TEST_F(BasicFunctionalityTest, PathOperations) {
    fs::path test_path = test_dir_ / "subdir" / "nested.txt";
    
    // Test path components
    EXPECT_EQ(test_path.filename(), "nested.txt");
    EXPECT_EQ(test_path.extension(), ".txt");
    EXPECT_EQ(test_path.stem(), "nested");
    EXPECT_EQ(test_path.parent_path().filename(), "subdir");
    
    // Test path manipulation
    fs::path new_path = test_path.parent_path() / "new_file.dat";
    EXPECT_EQ(new_path.extension(), ".dat");
    
    // Test relative path
    fs::path relative = fs::relative(test_path, test_dir_);
    EXPECT_EQ(relative, fs::path("subdir") / "nested.txt");
    
    // Test absolute path
    fs::path absolute = fs::absolute(relative);
    EXPECT_TRUE(absolute.is_absolute());
}

// Test error handling
TEST_F(BasicFunctionalityTest, ErrorHandling) {
    fs::path nonexistent = test_dir_ / "does_not_exist.txt";
    
    // Test operations on non-existent files
    EXPECT_FALSE(fs::exists(nonexistent));
    
    std::error_code ec;
    
    // These operations should fail gracefully
    fs::file_size(nonexistent, ec);
    EXPECT_TRUE(ec); // Should have an error
    
    fs::last_write_time(nonexistent, ec);
    EXPECT_TRUE(ec); // Should have an error
    
    fs::copy_file(nonexistent, test_dir_ / "copy.txt", ec);
    EXPECT_TRUE(ec); // Should have an error
    
    // Test invalid path operations
    fs::path invalid_dest = test_dir_ / "nonexistent_dir" / "file.txt";
    fs::copy_file(test_dir_ / "test1.txt", invalid_dest, ec);
    EXPECT_TRUE(ec); // Should fail due to missing parent directory
}

// Test file content operations
TEST_F(BasicFunctionalityTest, FileContentOperations) {
    fs::path test_file = test_dir_ / "content_test.txt";
    std::string test_content = "This is test content\nwith multiple lines\nfor testing purposes.";
    
    // Write content to file
    {
        std::ofstream file(test_file);
        EXPECT_TRUE(file.is_open());
        file << test_content;
    }
    
    EXPECT_TRUE(fs::exists(test_file));
    EXPECT_EQ(fs::file_size(test_file), test_content.size());
    
    // Read content from file
    {
        std::ifstream file(test_file);
        EXPECT_TRUE(file.is_open());
        
        std::string read_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        EXPECT_EQ(read_content, test_content);
    }
    
    // Test line-by-line reading
    {
        std::ifstream file(test_file);
        std::string line;
        int line_count = 0;
        
        while (std::getline(file, line)) {
            line_count++;
            EXPECT_FALSE(line.empty());
        }
        
        EXPECT_EQ(line_count, 3); // Should have 3 lines
    }
}

// Test performance with multiple files
TEST_F(BasicFunctionalityTest, MultipleFileOperations) {
    const int num_files = 100;
    std::vector<fs::path> created_files;
    
    // Create multiple files
    for (int i = 0; i < num_files; ++i) {
        fs::path file_path = test_dir_ / ("file_" + std::to_string(i) + ".txt");
        std::string content = "Content for file " + std::to_string(i);
        
        std::ofstream file(file_path);
        file << content;
        
        created_files.push_back(file_path);
    }
    
    // Verify all files exist
    for (const auto& file_path : created_files) {
        EXPECT_TRUE(fs::exists(file_path));
    }
    
    // Count files in directory
    int file_count = 0;
    for (const auto& entry : fs::directory_iterator(test_dir_)) {
        if (entry.is_regular_file()) {
            file_count++;
        }
    }
    
    EXPECT_GE(file_count, num_files + 3); // Original 3 files plus new ones
    
    // Copy all created files
    fs::path backup_dir = test_dir_ / "backup";
    fs::create_directory(backup_dir);
    
    for (const auto& file_path : created_files) {
        fs::path backup_path = backup_dir / file_path.filename();
        std::error_code ec;
        fs::copy_file(file_path, backup_path, ec);
        EXPECT_FALSE(ec) << "Failed to copy " << file_path;
        EXPECT_TRUE(fs::exists(backup_path));
    }
    
    // Remove all created files
    for (const auto& file_path : created_files) {
        std::error_code ec;
        fs::remove(file_path, ec);
        EXPECT_FALSE(ec) << "Failed to remove " << file_path;
        EXPECT_FALSE(fs::exists(file_path));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
