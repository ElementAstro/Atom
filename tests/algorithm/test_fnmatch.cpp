#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "atom/algorithm/fnmatch.hpp"
#include "atom/log/loguru.hpp"
#include "atom/macro.hpp"

using namespace atom::algorithm;
using namespace std::string_literals;

// Test fixture for fnmatch tests
class FnmatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    // Helper function to generate random strings for performance testing
    std::string generateRandomString(size_t length) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(
            32, 126);  // ASCII printable chars

        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
        return result;
    }

    // Common test cases
    const std::vector<std::string> filenames = {
        "file.txt",       "file.jpg",   "document.pdf",
        "image.png",      "script.py",  "config.ini",
        "readme.md",      "index.html", "main.cpp",
        "CMakeLists.txt", "data.csv",   "log.log",
        ".gitignore",     ".hidden",    "file with spaces.txt"};
};

// Basic functionality tests
TEST_F(FnmatchTest, BasicMatching) {
    // Simple exact matches
    EXPECT_TRUE(fnmatch("file.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file.txt", "file.jpg"));

    // Question mark wildcard
    EXPECT_TRUE(fnmatch("file.???", "file.txt"));
    EXPECT_TRUE(fnmatch("file.???", "file.jpg"));
    EXPECT_FALSE(fnmatch("file.???", "file.html"));

    // Asterisk wildcard
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("*.txt", "file.jpg"));
    EXPECT_TRUE(fnmatch("file*", "file.txt"));
    EXPECT_TRUE(fnmatch("*.*", "file.txt"));
    EXPECT_FALSE(fnmatch("*.*", "filename"));
}

TEST_F(FnmatchTest, CharacterClasses) {
    // Basic character class
    EXPECT_TRUE(fnmatch("file.[tj]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[tj]*", "file.jpg"));
    EXPECT_FALSE(fnmatch("file.[tj]*", "file.png"));

    // Negated character class
    EXPECT_FALSE(fnmatch("file.[!tj]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[!tj]*", "file.jpg"));
    EXPECT_TRUE(fnmatch("file.[!tj]*", "file.png"));

    // Character ranges
    EXPECT_TRUE(fnmatch("file.[a-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[A-Z]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[0-9a-z]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[0-9a-z]*", "file.1txt"));

    // Special characters in character class
    EXPECT_TRUE(fnmatch("file.[.]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[^.]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[*?]*", "file?txt"));
}

TEST_F(FnmatchTest, ComplexPatterns) {
    // Multiple wildcards
    EXPECT_TRUE(fnmatch("*.*", "file.txt"));
    EXPECT_TRUE(fnmatch("f*.t*", "file.txt"));
    EXPECT_TRUE(fnmatch("*i*.*t*", "file.txt"));
    EXPECT_FALSE(fnmatch("*z*.*", "file.txt"));

    // Multiple character classes
    EXPECT_TRUE(fnmatch("[a-z]*.[a-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("[A-Z]*.[a-z]*", "file.txt"));

    // Combinations
    EXPECT_TRUE(fnmatch("*[aeiou]*.[!b-df-hj-np-tv-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("*[aeiou]*.[!b-df-hj-np-tv-z]*", "file.jpg"));

    // Complex nested patterns
    EXPECT_TRUE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "file.txt"));
    EXPECT_TRUE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "main.cpp"));
    EXPECT_FALSE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "a1.txt"));
}

TEST_F(FnmatchTest, Escapes) {
    // Test escaped special characters
    EXPECT_TRUE(fnmatch("file\\.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file\\.txt", "file-txt"));

    // Escaped wildcards
    EXPECT_TRUE(fnmatch("file\\*.txt", "file*.txt"));
    EXPECT_FALSE(fnmatch("file\\*.txt", "filename.txt"));

    EXPECT_TRUE(fnmatch("file\\?.txt", "file?.txt"));
    EXPECT_FALSE(fnmatch("file\\?.txt", "filex.txt"));

    // Escaped brackets
    EXPECT_TRUE(fnmatch("file\\[abc].txt", "file[abc].txt"));
    EXPECT_FALSE(fnmatch("file\\[abc].txt", "filec.txt"));

    // Test with NOESCAPE flag
    EXPECT_FALSE(fnmatch("file\\.txt", "file.txt", flags::NOESCAPE));
    EXPECT_TRUE(fnmatch("file\\.txt", "file\\.txt", flags::NOESCAPE));
}

TEST_F(FnmatchTest, CasefoldsFlag) {
    // Case-sensitive by default
    EXPECT_TRUE(fnmatch("file.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file.txt", "FILE.TXT"));

    // Case-insensitive with CASEFOLD flag
    EXPECT_TRUE(fnmatch("file.txt", "FILE.TXT", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("FILE.TXT", "file.txt", flags::CASEFOLD));

    // Mixed case patterns
    EXPECT_TRUE(fnmatch("F[Ii]Le.*", "file.txt", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("F[Ii]Le.*", "FILE.TXT", flags::CASEFOLD));
    EXPECT_FALSE(fnmatch("F[Ii]Le.*", "bile.txt", flags::CASEFOLD));

    // Character classes with CASEFOLD
    EXPECT_TRUE(fnmatch("[A-Z]*.txt", "file.txt", flags::CASEFOLD));
    EXPECT_FALSE(fnmatch("[A-Z]*.txt", "123.txt", flags::CASEFOLD));
}

// Filter tests
TEST_F(FnmatchTest, BasicFilter) {
    // Simple filtering
    bool has_txt = filter(filenames, "*.txt");
    bool has_exe = filter(filenames, "*.exe");

    EXPECT_TRUE(has_txt);
    EXPECT_FALSE(has_exe);

    // Multiple file types
    bool has_images = filter(filenames, "*.jpg") || filter(filenames, "*.png");
    EXPECT_TRUE(has_images);
}

TEST_F(FnmatchTest, MultiplePatternFilter) {
    // Test with fixed patterns
    std::vector<std::string> patterns = {"*.txt", "*.jpg", "*.md"};
    auto matched = filter(filenames, patterns);

    ASSERT_EQ(matched.size(), 3);
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "file.txt") !=
                matched.end());
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "file.jpg") !=
                matched.end());
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "readme.md") !=
                matched.end());

    // Test with empty patterns
    std::vector<std::string> empty_patterns;
    auto empty_matched = filter(filenames, empty_patterns);
    EXPECT_TRUE(empty_matched.empty());

    // Test with flags
    std::vector<std::string> case_patterns = {"*.TXT", "*.JPG"};
    auto case_matched = filter(filenames, case_patterns, flags::CASEFOLD);

    ASSERT_EQ(case_matched.size(), 2);
    EXPECT_TRUE(std::find(case_matched.begin(), case_matched.end(),
                          "file.txt") != case_matched.end());
}

TEST_F(FnmatchTest, FilterParallelExecution) {
    // Generate a large dataset to test parallel execution
    std::vector<std::string> large_dataset;
    large_dataset.reserve(1000);

    for (int i = 0; i < 1000; ++i) {
        large_dataset.push_back("file" + std::to_string(i) + ".txt");
    }

    // Add some non-matching files
    for (int i = 0; i < 1000; ++i) {
        large_dataset.push_back("doc" + std::to_string(i) + ".pdf");
    }

    std::vector<std::string> patterns = {"*.txt", "*.jpg"};

    // Test with both sequential and parallel execution
    auto start_seq = std::chrono::high_resolution_clock::now();
    auto matched_seq = filter(large_dataset, patterns, 0, false);
    auto end_seq = std::chrono::high_resolution_clock::now();

    auto start_par = std::chrono::high_resolution_clock::now();
    auto matched_par = filter(large_dataset, patterns, 0, true);
    auto end_par = std::chrono::high_resolution_clock::now();

    // Results should be identical
    ASSERT_EQ(matched_seq.size(), matched_par.size());
    std::sort(matched_seq.begin(), matched_seq.end());
    std::sort(matched_par.begin(), matched_par.end());
    EXPECT_EQ(matched_seq, matched_par);

    // Parallel execution should not be slower (this is a soft check)
    auto seq_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_seq - start_seq)
                            .count();
    auto par_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_par - start_par)
                            .count();

    std::cout << "Sequential execution: " << seq_duration << "ms" << std::endl;
    std::cout << "Parallel execution: " << par_duration << "ms" << std::endl;

    // On sufficiently large datasets, parallel execution should be faster
    // But this may not always be true depending on hardware/test environment
    // So this is more of an informational output than a strict test
}

// Pattern translation tests
TEST_F(FnmatchTest, TranslateBasicPattern) {
    auto result = translate("file.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "file\\.txt");

    result = translate("*.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ".*\\.txt");

    result = translate("file.?");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "file\\..");

    result = translate("file[abc].txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "file[abc]\\.txt");
}

TEST_F(FnmatchTest, TranslateComplexPatterns) {
    auto result = translate("*[a-z]file?.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), ".*[a-z]file\\.\\.txt");

    // With case folding
    result = translate("File.txt", flags::CASEFOLD);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "[Ff][Ii][Ll][Ee]\\.[Tt][Xx][Tt]");

    // With escapes
    result = translate("file\\*.txt");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "file\\*\\.txt");
}

TEST_F(FnmatchTest, TranslateInvalidPatterns) {
    auto result = translate("[abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::UnmatchedBracket);

    result = translate("file\\");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::EscapeAtEnd);

    result = translate("[");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::UnmatchedBracket);
}

// Error handling tests
TEST_F(FnmatchTest, ErrorHandlingInFnmatch) {
    // Unmatched bracket
    EXPECT_THROW(
        { ATOM_UNUSED_RESULT(fnmatch("[abc", "abc")); }, FnmatchException);

    // Try to match the error message
    try {
        EXPECT_THROW(
            { ATOM_UNUSED_RESULT(fnmatch("[abc", "abc")); }, FnmatchException);
        FAIL() << "Expected FnmatchException";
    } catch (const FnmatchException& e) {
        EXPECT_STREQ(e.what(), "Unmatched bracket in pattern");
    }

    // Escape at end of pattern
    EXPECT_THROW(
        { ATOM_UNUSED_RESULT(fnmatch("abc\\", "abc")); }, FnmatchException);

    try {
        EXPECT_THROW(
            { ATOM_UNUSED_RESULT(fnmatch("abc\\", "abc")); }, FnmatchException);
    } catch (const FnmatchException& e) {
        EXPECT_STREQ(e.what(), "Escape character at end of pattern");
    }
}

TEST_F(FnmatchTest, ErrorHandlingInNothrow) {
    // Test the nothrow version of fnmatch
    auto result = fnmatch_nothrow("[abc", "abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::UnmatchedBracket);

    result = fnmatch_nothrow("abc\\", "abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::EscapeAtEnd);

    // Valid pattern should return value
    result = fnmatch_nothrow("abc", "abc");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

// Edge cases and special conditions
TEST_F(FnmatchTest, EdgeCases) {
    // Empty patterns and strings
    EXPECT_TRUE(fnmatch("", ""));
    EXPECT_FALSE(fnmatch("", "abc"));
    EXPECT_FALSE(fnmatch("abc", ""));

    // Just wildcards
    EXPECT_TRUE(fnmatch("*", "anything"));
    EXPECT_TRUE(fnmatch("*", ""));
    EXPECT_TRUE(fnmatch("?", "a"));
    EXPECT_FALSE(fnmatch("?", ""));
    EXPECT_FALSE(fnmatch("?", "ab"));

    // Patterns with just brackets
    EXPECT_TRUE(fnmatch("[a]", "a"));
    EXPECT_FALSE(fnmatch("[a]", "b"));
    EXPECT_TRUE(fnmatch("[!a]", "b"));
    EXPECT_FALSE(fnmatch("[!a]", "a"));

    // Special case handling in character classes
    EXPECT_TRUE(fnmatch("[[]]", "["));  // Match [ in a character class
    EXPECT_TRUE(fnmatch("[]]", "]"));   // Match ] in a character class

    // Multiple asterisks (should behave like a single asterisk)
    EXPECT_TRUE(fnmatch("**", "anything"));
    EXPECT_TRUE(fnmatch("a**b", "ab"));
    EXPECT_TRUE(fnmatch("a**b", "axyzb"));
}

TEST_F(FnmatchTest, SpecialCharacters) {
    // Test with various special characters
    EXPECT_TRUE(fnmatch("file-*.txt", "file-1.txt"));
    EXPECT_TRUE(fnmatch("file+*.txt", "file+1.txt"));
    EXPECT_TRUE(fnmatch("file $*.txt", "file $1.txt"));

    // Test with Unicode characters (if supported by the system)
    EXPECT_TRUE(fnmatch("file_üñî*.txt", "file_üñîçøðé.txt"));

    // Test with very long strings
    std::string long_name(1000, 'a');
    EXPECT_TRUE(fnmatch("*", long_name));

    std::string long_pattern = "*" + std::string(500, '?') + "*";
    std::string matching_name = "prefix" + std::string(500, 'x') + "suffix";
    EXPECT_TRUE(fnmatch(long_pattern, matching_name));
}

// Performance tests
TEST_F(FnmatchTest, PerformanceBasicPatterns) {
    // Generate test data
    const int num_iterations = 1000;
    std::string pattern = "*.txt";
    std::string matching = "longfilename.txt";
    std::string non_matching = "document.pdf";

    // Measure performance for matching case
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(pattern, matching));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto matching_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    // Measure performance for non-matching case
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(pattern, non_matching));
    }
    end = std::chrono::high_resolution_clock::now();
    auto non_matching_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    std::cout << "Performance for " << num_iterations
              << " iterations:" << std::endl;
    std::cout << "  Matching case: " << matching_duration << " μs" << std::endl;
    std::cout << "  Non-matching case: " << non_matching_duration << " μs"
              << std::endl;

    // This is more of a benchmark than an assertion
    // We just ensure it completes in a reasonable time
}

TEST_F(FnmatchTest, PerformanceComplexPatterns) {
    // Test with more complex patterns
    const int num_iterations = 100;
    std::string complex_pattern = "*[a-z0-9]?[!.][a-z]*.txt";
    std::string long_string =
        generateRandomString(1000) + ".txt";  // Should match

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(complex_pattern, long_string));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    std::cout << "Performance for complex pattern (" << num_iterations
              << " iterations): " << duration << " μs" << std::endl;
}

TEST_F(FnmatchTest, PerformanceMultipleFilters) {
    // Generate a large dataset
    std::vector<std::string> large_dataset;
    large_dataset.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        if (i % 2 == 0) {
            large_dataset.push_back("file" + std::to_string(i) + ".txt");
        } else {
            large_dataset.push_back("doc" + std::to_string(i) + ".pdf");
        }
    }

    std::vector<std::string> patterns = {"*.txt", "file*0.pdf", "doc*9.pdf"};

    auto start = std::chrono::high_resolution_clock::now();
    auto matched = filter(large_dataset, patterns, 0, true);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();

    std::cout << "Performance for filtering 10000 files with 3 patterns: "
              << duration << " ms" << std::endl;
    std::cout << "Matched files: " << matched.size() << std::endl;
}

// Thread safety tests
TEST_F(FnmatchTest, ThreadSafety) {
    // Test concurrent matching operations
    std::string pattern = "*.txt";
    std::vector<std::string> test_strings = {
        "file1.txt", "file2.doc", "file3.txt", "file4.pdf",
        "file5.txt", "file6.jpg", "file7.txt", "file8.png"};

    std::vector<std::future<bool>> futures;
    for (const auto& str : test_strings) {
        futures.push_back(std::async(std::launch::async, [&pattern, str]() {
            return fnmatch(pattern, str);
        }));
    }

    std::vector<bool> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }

    // Verify correct results
    std::vector<bool> expected = {true, false, true, false,
                                  true, false, true, false};

    EXPECT_EQ(results, expected);
}

TEST_F(FnmatchTest, ThreadSafetyWithPatternCache) {
    // Test concurrent operations that use the pattern cache
    const int num_threads = 10;
    std::string pattern = "file[0-9]*.txt";

    // Each thread will perform multiple matches with the same pattern
    // This tests the thread safety of pattern caching
    std::vector<std::future<void>> futures;
    std::vector<std::array<bool, 4>> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        futures.push_back(
            std::async(std::launch::async, [i, &pattern, &results]() {
                results[i][0] = fnmatch(pattern, "file1.txt");
                results[i][1] = fnmatch(pattern, "file20.txt");
                results[i][2] = fnmatch(pattern, "file.txt");
                results[i][3] = fnmatch(pattern, "fileX.txt");
            }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        future.get();
    }

    // All threads should get the same results
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[i], results[0]);
    }

    // Verify correct results
    EXPECT_TRUE(results[0][0]);   // "file1.txt" should match
    EXPECT_TRUE(results[0][1]);   // "file20.txt" should match
    EXPECT_FALSE(results[0][2]);  // "file.txt" should not match (no digit)
    EXPECT_FALSE(
        results[0][3]);  // "fileX.txt" should not match (X is not a digit)
}

// System-specific tests
#ifndef _WIN32
TEST_F(FnmatchTest, SystemFnmatchCompatibility) {
    // On non-Windows systems, test compatibility with system fnmatch

    // Basic patterns
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("*.txt", "file.jpg"));

    // More complex patterns
    EXPECT_TRUE(fnmatch("file[1-9].txt", "file5.txt"));
    EXPECT_FALSE(fnmatch("file[1-9].txt", "fileA.txt"));

    // Test with flags
    EXPECT_TRUE(fnmatch("FILE.TXT", "file.txt", flags::CASEFOLD));

    // Test special cases
    EXPECT_TRUE(fnmatch("*", "anything"));
    EXPECT_TRUE(fnmatch("?", "a"));
    EXPECT_FALSE(fnmatch("?", "ab"));
}
#endif

// Integration tests with filesystem
TEST_F(FnmatchTest, FilesystemIntegration) {
    // Create temporary directory structure for testing
    std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fnmatch_test";

    try {
        // Clean up from previous test runs if needed
        if (std::filesystem::exists(temp_dir)) {
            std::filesystem::remove_all(temp_dir);
        }

        // Create directory structure
        std::filesystem::create_directory(temp_dir);
        std::filesystem::create_directory(temp_dir / "subdir1");
        std::filesystem::create_directory(temp_dir / "subdir2");

        // Create some test files
        std::ofstream file1(temp_dir / "file1.txt");
        file1.close();
        std::ofstream file2(temp_dir / "file2.txt");
        file2.close();
        std::ofstream doc(temp_dir / "document.pdf");
        doc.close();
        std::ofstream img(temp_dir / "image.jpg");
        img.close();
        std::ofstream nested(temp_dir / "subdir1" / "nested.txt");
        nested.close();
        std::ofstream other(temp_dir / "subdir2" / "other.pdf");
        other.close();

        // Collect file names
        std::vector<std::string> all_files;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(temp_dir)) {
            if (entry.is_regular_file()) {
                all_files.push_back(entry.path().filename().string());
            }
        }

        // Apply fnmatch filtering
        std::vector<std::string> txt_files;
        for (const auto& file : all_files) {
            if (fnmatch("*.txt", file)) {
                txt_files.push_back(file);
            }
        }

        // Verify results
        ASSERT_EQ(txt_files.size(), 3);
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "file1.txt") != txt_files.end());
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "file2.txt") != txt_files.end());
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "nested.txt") != txt_files.end());

        // Test with multiple patterns
        std::vector<std::string> patterns = {"*.pdf", "*.jpg"};
        auto media_files = filter(all_files, patterns);

        ASSERT_EQ(media_files.size(), 3);
        EXPECT_TRUE(std::find(media_files.begin(), media_files.end(),
                              "document.pdf") != media_files.end());
        EXPECT_TRUE(std::find(media_files.begin(), media_files.end(),
                              "image.jpg") != media_files.end());
        EXPECT_TRUE(std::find(media_files.begin(), media_files.end(),
                              "other.pdf") != media_files.end());

    } catch (const std::exception& e) {
        std::cerr << "Filesystem test exception: " << e.what() << std::endl;
        // Clean up
        if (std::filesystem::exists(temp_dir)) {
            std::filesystem::remove_all(temp_dir);
        }
        FAIL() << "Filesystem test failed: " << e.what();
    }

    // Clean up
    if (std::filesystem::exists(temp_dir)) {
        std::filesystem::remove_all(temp_dir);
    }
}

// Test explicit template instantiations
TEST_F(FnmatchTest, ExplicitTemplateInstantiations) {
    // Test with std::string
    std::string pattern_str = "*.txt";
    std::string text_str = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_str, text_str));

    // Test with string literals
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));

    // Test with string_view
    std::string_view pattern_view = "*.txt";
    std::string_view text_view = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_view, text_view));

    // Test with char*
    const char* pattern_cstr = "*.txt";
    const char* text_cstr = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_cstr, text_cstr));

    // Test with mixed types
    EXPECT_TRUE(fnmatch(pattern_str, text_view));
    EXPECT_TRUE(fnmatch(pattern_view, text_cstr));
    EXPECT_TRUE(fnmatch(pattern_cstr, text_str));
}

// Test for SIMD optimizations if available
#ifdef __SSE4_2__
TEST_F(FnmatchTest, SimdOptimizations) {
    // Generate a string likely to benefit from SIMD
    std::string long_text(1000, 'a');
    std::string pattern = "*" + std::string(10, 'b') + "*";

    // Non-matching case should return quickly due to SIMD acceleration
    auto start = std::chrono::high_resolution_clock::now();
    bool result = fnmatch(pattern, long_text);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_FALSE(result);

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    std::cout << "SIMD acceleration test duration: " << duration << " μs"
              << std::endl;

    // We can't make strict assertions about performance, but we can log it
}
#endif
