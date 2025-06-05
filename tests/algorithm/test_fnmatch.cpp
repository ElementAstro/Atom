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
#include "spdlog/spdlog.h"

using namespace atom::algorithm;
using namespace std::string_literals;

class FnmatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
            initialized = true;
        }
    }

    std::string generateRandomString(size_t length) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(32, 126);
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(static_cast<char>(dis(gen)));
        }
        return result;
    }

    const std::vector<std::string> filenames = {
        "file.txt",       "file.jpg",   "document.pdf",
        "image.png",      "script.py",  "config.ini",
        "readme.md",      "index.html", "main.cpp",
        "CMakeLists.txt", "data.csv",   "log.log",
        ".gitignore",     ".hidden",    "file with spaces.txt"};
};

TEST_F(FnmatchTest, BasicMatching) {
    EXPECT_TRUE(fnmatch("file.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file.txt", "file.jpg"));
    EXPECT_TRUE(fnmatch("file.???", "file.txt"));
    EXPECT_TRUE(fnmatch("file.???", "file.jpg"));
    EXPECT_FALSE(fnmatch("file.???", "file.html"));
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("*.txt", "file.jpg"));
    EXPECT_TRUE(fnmatch("file*", "file.txt"));
    EXPECT_TRUE(fnmatch("*.*", "file.txt"));
    EXPECT_FALSE(fnmatch("*.*", "filename"));
}

TEST_F(FnmatchTest, CharacterClasses) {
    EXPECT_TRUE(fnmatch("file.[tj]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[tj]*", "file.jpg"));
    EXPECT_FALSE(fnmatch("file.[tj]*", "file.png"));
    EXPECT_FALSE(fnmatch("file.[!tj]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[!tj]*", "file.jpg"));
    EXPECT_TRUE(fnmatch("file.[!tj]*", "file.png"));
    EXPECT_TRUE(fnmatch("file.[a-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[A-Z]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[0-9a-z]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[0-9a-z]*", "file.1txt"));
    EXPECT_TRUE(fnmatch("file.[.]*", "file.txt"));
    EXPECT_FALSE(fnmatch("file.[^.]*", "file.txt"));
    EXPECT_TRUE(fnmatch("file.[*?]*", "file?txt"));
}

TEST_F(FnmatchTest, ComplexPatterns) {
    EXPECT_TRUE(fnmatch("*.*", "file.txt"));
    EXPECT_TRUE(fnmatch("f*.t*", "file.txt"));
    EXPECT_TRUE(fnmatch("*i*.*t*", "file.txt"));
    EXPECT_FALSE(fnmatch("*z*.*", "file.txt"));
    EXPECT_TRUE(fnmatch("[a-z]*.[a-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("[A-Z]*.[a-z]*", "file.txt"));
    EXPECT_TRUE(fnmatch("*[aeiou]*.[!b-df-hj-np-tv-z]*", "file.txt"));
    EXPECT_FALSE(fnmatch("*[aeiou]*.[!b-df-hj-np-tv-z]*", "file.jpg"));
    EXPECT_TRUE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "file.txt"));
    EXPECT_TRUE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "main.cpp"));
    EXPECT_FALSE(fnmatch("*[!.][a-z]?[a-z][!0-9]*", "a1.txt"));
}

TEST_F(FnmatchTest, Escapes) {
    EXPECT_TRUE(fnmatch("file\\.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file\\.txt", "file-txt"));
    EXPECT_TRUE(fnmatch("file\\*.txt", "file*.txt"));
    EXPECT_FALSE(fnmatch("file\\*.txt", "filename.txt"));
    EXPECT_TRUE(fnmatch("file\\?.txt", "file?.txt"));
    EXPECT_FALSE(fnmatch("file\\?.txt", "filex.txt"));
    EXPECT_TRUE(fnmatch("file\\[abc].txt", "file[abc].txt"));
    EXPECT_FALSE(fnmatch("file\\[abc].txt", "filec.txt"));
    EXPECT_FALSE(fnmatch("file\\.txt", "file.txt", flags::NOESCAPE));
    EXPECT_TRUE(fnmatch("file\\.txt", "file\\.txt", flags::NOESCAPE));
}

TEST_F(FnmatchTest, CasefoldsFlag) {
    EXPECT_TRUE(fnmatch("file.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("file.txt", "FILE.TXT"));
    EXPECT_TRUE(fnmatch("file.txt", "FILE.TXT", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("FILE.TXT", "file.txt", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("F[Ii]Le.*", "file.txt", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("F[Ii]Le.*", "FILE.TXT", flags::CASEFOLD));
    EXPECT_FALSE(fnmatch("F[Ii]Le.*", "bile.txt", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("[A-Z]*.txt", "file.txt", flags::CASEFOLD));
    EXPECT_FALSE(fnmatch("[A-Z]*.txt", "123.txt", flags::CASEFOLD));
}

TEST_F(FnmatchTest, BasicFilter) {
    bool has_txt = filter(filenames, "*.txt");
    bool has_exe = filter(filenames, "*.exe");
    EXPECT_TRUE(has_txt);
    EXPECT_FALSE(has_exe);
    bool has_images = filter(filenames, "*.jpg") || filter(filenames, "*.png");
    EXPECT_TRUE(has_images);
}

TEST_F(FnmatchTest, MultiplePatternFilter) {
    std::vector<std::string> patterns = {"*.txt", "*.jpg", "*.md"};
    auto matched = filter(filenames, patterns);
    ASSERT_EQ(matched.size(), 3);
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "file.txt") !=
                matched.end());
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "file.jpg") !=
                matched.end());
    EXPECT_TRUE(std::find(matched.begin(), matched.end(), "readme.md") !=
                matched.end());
    std::vector<std::string> empty_patterns;
    auto empty_matched = filter(filenames, empty_patterns);
    EXPECT_TRUE(empty_matched.empty());
    std::vector<std::string> case_patterns = {"*.TXT", "*.JPG"};
    auto case_matched = filter(filenames, case_patterns, flags::CASEFOLD);
    ASSERT_EQ(case_matched.size(), 2);
    EXPECT_TRUE(std::find(case_matched.begin(), case_matched.end(),
                          "file.txt") != case_matched.end());
}

TEST_F(FnmatchTest, FilterParallelExecution) {
    std::vector<std::string> large_dataset;
    large_dataset.reserve(2000);
    for (int i = 0; i < 1000; ++i)
        large_dataset.push_back("file" + std::to_string(i) + ".txt");
    for (int i = 0; i < 1000; ++i)
        large_dataset.push_back("doc" + std::to_string(i) + ".pdf");
    std::vector<std::string> patterns = {"*.txt", "*.jpg"};
    auto start_seq = std::chrono::high_resolution_clock::now();
    auto matched_seq = filter(large_dataset, patterns, 0, false);
    auto end_seq = std::chrono::high_resolution_clock::now();
    auto start_par = std::chrono::high_resolution_clock::now();
    auto matched_par = filter(large_dataset, patterns, 0, true);
    auto end_par = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(matched_seq.size(), matched_par.size());
    std::sort(matched_seq.begin(), matched_seq.end());
    std::sort(matched_par.begin(), matched_par.end());
    EXPECT_EQ(matched_seq, matched_par);
    auto seq_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_seq - start_seq)
                            .count();
    auto par_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_par - start_par)
                            .count();
    spdlog::info("Sequential execution: {}ms", seq_duration);
    spdlog::info("Parallel execution: {}ms", par_duration);
}

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
    result = translate("File.txt", flags::CASEFOLD);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "[Ff][Ii][Ll][Ee]\\.[Tt][Xx][Tt]");
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

TEST_F(FnmatchTest, ErrorHandlingInFnmatch) {
    EXPECT_THROW(
        { ATOM_UNUSED_RESULT(fnmatch("[abc", "abc")); }, FnmatchException);
    try {
        EXPECT_THROW(
            { ATOM_UNUSED_RESULT(fnmatch("[abc", "abc")); }, FnmatchException);
        FAIL() << "Expected FnmatchException";
    } catch (const FnmatchException& e) {
        EXPECT_STREQ(e.what(), "Unmatched bracket in pattern");
    }
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
    auto result = fnmatch_nothrow("[abc", "abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::UnmatchedBracket);
    result = fnmatch_nothrow("abc\\", "abc");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FnmatchError::EscapeAtEnd);
    result = fnmatch_nothrow("abc", "abc");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
}

TEST_F(FnmatchTest, EdgeCases) {
    EXPECT_TRUE(fnmatch("", ""));
    EXPECT_FALSE(fnmatch("", "abc"));
    EXPECT_FALSE(fnmatch("abc", ""));
    EXPECT_TRUE(fnmatch("*", "anything"));
    EXPECT_TRUE(fnmatch("*", ""));
    EXPECT_TRUE(fnmatch("?", "a"));
    EXPECT_FALSE(fnmatch("?", ""));
    EXPECT_FALSE(fnmatch("?", "ab"));
    EXPECT_TRUE(fnmatch("[a]", "a"));
    EXPECT_FALSE(fnmatch("[a]", "b"));
    EXPECT_TRUE(fnmatch("[!a]", "b"));
    EXPECT_FALSE(fnmatch("[!a]", "a"));
    EXPECT_TRUE(fnmatch("[[]]", "["));
    EXPECT_TRUE(fnmatch("[]]", "]"));
    EXPECT_TRUE(fnmatch("**", "anything"));
    EXPECT_TRUE(fnmatch("a**b", "ab"));
    EXPECT_TRUE(fnmatch("a**b", "axyzb"));
}

TEST_F(FnmatchTest, SpecialCharacters) {
    EXPECT_TRUE(fnmatch("file-*.txt", "file-1.txt"));
    EXPECT_TRUE(fnmatch("file+*.txt", "file+1.txt"));
    EXPECT_TRUE(fnmatch("file $*.txt", "file $1.txt"));
    EXPECT_TRUE(fnmatch("file_üñî*.txt", "file_üñîçøðé.txt"));
    std::string long_name(1000, 'a');
    EXPECT_TRUE(fnmatch("*", long_name));
    std::string long_pattern = "*" + std::string(500, '?') + "*";
    std::string matching_name = "prefix" + std::string(500, 'x') + "suffix";
    EXPECT_TRUE(fnmatch(long_pattern, matching_name));
}

TEST_F(FnmatchTest, PerformanceBasicPatterns) {
    const int num_iterations = 1000;
    std::string pattern = "*.txt";
    std::string matching = "longfilename.txt";
    std::string non_matching = "document.pdf";
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(pattern, matching));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto matching_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(pattern, non_matching));
    }
    end = std::chrono::high_resolution_clock::now();
    auto non_matching_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    spdlog::info("Performance for {} iterations:", num_iterations);
    spdlog::info("  Matching case: {} μs", matching_duration);
    spdlog::info("  Non-matching case: {} μs", non_matching_duration);
}

TEST_F(FnmatchTest, PerformanceComplexPatterns) {
    const int num_iterations = 100;
    std::string complex_pattern = "*[a-z0-9]?[!.][a-z]*.txt";
    std::string long_string = generateRandomString(1000) + ".txt";
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_iterations; ++i) {
        ATOM_UNUSED_RESULT(fnmatch(complex_pattern, long_string));
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    spdlog::info("Performance for complex pattern ({} iterations): {} μs",
                 num_iterations, duration);
}

TEST_F(FnmatchTest, PerformanceMultipleFilters) {
    std::vector<std::string> large_dataset;
    large_dataset.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        if (i % 2 == 0)
            large_dataset.push_back("file" + std::to_string(i) + ".txt");
        else
            large_dataset.push_back("doc" + std::to_string(i) + ".pdf");
    }
    std::vector<std::string> patterns = {"*.txt", "file*0.pdf", "doc*9.pdf"};
    auto start = std::chrono::high_resolution_clock::now();
    auto matched = filter(large_dataset, patterns, 0, true);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    spdlog::info("Performance for filtering 10000 files with 3 patterns: {} ms",
                 duration);
    spdlog::info("Matched files: {}", matched.size());
}

TEST_F(FnmatchTest, ThreadSafety) {
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
    std::vector<bool> expected = {true, false, true, false,
                                  true, false, true, false};
    EXPECT_EQ(results, expected);
}

TEST_F(FnmatchTest, ThreadSafetyWithPatternCache) {
    const int num_threads = 10;
    std::string pattern = "file[0-9]*.txt";
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
    for (auto& future : futures)
        future.get();
    for (int i = 1; i < num_threads; ++i) {
        EXPECT_EQ(results[i], results[0]);
    }
    EXPECT_TRUE(results[0][0]);
    EXPECT_TRUE(results[0][1]);
    EXPECT_FALSE(results[0][2]);
    EXPECT_FALSE(results[0][3]);
}

#ifndef _WIN32
TEST_F(FnmatchTest, SystemFnmatchCompatibility) {
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));
    EXPECT_FALSE(fnmatch("*.txt", "file.jpg"));
    EXPECT_TRUE(fnmatch("file[1-9].txt", "file5.txt"));
    EXPECT_FALSE(fnmatch("file[1-9].txt", "fileA.txt"));
    EXPECT_TRUE(fnmatch("FILE.TXT", "file.txt", flags::CASEFOLD));
    EXPECT_TRUE(fnmatch("*", "anything"));
    EXPECT_TRUE(fnmatch("?", "a"));
    EXPECT_FALSE(fnmatch("?", "ab"));
}
#endif

TEST_F(FnmatchTest, FilesystemIntegration) {
    std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path() / "fnmatch_test";
    try {
        if (std::filesystem::exists(temp_dir))
            std::filesystem::remove_all(temp_dir);
        std::filesystem::create_directory(temp_dir);
        std::filesystem::create_directory(temp_dir / "subdir1");
        std::filesystem::create_directory(temp_dir / "subdir2");
        std::ofstream(temp_dir / "file1.txt").close();
        std::ofstream(temp_dir / "file2.txt").close();
        std::ofstream(temp_dir / "document.pdf").close();
        std::ofstream(temp_dir / "image.jpg").close();
        std::ofstream(temp_dir / "subdir1" / "nested.txt").close();
        std::ofstream(temp_dir / "subdir2" / "other.pdf").close();
        std::vector<std::string> all_files;
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(temp_dir)) {
            if (entry.is_regular_file())
                all_files.push_back(entry.path().filename().string());
        }
        std::vector<std::string> txt_files;
        for (const auto& file : all_files) {
            if (fnmatch("*.txt", file))
                txt_files.push_back(file);
        }
        ASSERT_EQ(txt_files.size(), 3);
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "file1.txt") != txt_files.end());
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "file2.txt") != txt_files.end());
        EXPECT_TRUE(std::find(txt_files.begin(), txt_files.end(),
                              "nested.txt") != txt_files.end());
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
        spdlog::error("Filesystem test exception: {}", e.what());
        if (std::filesystem::exists(temp_dir))
            std::filesystem::remove_all(temp_dir);
        FAIL() << "Filesystem test failed: " << e.what();
    }
    if (std::filesystem::exists(temp_dir))
        std::filesystem::remove_all(temp_dir);
}

TEST_F(FnmatchTest, ExplicitTemplateInstantiations) {
    std::string pattern_str = "*.txt";
    std::string text_str = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_str, text_str));
    EXPECT_TRUE(fnmatch("*.txt", "file.txt"));
    std::string_view pattern_view = "*.txt";
    std::string_view text_view = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_view, text_view));
    const char* pattern_cstr = "*.txt";
    const char* text_cstr = "file.txt";
    EXPECT_TRUE(fnmatch(pattern_cstr, text_cstr));
    EXPECT_TRUE(fnmatch(pattern_str, text_view));
    EXPECT_TRUE(fnmatch(pattern_view, text_cstr));
    EXPECT_TRUE(fnmatch(pattern_cstr, text_str));
}

#ifdef __SSE4_2__
TEST_F(FnmatchTest, SimdOptimizations) {
    std::string long_text(1000, 'a');
    std::string pattern = "*" + std::string(10, 'b') + "*";
    auto start = std::chrono::high_resolution_clock::now();
    bool result = fnmatch(pattern, long_text);
    auto end = std::chrono::high_resolution_clock::now();
    EXPECT_FALSE(result);
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    spdlog::info("SIMD acceleration test duration: {} μs", duration);
}
#endif
