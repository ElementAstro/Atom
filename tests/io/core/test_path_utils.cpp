/*
 * test_path_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_path_utils.cpp
 * @brief Unit tests for atom::io::path_utils
 *
 * Tests path validation, file/folder name validation, and permission checking.
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "atom/io/core/path_utils.hpp"

namespace fs = std::filesystem;
using namespace atom::io::detail;

namespace atom::io::test {

// ============================================================================
// Path Validation Tests
// ============================================================================

class PathUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for tests
        tempDir_ = fs::temp_directory_path() / "path_utils_test";
        fs::create_directories(tempDir_);
    }

    void TearDown() override {
        // Cleanup temp directory
        std::error_code ec;
        fs::remove_all(tempDir_, ec);
    }

    fs::path tempDir_;
};

// Test basic path validation
TEST_F(PathUtilsTest, ValidatePathBasic) {
    // Valid paths
    EXPECT_TRUE(validatePath("documents/file.txt"));
    EXPECT_TRUE(validatePath("/home/user/file.txt"));
    EXPECT_TRUE(validatePath("."));
    EXPECT_TRUE(validatePath(".."));
    EXPECT_TRUE(validatePath("file.txt"));

    // Invalid paths
    EXPECT_FALSE(validatePath(""));  // Empty path
}

// Test path with null byte (security vulnerability)
TEST_F(PathUtilsTest, ValidatePathNullByte) {
    std::string pathWithNull = "file";
    pathWithNull += '\0';
    pathWithNull += "hidden.txt";
    EXPECT_FALSE(validatePath(pathWithNull));
}

// Test path length limits
TEST_F(PathUtilsTest, ValidatePathLength) {
    // Very long path should be rejected
    std::string longPath(5000, 'a');
    EXPECT_FALSE(validatePath(longPath));

    // Normal length path should be accepted
    std::string normalPath(100, 'a');
    EXPECT_TRUE(validatePath(normalPath));
}

#ifdef _WIN32
// Windows-specific path validation tests
TEST_F(PathUtilsTest, ValidatePathWindowsInvalidChars) {
    EXPECT_FALSE(validatePath("file<name>.txt"));
    EXPECT_FALSE(validatePath("file>name.txt"));
    EXPECT_FALSE(validatePath("file:name.txt"));
    EXPECT_FALSE(validatePath("file\"name.txt"));
    EXPECT_FALSE(validatePath("file|name.txt"));
    EXPECT_FALSE(validatePath("file?name.txt"));
    EXPECT_FALSE(validatePath("file*name.txt"));
}

TEST_F(PathUtilsTest, ValidatePathWindowsReservedNames) {
    EXPECT_FALSE(validatePath("CON"));
    EXPECT_FALSE(validatePath("PRN"));
    EXPECT_FALSE(validatePath("AUX"));
    EXPECT_FALSE(validatePath("NUL"));
    EXPECT_FALSE(validatePath("COM1"));
    EXPECT_FALSE(validatePath("LPT1"));
    EXPECT_FALSE(validatePath("NUL.txt"));
    EXPECT_FALSE(validatePath("CON.log"));
}
#endif

// ============================================================================
// Folder Name Validation Tests
// ============================================================================

TEST_F(PathUtilsTest, IsFolderNameValidBasic) {
    // Valid folder names
    EXPECT_TRUE(isFolderNameValid("my_folder"));
    EXPECT_TRUE(isFolderNameValid("MyFolder"));
    EXPECT_TRUE(isFolderNameValid("folder123"));
    EXPECT_TRUE(isFolderNameValid("folder.backup"));
    EXPECT_TRUE(isFolderNameValid("my-folder"));
    EXPECT_TRUE(isFolderNameValid("folder with spaces"));

    // Invalid folder names
    EXPECT_FALSE(isFolderNameValid(""));  // Empty name
}

#ifdef _WIN32
TEST_F(PathUtilsTest, IsFolderNameValidWindows) {
    EXPECT_FALSE(isFolderNameValid("folder:name"));
    EXPECT_FALSE(isFolderNameValid("folder*name"));
    EXPECT_FALSE(isFolderNameValid("folder?name"));
}
#else
TEST_F(PathUtilsTest, IsFolderNameValidUnix) {
    EXPECT_FALSE(isFolderNameValid("folder/name"));
}
#endif

// ============================================================================
// File Name Validation Tests
// ============================================================================

TEST_F(PathUtilsTest, IsFileNameValidBasic) {
    // Valid file names
    EXPECT_TRUE(isFileNameValid("document.txt"));
    EXPECT_TRUE(isFileNameValid("My Document.pdf"));
    EXPECT_TRUE(isFileNameValid("file.backup.txt"));
    EXPECT_TRUE(isFileNameValid("my_file.cpp"));
    EXPECT_TRUE(isFileNameValid("file123.log"));

    // Invalid file names
    EXPECT_FALSE(isFileNameValid(""));  // Empty name
}

#ifdef _WIN32
TEST_F(PathUtilsTest, IsFileNameValidWindows) {
    EXPECT_FALSE(isFileNameValid("file<1>.txt"));
    EXPECT_FALSE(isFileNameValid("file>1.txt"));
    EXPECT_FALSE(isFileNameValid("file?.txt"));
    EXPECT_FALSE(isFileNameValid("file*.txt"));
    EXPECT_FALSE(isFileNameValid("file|name.txt"));
}
#else
TEST_F(PathUtilsTest, IsFileNameValidUnix) {
    EXPECT_FALSE(isFileNameValid("file/name.txt"));
}
#endif

// ============================================================================
// Permission Validation Tests
// ============================================================================

TEST_F(PathUtilsTest, ValidatePermissionsReadable) {
    // Create a test file
    fs::path testFile = tempDir_ / "readable_file.txt";
    {
        std::ofstream ofs(testFile);
        ofs << "Test content";
    }

    // Should be readable
    EXPECT_TRUE(validatePermissions(testFile.string(), false));

    // Cleanup
    fs::remove(testFile);
}

TEST_F(PathUtilsTest, ValidatePermissionsWritable) {
    // Create a test file
    fs::path testFile = tempDir_ / "writable_file.txt";
    {
        std::ofstream ofs(testFile);
        ofs << "Test content";
    }

    // Should be writable (we just created it)
    EXPECT_TRUE(validatePermissions(testFile.string(), true));

    // Cleanup
    fs::remove(testFile);
}

TEST_F(PathUtilsTest, ValidatePermissionsNonExistent) {
    // Non-existent file should fail
    EXPECT_FALSE(validatePermissions("/nonexistent/file.txt", false));
    EXPECT_FALSE(validatePermissions("/nonexistent/file.txt", true));
}

// ============================================================================
// isValidPath Tests (for directory stack operations)
// ============================================================================

TEST_F(PathUtilsTest, IsValidPathExisting) {
    // Existing directories should be valid
    EXPECT_TRUE(isValidPath(fs::path(".")));
    EXPECT_TRUE(isValidPath(fs::temp_directory_path()));
    EXPECT_TRUE(isValidPath(tempDir_));
}

TEST_F(PathUtilsTest, IsValidPathNonExisting) {
    // Non-existing paths should be invalid
    EXPECT_FALSE(isValidPath(fs::path("/definitely/not/exists/anywhere")));
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(PathUtilsTest, PathWithSpecialCharacters) {
    // Paths with special but valid characters
    EXPECT_TRUE(validatePath("file-name.txt"));
    EXPECT_TRUE(validatePath("file_name.txt"));
    EXPECT_TRUE(validatePath("file.name.txt"));
    EXPECT_TRUE(validatePath("file name.txt"));
}

TEST_F(PathUtilsTest, PathWithUnicode) {
    // Unicode paths (if supported by filesystem)
    // Note: This may fail on some systems
    try {
        EXPECT_TRUE(validatePath("文件.txt"));
        EXPECT_TRUE(validatePath("файл.txt"));
    } catch (...) {
        // Unicode not supported, skip
    }
}

TEST_F(PathUtilsTest, RelativePathTraversal) {
    // Relative path traversal patterns
    // These should be validated but may be flagged as suspicious
    EXPECT_TRUE(validatePath("../parent/file.txt"));
    EXPECT_TRUE(validatePath("./current/file.txt"));
    EXPECT_TRUE(validatePath("dir/../file.txt"));
}

}  // namespace atom::io::test
