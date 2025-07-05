#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "atom/io/glob.hpp"

// 不使用 using namespace atom::io，而是明确指定要使用的函数
namespace fs = std::filesystem;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

class GlobTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory structure for testing
        testDir = fs::temp_directory_path() / "glob_test";
        fs::create_directories(testDir);

        // Create test files and directories
        createTestFile(testDir / "file1.txt");
        createTestFile(testDir / "file2.txt");
        createTestFile(testDir / "file.md");
        createTestFile(testDir / "file.cpp");
        createTestFile(testDir / ".hidden.txt");

        fs::create_directories(testDir / "dir1");
        createTestFile(testDir / "dir1" / "nested1.txt");
        createTestFile(testDir / "dir1" / "nested2.txt");
        createTestFile(testDir / "dir1" / ".hidden_nested.txt");

        fs::create_directories(testDir / "dir2");
        createTestFile(testDir / "dir2" / "foo.txt");
        createTestFile(testDir / "dir2" / "bar.cpp");

        fs::create_directories(testDir / ".hidden_dir");
        createTestFile(testDir / ".hidden_dir" / "hidden_file.txt");

        // Save current working directory
        originalPath = fs::current_path();
        // Change to the test directory for testing
        fs::current_path(testDir);
    }

    void TearDown() override {
        // Restore original working directory
        fs::current_path(originalPath);

        // Clean up the test directory
        try {
            fs::remove_all(testDir);
        } catch (const std::exception&) {
            // Ignore cleanup errors
        }
    }

    void createTestFile(const fs::path& path) {
        std::ofstream file(path);
        file << "Test content for " << path.filename().string() << std::endl;
        file.close();
    }

    fs::path testDir;
    fs::path originalPath;
};

// Test basic glob with no wildcards
TEST_F(GlobTest, BasicGlobNoWildcards) {
    // 使用atom::io::名称空间前缀明确调用静态glob函数
    auto results = atom::io::glob("file1.txt");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].filename().string(), "file1.txt");

    // Non-existent file
    results = atom::io::glob("nonexistent.txt");
    EXPECT_THAT(results, IsEmpty());

    // Exact directory match
    results = atom::io::glob("dir1");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].filename().string(), "dir1");
}

// Test glob with * wildcard
TEST_F(GlobTest, GlobWithAsterisk) {
    // Match all .txt files in current directory
    auto results = atom::io::glob("*.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt
    EXPECT_THAT(results, Contains(fs::path("file1.txt")));
    EXPECT_THAT(results, Contains(fs::path("file2.txt")));
    EXPECT_THAT(results,
                Not(Contains(fs::path(
                    ".hidden.txt"))));  // Hidden files should not be matched

    // Match all files with any extension
    results = atom::io::glob("file*");
    EXPECT_EQ(results.size(), 4);  // file1.txt, file2.txt, file.md, file.cpp

    // Match files with specific pattern
    results = atom::io::glob("file?.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Match all files in subdirectory
    results = atom::io::glob("dir1/*");
    EXPECT_EQ(results.size(), 2);  // nested1.txt and nested2.txt
    EXPECT_THAT(results,
                Not(Contains(fs::path(
                    "dir1/.hidden_nested.txt"))));  // Hidden files not matched
}

// Test glob with ? wildcard
TEST_F(GlobTest, GlobWithQuestionMark) {
    // Match single character
    auto results = atom::io::glob("file?.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Multiple question marks
    results = atom::io::glob("nested?.txt");
    EXPECT_THAT(results, IsEmpty());  // not in current directory

    results = atom::io::glob("dir1/nested?.txt");
    EXPECT_EQ(results.size(), 2);  // nested1.txt and nested2.txt
}

// Test glob with character classes
TEST_F(GlobTest, GlobWithCharacterClasses) {
    // Match character range
    auto results = atom::io::glob("file[1-2].txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Match specific characters
    results = atom::io::glob("file[12].txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Negated character class
    results = atom::io::glob("file[!2].txt");
    EXPECT_EQ(results.size(), 1);  // file1.txt

    // Character class with special characters
    results = atom::io::glob("file.[cm]*");
    EXPECT_EQ(results.size(), 2);  // file.md and file.cpp
}

// Test recursive glob
TEST_F(GlobTest, RecursiveGlob) {
    // Recursive glob for all .txt files
    auto results = atom::io::rglob("**/*.txt");

    // Should find all .txt files in all directories (except hidden ones)
    EXPECT_GE(results.size(),
              5);  // file1.txt, file2.txt, nested1.txt, nested2.txt, foo.txt
    EXPECT_THAT(results, Contains(fs::path("file1.txt")));
    EXPECT_THAT(results, Contains(fs::path("file2.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir1/nested1.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir1/nested2.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir2/foo.txt")));

    // Recursive glob in specific directory
    results = atom::io::rglob("dir1/**/*.txt");
    EXPECT_EQ(results.size(), 2);  // nested1.txt and nested2.txt
}

// Test directory-only globbing
TEST_F(GlobTest, DirectoryOnlyGlob) {
    // 直接调用非静态的glob函数以指定dironly参数
    auto results = atom::io::glob("*", false, true);
    EXPECT_EQ(results.size(), 2);  // dir1 and dir2
    EXPECT_THAT(results, Contains(fs::path("dir1")));
    EXPECT_THAT(results, Contains(fs::path("dir2")));
    EXPECT_THAT(
        results,
        Not(Contains(fs::path("file1.txt"))));  // Files should not be matched

    // Test recursive directory-only glob
    results = atom::io::glob("**", true, true);
    EXPECT_GE(results.size(), 2);  // dir1 and dir2
    EXPECT_THAT(results, Contains(fs::path("dir1")));
    EXPECT_THAT(results, Contains(fs::path("dir2")));
}

// Test tilde expansion
TEST_F(GlobTest, TildeExpansion) {
    // Note: This test might be challenging in CI environments with different
    // user setups We'll just verify that the function doesn't throw an
    // exception

    // Temporarily change back to original directory
    fs::current_path(originalPath);

    // Create a mock version of the expandTilde function for testing
    auto testExpandTilde = [](const fs::path& path) {
        return atom::io::expandTilde(path);
    };

    // This might not expand to a valid path in all environments
    EXPECT_NO_THROW(testExpandTilde(fs::path("~")));
    EXPECT_NO_THROW(testExpandTilde(fs::path("~/some_path")));

    // Return to test directory
    fs::current_path(testDir);
}

// Test glob with multiple patterns via vector
TEST_F(GlobTest, GlobWithVectorPatterns) {
    std::vector<std::string> patterns = {"*.txt", "*.cpp"};
    auto results = atom::io::glob(patterns);

    EXPECT_EQ(results.size(), 3);  // file1.txt, file2.txt, file.cpp
    EXPECT_THAT(results, Contains(fs::path("file1.txt")));
    EXPECT_THAT(results, Contains(fs::path("file2.txt")));
    EXPECT_THAT(results, Contains(fs::path("file.cpp")));
    EXPECT_THAT(results, Not(Contains(fs::path("file.md"))));

    // Test recursive glob with vector patterns
    patterns = {"dir1/*.txt", "dir2/*.cpp"};
    results = atom::io::rglob(patterns);

    EXPECT_GE(results.size(), 3);  // nested1.txt, nested2.txt, bar.cpp
    EXPECT_THAT(results, Contains(fs::path("dir1/nested1.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir1/nested2.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir2/bar.cpp")));
}

// Test glob with initializer list
TEST_F(GlobTest, GlobWithInitializerList) {
    auto results = atom::io::glob({"*.txt", "*.cpp"});

    EXPECT_EQ(results.size(), 3);  // file1.txt, file2.txt, file.cpp
    EXPECT_THAT(results, Contains(fs::path("file1.txt")));
    EXPECT_THAT(results, Contains(fs::path("file2.txt")));
    EXPECT_THAT(results, Contains(fs::path("file.cpp")));

    // Test recursive glob with initializer list
    results = atom::io::rglob({"dir1/*.txt", "dir2/*.cpp"});

    EXPECT_GE(results.size(), 3);  // nested1.txt, nested2.txt, bar.cpp
    EXPECT_THAT(results, Contains(fs::path("dir1/nested1.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir1/nested2.txt")));
    EXPECT_THAT(results, Contains(fs::path("dir2/bar.cpp")));
}

// Test edge cases and corner conditions
TEST_F(GlobTest, EdgeCases) {
    // Empty pattern
    auto results = atom::io::glob("");
    EXPECT_THAT(results, IsEmpty());

    // Current directory
    results = atom::io::glob(".");
    EXPECT_EQ(results.size(), 1);

    // Parent directory
    results = atom::io::glob("..");
    EXPECT_EQ(results.size(), 1);

    // Pattern with just wildcards
    results = atom::io::glob("*");
    EXPECT_GT(results.size(), 0);

    // Multiple wildcards
    results = atom::io::glob("*.*");
    EXPECT_GT(results.size(), 0);

    // Complex pattern
    results = atom::io::glob("*.[ct]*");
    EXPECT_GT(results.size(), 0);  // Should match .txt and .cpp files

    // Non-existent directory
    results = atom::io::glob("nonexistent_dir/*");
    EXPECT_THAT(results, IsEmpty());
}

// Test utility functions
TEST_F(GlobTest, UtilityFunctions) {
    // Test hasMagic
    EXPECT_TRUE(atom::io::hasMagic("*.txt"));
    EXPECT_TRUE(atom::io::hasMagic("file?.txt"));
    EXPECT_TRUE(atom::io::hasMagic("file[1-2].txt"));
    EXPECT_FALSE(atom::io::hasMagic("file.txt"));

    // Test isHidden
    EXPECT_TRUE(atom::io::isHidden(".hidden.txt"));
    EXPECT_TRUE(atom::io::isHidden("dir/.hidden.txt"));
    EXPECT_FALSE(atom::io::isHidden("file.txt"));
    EXPECT_FALSE(atom::io::isHidden("dir/file.txt"));

    // Test isRecursive
    EXPECT_TRUE(atom::io::isRecursive("**"));
    EXPECT_FALSE(atom::io::isRecursive("*"));
    EXPECT_FALSE(atom::io::isRecursive("file.txt"));

    // Test translate (pattern to regex conversion)
    std::string regex = atom::io::translate("*.txt");
    EXPECT_NE(regex.find(".*\\.txt"), std::string::npos);

    // Test fnmatch
    EXPECT_TRUE(atom::io::fnmatch(fs::path("file.txt"), "*.txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file.txt"), "*.md"));
}

// Test directory iteration
TEST_F(GlobTest, DirectoryIteration) {
    // Test iterDirectory
    auto results = atom::io::iterDirectory(fs::path("."), false);
    EXPECT_GT(results.size(), 0);

    // Test rlistdir (recursive directory listing)
    results = atom::io::rlistdir(fs::path("."), false);
    EXPECT_GT(results.size(), 0);

    // Test glob0, glob1, glob2 (internal functions)
    results = atom::io::glob0(fs::path("."), fs::path("file1.txt"), false);
    EXPECT_EQ(results.size(), 1);

    results = atom::io::glob1(fs::path("."), "*.txt", false);
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    results = atom::io::glob2(fs::path("."), "**", false);
    EXPECT_GT(results.size(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
