#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "atom/io/glob.hpp"

// 不使用 using namespace atom::io，而是明确指定要使用的函数
namespace fs = std::filesystem;
using atom::containers::String;
using atom::containers::Vector;

// Helper functions to avoid overload resolution issues
auto globHelper(const String& pattern, bool recursive = false,
                bool dironly = false) {
    return atom::io::glob(pattern, recursive, dironly);
}

auto globHelper(const fs::path& pattern, bool recursive = false,
                bool dironly = false) {
    return atom::io::glob(String{pattern.string().c_str()}, recursive, dironly);
}

inline auto glob_helper(const char* pattern, bool recursive = false,
                        bool dironly = false) {
    return globHelper(String(pattern), recursive, dironly);
}

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
    auto results = glob_helper("file1.txt");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].filename().string(), "file1.txt");

    // Non-existent file
    results = glob_helper("nonexistent.txt");
    EXPECT_THAT(results, IsEmpty());

    // Exact directory match
    results = glob_helper("dir1");
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].filename().string(), "dir1");
}

// Test glob with * wildcard
TEST_F(GlobTest, GlobWithAsterisk) {
    // Match all .txt files in current directory
    auto results = glob_helper("*.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt
    EXPECT_THAT(results, Contains(fs::path("file1.txt")));
    EXPECT_THAT(results, Contains(fs::path("file2.txt")));
    EXPECT_THAT(results,
                Not(Contains(fs::path(
                    ".hidden.txt"))));  // Hidden files should not be matched

    // Match all files with any extension
    results = glob_helper("file*");
    EXPECT_EQ(results.size(), 4);  // file1.txt, file2.txt, file.md, file.cpp

    // Match files with specific pattern
    results = glob_helper("file?.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Match all files in subdirectory
    results = glob_helper("dir1/*");
    EXPECT_EQ(results.size(), 2);  // nested1.txt and nested2.txt
    EXPECT_THAT(results,
                Not(Contains(fs::path(
                    "dir1/.hidden_nested.txt"))));  // Hidden files not matched
}

// Test glob with ? wildcard
TEST_F(GlobTest, GlobWithQuestionMark) {
    // Match single character
    auto results = glob_helper("file?.txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Multiple question marks
    results = glob_helper("nested?.txt");
    EXPECT_THAT(results, IsEmpty());  // not in current directory

    results = glob_helper("dir1/nested?.txt");
    EXPECT_EQ(results.size(), 2);  // nested1.txt and nested2.txt
}

// Test glob with character classes
TEST_F(GlobTest, GlobWithCharacterClasses) {
    // Match character range
    auto results = glob_helper("file[1-2].txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Match specific characters
    results = glob_helper("file[12].txt");
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Negated character class
    results = glob_helper("file[!2].txt");
    EXPECT_EQ(results.size(), 1);  // file1.txt

    // Character class with special characters
    results = glob_helper("file.[cm]*");
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
    auto results = globHelper(String("*"), false, true);
    EXPECT_EQ(results.size(), 2);  // dir1 and dir2
    EXPECT_THAT(results, Contains(fs::path("dir1")));
    EXPECT_THAT(results, Contains(fs::path("dir2")));
    EXPECT_THAT(
        results,
        Not(Contains(fs::path("file1.txt"))));  // Files should not be matched

    // Test recursive directory-only glob
    results = globHelper(String("**"), true, true);
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
    auto results = glob_helper("");
    EXPECT_THAT(results, IsEmpty());

    // Current directory
    results = glob_helper(".");
    EXPECT_EQ(results.size(), 1);

    // Parent directory
    results = glob_helper("..");
    EXPECT_EQ(results.size(), 1);

    // Pattern with just wildcards
    results = glob_helper("*");
    EXPECT_GT(results.size(), 0);

    // Multiple wildcards
    results = glob_helper("*.*");
    EXPECT_GT(results.size(), 0);

    // Complex pattern
    results = glob_helper("*.[ct]*");
    EXPECT_GT(results.size(), 0);  // Should match .txt and .cpp files

    // Non-existent directory
    results = glob_helper("nonexistent_dir/*");
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

// Test translate function with complex patterns
TEST_F(GlobTest, TranslateComplexPatterns) {
    // Test basic wildcard
    std::string regex = atom::io::translate("*");
    EXPECT_NE(regex.find(".*"), std::string::npos);

    // Test question mark
    regex = atom::io::translate("file?.txt");
    EXPECT_NE(regex.find("file.\\.txt"), std::string::npos);

    // Test character class
    regex = atom::io::translate("file[123].txt");
    EXPECT_NE(regex.find("[123]"), std::string::npos);

    // Test negated character class
    regex = atom::io::translate("file[!abc].txt");
    EXPECT_NE(regex.find("[^abc]"), std::string::npos);

    // Test range in character class
    regex = atom::io::translate("file[a-z].txt");
    EXPECT_NE(regex.find("[a-z]"), std::string::npos);

    // Test escaped special characters
    regex = atom::io::translate("file\\*.txt");
    EXPECT_NE(regex.find("\\*"), std::string::npos);
}

// Test filter function
TEST_F(GlobTest, FilterFunction) {
    std::vector<fs::path> paths = {fs::path("file1.txt"), fs::path("file2.txt"),
                                   fs::path("file.cpp"),
                                   fs::path("document.md")};

    // Filter for .txt files
    auto filtered = atom::io::filter(paths, "*.txt");
    EXPECT_EQ(filtered.size(), 2);
    EXPECT_THAT(filtered, Contains(fs::path("file1.txt")));
    EXPECT_THAT(filtered, Contains(fs::path("file2.txt")));

    // Filter for .cpp files
    filtered = atom::io::filter(paths, "*.cpp");
    EXPECT_EQ(filtered.size(), 1);
    EXPECT_THAT(filtered, Contains(fs::path("file.cpp")));

    // Filter with question mark
    filtered = atom::io::filter(paths, "file?.txt");
    EXPECT_EQ(filtered.size(), 2);

    // Filter with character class
    filtered = atom::io::filter(paths, "file[12].txt");
    EXPECT_EQ(filtered.size(), 2);

    // Filter with no matches
    filtered = atom::io::filter(paths, "*.xyz");
    EXPECT_THAT(filtered, IsEmpty());
}

// Test expandTilde with various scenarios
TEST_F(GlobTest, ExpandTildeScenarios) {
    // Test with empty path
    fs::path empty_path;
    EXPECT_EQ(atom::io::expandTilde(empty_path), empty_path);

    // Test with path not starting with tilde
    fs::path regular_path = "/home/user/file.txt";
    EXPECT_EQ(atom::io::expandTilde(regular_path), regular_path);

    // Test with tilde at start (should expand to home directory)
    fs::path tilde_path = "~/documents/file.txt";
    auto expanded = atom::io::expandTilde(tilde_path);
    EXPECT_NE(expanded.string().find('~'), 0);  // Tilde should be replaced

    // Test with just tilde
    fs::path just_tilde = "~";
    expanded = atom::io::expandTilde(just_tilde);
    EXPECT_NE(expanded, just_tilde);  // Should be expanded
}

// Test compilePattern function
TEST_F(GlobTest, CompilePattern) {
    // Test that patterns compile without throwing
    EXPECT_NO_THROW(atom::io::compilePattern("*.txt"));
    EXPECT_NO_THROW(atom::io::compilePattern("file?.cpp"));
    EXPECT_NO_THROW(atom::io::compilePattern("file[0-9].txt"));
    EXPECT_NO_THROW(atom::io::compilePattern("**/*.txt"));

    // Test that compiled patterns can be used for matching
    auto pattern = atom::io::compilePattern("*.txt");
    EXPECT_TRUE(std::regex_match("file.txt", pattern));
    EXPECT_FALSE(std::regex_match("file.cpp", pattern));
}

// Test fnmatch with various patterns
TEST_F(GlobTest, FnmatchPatterns) {
    // Basic wildcard
    EXPECT_TRUE(atom::io::fnmatch(fs::path("file.txt"), "*.txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file.txt"), "*.cpp"));

    // Question mark
    EXPECT_TRUE(atom::io::fnmatch(fs::path("file1.txt"), "file?.txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file12.txt"), "file?.txt"));

    // Character class
    EXPECT_TRUE(atom::io::fnmatch(fs::path("file1.txt"), "file[123].txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file4.txt"), "file[123].txt"));

    // Negated character class
    EXPECT_TRUE(atom::io::fnmatch(fs::path("file4.txt"), "file[!123].txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file1.txt"), "file[!123].txt"));

    // Range
    EXPECT_TRUE(atom::io::fnmatch(fs::path("filec.txt"), "file[a-z].txt"));
    EXPECT_FALSE(atom::io::fnmatch(fs::path("file1.txt"), "file[a-z].txt"));
}

// Test hasMagic with edge cases
TEST_F(GlobTest, HasMagicEdgeCases) {
    // Empty string
    EXPECT_FALSE(atom::io::hasMagic(""));

    // Only magic characters
    EXPECT_TRUE(atom::io::hasMagic("*"));
    EXPECT_TRUE(atom::io::hasMagic("?"));
    EXPECT_TRUE(atom::io::hasMagic("["));

    // Magic characters in middle
    EXPECT_TRUE(atom::io::hasMagic("file*name"));
    EXPECT_TRUE(atom::io::hasMagic("file?name"));
    EXPECT_TRUE(atom::io::hasMagic("file[123]name"));

    // Escaped magic characters (still detected as magic)
    EXPECT_TRUE(atom::io::hasMagic("file\\*.txt"));

    // No magic characters
    EXPECT_FALSE(atom::io::hasMagic("simple_filename.txt"));
    EXPECT_FALSE(atom::io::hasMagic("/path/to/file.txt"));
}

// Test isHidden with various paths
TEST_F(GlobTest, IsHiddenVariousPaths) {
    // Hidden files
    EXPECT_TRUE(atom::io::isHidden(".hidden"));
    EXPECT_TRUE(atom::io::isHidden(".hidden.txt"));
    EXPECT_TRUE(atom::io::isHidden("dir/.hidden"));
    EXPECT_TRUE(atom::io::isHidden("/path/to/.hidden"));

    // Non-hidden files
    EXPECT_FALSE(atom::io::isHidden("visible.txt"));
    EXPECT_FALSE(atom::io::isHidden("dir/visible.txt"));
    EXPECT_FALSE(atom::io::isHidden("/path/to/visible.txt"));

    // Edge cases
    EXPECT_FALSE(atom::io::isHidden(""));
    EXPECT_FALSE(atom::io::isHidden(".."));  // Parent directory reference
    EXPECT_FALSE(atom::io::isHidden("."));   // Current directory reference
}

// Test iterDirectory with dironly flag
TEST_F(GlobTest, IterDirectoryDirOnly) {
    // Get all entries
    auto all_entries = atom::io::iterDirectory(fs::path("."), false);
    EXPECT_GT(all_entries.size(), 0);

    // Get only directories
    auto dirs_only = atom::io::iterDirectory(fs::path("."), true);
    EXPECT_GT(dirs_only.size(), 0);

    // Verify all returned entries are directories
    for (const auto& entry : dirs_only) {
        EXPECT_TRUE(fs::is_directory(entry));
    }

    // Directories only should be less than or equal to all entries
    EXPECT_LE(dirs_only.size(), all_entries.size());
}

// Test rlistdir recursively
TEST_F(GlobTest, RlistdirRecursive) {
    // Get all entries recursively
    auto all_recursive = atom::io::rlistdir(fs::path("."), false);
    EXPECT_GT(all_recursive.size(), 0);

    // Get only directories recursively
    auto dirs_recursive = atom::io::rlistdir(fs::path("."), true);
    EXPECT_GT(dirs_recursive.size(), 0);

    // Verify all returned entries are directories
    for (const auto& entry : dirs_recursive) {
        EXPECT_TRUE(fs::is_directory(entry));
    }

    // Should find nested directories
    EXPECT_THAT(all_recursive, Contains(fs::path("dir1")));
    EXPECT_THAT(all_recursive, Contains(fs::path("dir2")));
}

// Test glob0 with various inputs
TEST_F(GlobTest, Glob0Function) {
    // Test with existing file
    auto results = atom::io::glob0(fs::path("."), fs::path("file1.txt"), false);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], fs::path("file1.txt"));

    // Test with non-existent file
    results =
        atom::io::glob0(fs::path("."), fs::path("nonexistent.txt"), false);
    EXPECT_THAT(results, IsEmpty());

    // Test with empty basename
    results = atom::io::glob0(fs::path("."), fs::path(""), false);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], fs::path(""));

    // Test with directory
    results = atom::io::glob0(fs::path("."), fs::path("dir1"), false);
    EXPECT_EQ(results.size(), 1);
}

// Test glob1 with patterns
TEST_F(GlobTest, Glob1Patterns) {
    // Test with simple wildcard
    auto results = atom::io::glob1(fs::path("."), "*.txt", false);
    EXPECT_EQ(results.size(), 2);  // file1.txt and file2.txt

    // Test with question mark
    results = atom::io::glob1(fs::path("."), "file?.txt", false);
    EXPECT_EQ(results.size(), 2);

    // Test with character class
    results = atom::io::glob1(fs::path("."), "file[12].txt", false);
    EXPECT_EQ(results.size(), 2);

    // Test with no matches
    results = atom::io::glob1(fs::path("."), "*.xyz", false);
    EXPECT_THAT(results, IsEmpty());

    // Test dironly flag
    results = atom::io::glob1(fs::path("."), "*", true);
    for (const auto& result : results) {
        EXPECT_TRUE(fs::is_directory(result));
    }
}

// Test glob2 recursive pattern
TEST_F(GlobTest, Glob2Recursive) {
    // Test recursive glob
    auto results = atom::io::glob2(fs::path("."), "**", false);
    EXPECT_GT(results.size(), 0);

    // Should include nested directories
    bool found_nested = false;
    for (const auto& result : results) {
        if (result.string().find("dir1") != std::string::npos ||
            result.string().find("dir2") != std::string::npos) {
            found_nested = true;
            break;
        }
    }
    EXPECT_TRUE(found_nested);

    // Test with dironly flag
    results = atom::io::glob2(fs::path("."), "**", true);
    for (const auto& result : results) {
        EXPECT_TRUE(fs::is_directory(result));
    }
}

// Test stringReplace helper function
TEST_F(GlobTest, StringReplaceFunction) {
    std::string str = "hello world";
    bool replaced = atom::io::stringReplace(str, "world", "universe");
    EXPECT_TRUE(replaced);
    EXPECT_EQ(str, "hello universe");

    // Test with non-existent substring
    str = "hello world";
    replaced = atom::io::stringReplace(str, "foo", "bar");
    EXPECT_FALSE(replaced);
    EXPECT_EQ(str, "hello world");

    // Test with empty string
    str = "";
    replaced = atom::io::stringReplace(str, "foo", "bar");
    EXPECT_FALSE(replaced);
}

// Test complex bracket expressions
TEST_F(GlobTest, ComplexBracketExpressions) {
    // Test range with negation
    auto result =
        globHelper(String{(testDir / "[!a-m]*.txt").string().c_str()});
    EXPECT_THAT(result, Contains(testDir / "nested.txt"));
    EXPECT_THAT(result, Not(Contains(testDir / "file1.txt")));

    // Test multiple ranges
    result =
        globHelper(String{(testDir / "[a-zA-Z0-9]*.txt").string().c_str()});
    EXPECT_GE(result.size(), 2);

    // Test character class with special characters
    result = globHelper(String{(testDir / "[._-]*.txt").string().c_str()});
    // Should match files starting with ., _, or -
}

// Test glob with very long patterns
TEST_F(GlobTest, VeryLongPatterns) {
    // Create a file with a long name
    std::string long_name(200, 'a');
    long_name += ".txt";
    fs::path long_file = testDir / long_name;
    std::ofstream(long_file).close();

    // Test glob with long pattern
    auto result = globHelper(
        String{(testDir / (std::string(200, 'a') + ".txt")).string().c_str()});
    EXPECT_EQ(result.size(), 1);
    EXPECT_THAT(result, Contains(long_file));
}

// Test glob with special characters in directory names
TEST_F(GlobTest, SpecialCharactersInDirectoryNames) {
    // Create directories with special characters
    fs::path special_dir = testDir / "dir with spaces";
    fs::create_directories(special_dir);
    std::ofstream(special_dir / "file.txt").close();

    // Test glob with spaces
    auto result = globHelper(
        String{(testDir / "dir with spaces" / "*.txt").string().c_str()});
    EXPECT_EQ(result.size(), 1);

    // Create directory with parentheses
    fs::path paren_dir = testDir / "dir(with)parens";
    fs::create_directories(paren_dir);
    std::ofstream(paren_dir / "file.txt").close();

    result = globHelper(
        String{(testDir / "dir(with)parens" / "*.txt").string().c_str()});
    EXPECT_EQ(result.size(), 1);
}

// Test glob with empty directory
TEST_F(GlobTest, EmptyDirectory) {
    fs::path empty_dir = testDir / "empty";
    fs::create_directories(empty_dir);

    auto result = globHelper(empty_dir / "*.txt");
    EXPECT_TRUE(result.empty());

    result = atom::io::rglob((empty_dir / "**" / "*.txt").string());
    EXPECT_TRUE(result.empty());
}

// Test glob with circular symlinks
TEST_F(GlobTest, CircularSymlinks) {
#ifdef _WIN32
    GTEST_SKIP() << "Skipping symlink tests on Windows";
#endif

    fs::path link_a = testDir / "link_a";
    fs::path link_b = testDir / "link_b";

    std::error_code ec;
    fs::create_directory_symlink(link_b, link_a, ec);
    fs::create_directory_symlink(link_a, link_b, ec);

    // Glob should handle circular symlinks gracefully
    EXPECT_NO_THROW({
        auto result = globHelper(String{(testDir / "*").string().c_str()});
    });
}

// Test glob with deep nesting
TEST_F(GlobTest, DeepNesting) {
    // Create deeply nested structure
    fs::path deep_path = testDir;
    for (int i = 0; i < 20; ++i) {
        deep_path /= ("level" + std::to_string(i));
    }
    fs::create_directories(deep_path);
    std::ofstream(deep_path / "deep.txt").close();

    // Test recursive glob
    auto result = atom::io::rglob((testDir / "**" / "deep.txt").string());
    EXPECT_EQ(result.size(), 1);
    EXPECT_THAT(result, Contains(deep_path / "deep.txt"));
}

// Test glob with multiple wildcards
TEST_F(GlobTest, MultipleWildcards) {
    // Create test files
    std::ofstream(testDir / "abc_def_ghi.txt").close();
    std::ofstream(testDir / "abc_xyz_ghi.txt").close();
    std::ofstream(testDir / "abc_def_xyz.txt").close();

    // Test pattern with multiple wildcards
    auto result =
        globHelper(String{(testDir / "abc_*_ghi.txt").string().c_str()});
    EXPECT_EQ(result.size(), 2);

    result = globHelper(String{(testDir / "*_*_*.txt").string().c_str()});
    EXPECT_GE(result.size(), 3);
}

// Test glob error handling
TEST_F(GlobTest, ErrorHandling) {
    // Test with non-existent directory
    auto result = globHelper(
        String{(testDir / "nonexistent" / "*.txt").string().c_str()});
    EXPECT_TRUE(result.empty());

    // Test with invalid pattern characters (platform-specific)
    EXPECT_NO_THROW({
        result =
            globHelper(String{(testDir / "**" / "*.txt").string().c_str()});
    });
}

// Test filter function with complex predicates
TEST_F(GlobTest, FilterComplexPredicates) {
    Vector<fs::path> paths = {testDir / "file1.txt", testDir / "file2.txt",
                              testDir / "nested.txt", testDir / "other.dat"};

    // Filter for .txt files only
    auto result = atom::io::filter(paths, "*.txt");
    EXPECT_EQ(result.size(), 3);

    // Filter for specific pattern
    result = atom::io::filter(paths, "*1.txt");
    EXPECT_EQ(result.size(), 1);

    // Filter with no matches
    result = atom::io::filter(paths, "*.xyz");
    EXPECT_TRUE(result.empty());
}

// Test concurrent glob operations
TEST_F(GlobTest, ConcurrentGlobOperations) {
    const int num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &success_count]() {
            auto result =
                globHelper(String{(testDir / "*.txt").string().c_str()});
            if (!result.empty()) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads);
}

// Test glob with hidden files
TEST_F(GlobTest, HiddenFilesHandling) {
    // Create hidden file
    fs::path hidden = testDir / ".hidden.txt";
    std::ofstream(hidden).close();

    // Test that isHidden works
    EXPECT_TRUE(atom::io::isHidden(String(hidden.string().c_str())));
    EXPECT_FALSE(
        atom::io::isHidden(String((testDir / "file1.txt").string().c_str())));

    // Glob should find hidden files when explicitly requested
    auto result = globHelper(String{(testDir / ".*").string().c_str()});
    EXPECT_GE(result.size(), 1);
}

// Test glob performance with large directory
TEST_F(GlobTest, LargeDirectoryPerformance) {
    // Create many files
    fs::path large_dir = testDir / "large";
    fs::create_directories(large_dir);

    for (int i = 0; i < 100; ++i) {
        std::ofstream(large_dir / ("file" + std::to_string(i) + ".txt"))
            .close();
    }

    // Time the glob operation
    auto start = std::chrono::high_resolution_clock::now();
    auto result = globHelper(large_dir / "*.txt");
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(result.size(), 100);

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Should complete in reasonable time (< 1 second for 100 files)
    EXPECT_LT(duration.count(), 1000);
}

// Test translate function edge cases
TEST_F(GlobTest, TranslateEdgeCases) {
    // Test empty pattern
    auto regex = atom::io::translate("");
    EXPECT_FALSE(regex.empty());

    // Test pattern with only wildcards
    regex = atom::io::translate("***");
    EXPECT_FALSE(regex.empty());

    // Test pattern with escaped characters
    regex = atom::io::translate("file\\*.txt");
    EXPECT_FALSE(regex.empty());
}

// Test fnmatch edge cases
TEST_F(GlobTest, FnmatchEdgeCases) {
    // Test empty strings
    EXPECT_TRUE(atom::io::fnmatch("", ""));
    EXPECT_FALSE(atom::io::fnmatch("*", ""));

    // Test case sensitivity
    EXPECT_TRUE(
        atom::io::fnmatch("*.txt", "FILE.TXT"));  // Case insensitive by default

    // Test with path separators
    EXPECT_TRUE(atom::io::fnmatch("dir/*.txt", "dir/file.txt"));

    // Test complex patterns
    EXPECT_TRUE(atom::io::fnmatch("[a-z]*.txt", "abc.txt"));
    EXPECT_FALSE(atom::io::fnmatch("[a-z]*.txt", "123.txt"));
}
