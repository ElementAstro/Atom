// filepath: /home/max/Atom-1/atom/utils/test_difflib.hpp
/*
 * test_difflib.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-18

Description: Tests for difflib utilities

**************************************************/

#ifndef ATOM_UTILS_TEST_DIFFLIB_HPP
#define ATOM_UTILS_TEST_DIFFLIB_HPP

#include <gtest/gtest.h>
#include <chrono>
#include "atom/utils/format/difflib.hpp"

namespace atom::utils::test {

class SequenceMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test strings
        str1 = "Hello World";
        str2 = "Hello Earth";
        str3 = "Completely Different";
        str4 = "";
        str5 = "A";

        // Common test vectors
        vec1 = {1, 2, 3, 4, 5};
        vec2 = {1, 2, 4, 5, 6};
        vec3 = {10, 20, 30};
        vec4 = {};
        vec5 = {1};
    }

    std::string str1, str2, str3, str4, str5;
    std::vector<int> vec1, vec2, vec3, vec4, vec5;
};

// Test basic sequence matching functionality
TEST_F(SequenceMatcherTest, BasicMatching) {
    SequenceMatcher<std::string> matcher(str1, str2);

    // Test similarity ratio
    double ratio = matcher.ratio();
    EXPECT_GT(ratio, 0.0);
    EXPECT_LE(ratio, 1.0);

    // Test that identical strings have ratio 1.0
    SequenceMatcher<std::string> identicalMatcher(str1, str1);
    EXPECT_DOUBLE_EQ(identicalMatcher.ratio(), 1.0);

    // Test that completely different strings have low ratio
    SequenceMatcher<std::string> differentMatcher(str1, str3);
    EXPECT_LT(differentMatcher.ratio(), 0.5);
}

TEST_F(SequenceMatcherTest, EmptyStrings) {
    // Test empty vs empty
    SequenceMatcher<std::string> emptyMatcher(str4, str4);
    EXPECT_DOUBLE_EQ(emptyMatcher.ratio(), 1.0);

    // Test empty vs non-empty
    SequenceMatcher<std::string> emptyVsNonEmpty(str4, str1);
    EXPECT_DOUBLE_EQ(emptyVsNonEmpty.ratio(), 0.0);

    // Test non-empty vs empty
    SequenceMatcher<std::string> nonEmptyVsEmpty(str1, str4);
    EXPECT_DOUBLE_EQ(nonEmptyVsEmpty.ratio(), 0.0);
}

TEST_F(SequenceMatcherTest, SingleCharacter) {
    // Test single character strings
    SequenceMatcher<std::string> singleMatcher(str5, str5);
    EXPECT_DOUBLE_EQ(singleMatcher.ratio(), 1.0);

    SequenceMatcher<std::string> singleVsDifferent(str5, "B");
    EXPECT_DOUBLE_EQ(singleVsDifferent.ratio(), 0.0);
}

TEST_F(SequenceMatcherTest, VectorMatching) {
    SequenceMatcher<std::vector<int>> vecMatcher(vec1, vec2);

    // Test similarity ratio for vectors
    double ratio = vecMatcher.ratio();
    EXPECT_GT(ratio, 0.0);
    EXPECT_LE(ratio, 1.0);

    // Test identical vectors
    SequenceMatcher<std::vector<int>> identicalVecMatcher(vec1, vec1);
    EXPECT_DOUBLE_EQ(identicalVecMatcher.ratio(), 1.0);

    // Test completely different vectors
    SequenceMatcher<std::vector<int>> differentVecMatcher(vec1, vec3);
    EXPECT_LT(differentVecMatcher.ratio(), 0.5);
}

TEST_F(SequenceMatcherTest, EmptyVectors) {
    // Test empty vs empty vectors
    SequenceMatcher<std::vector<int>> emptyVecMatcher(vec4, vec4);
    EXPECT_DOUBLE_EQ(emptyVecMatcher.ratio(), 1.0);

    // Test empty vs non-empty vectors
    SequenceMatcher<std::vector<int>> emptyVsNonEmptyVec(vec4, vec1);
    EXPECT_DOUBLE_EQ(emptyVsNonEmptyVec.ratio(), 0.0);
}

TEST_F(SequenceMatcherTest, SingleElementVector) {
    // Test single element vectors
    SequenceMatcher<std::vector<int>> singleVecMatcher(vec5, vec5);
    EXPECT_DOUBLE_EQ(singleVecMatcher.ratio(), 1.0);

    std::vector<int> differentSingle = {2};
    SequenceMatcher<std::vector<int>> singleVsDifferentVec(vec5,
                                                           differentSingle);
    EXPECT_DOUBLE_EQ(singleVsDifferentVec.ratio(), 0.0);
}

// Test diff generation functionality
class DiffTest : public ::testing::Test {
protected:
    void SetUp() override {
        lines1 = {"line1", "line2", "line3", "line4"};
        lines2 = {"line1", "modified_line2", "line3", "line5"};
        lines3 = {"completely", "different", "content"};
        emptyLines = {};
        singleLine = {"single"};
    }

    std::vector<std::string> lines1, lines2, lines3, emptyLines, singleLine;
};

TEST_F(DiffTest, BasicDiff) {
    auto diff = unifiedDiff(lines1, lines2, "file1", "file2");

    // Should contain diff markers
    bool hasMinusLine = false;
    bool hasPlusLine = false;

    for (const auto& line : diff) {
        if (line.starts_with("-"))
            hasMinusLine = true;
        if (line.starts_with("+"))
            hasPlusLine = true;
    }

    EXPECT_TRUE(hasMinusLine ||
                hasPlusLine);  // Should have at least one change
}

TEST_F(DiffTest, IdenticalFiles) {
    auto diff = unifiedDiff(lines1, lines1, "file1", "file2");

    // Should be empty or contain only context
    bool hasChanges = false;
    for (const auto& line : diff) {
        if (line.starts_with("-") || line.starts_with("+")) {
            hasChanges = true;
            break;
        }
    }

    EXPECT_FALSE(hasChanges);
}

TEST_F(DiffTest, EmptyFiles) {
    auto diff = unifiedDiff(emptyLines, emptyLines, "empty1", "empty2");

    // Should produce minimal diff output
    EXPECT_TRUE(diff.empty() || diff.size() <= 3);  // Header lines only
}

TEST_F(DiffTest, EmptyVsNonEmpty) {
    auto diff = unifiedDiff(emptyLines, lines1, "empty", "nonempty");

    // Should show all lines as additions
    int additionCount = 0;
    for (const auto& line : diff) {
        if (line.starts_with("+") && !line.starts_with("+++")) {
            additionCount++;
        }
    }

    EXPECT_EQ(additionCount, static_cast<int>(lines1.size()));
}

TEST_F(DiffTest, NonEmptyVsEmpty) {
    auto diff = unifiedDiff(lines1, emptyLines, "nonempty", "empty");

    // Should show all lines as deletions
    int deletionCount = 0;
    for (const auto& line : diff) {
        if (line.starts_with("-") && !line.starts_with("---")) {
            deletionCount++;
        }
    }

    EXPECT_EQ(deletionCount, static_cast<int>(lines1.size()));
}

// Test performance and edge cases
class DiffPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate large test data
        for (int i = 0; i < 1000; ++i) {
            largeLines1.push_back("Line " + std::to_string(i));
            if (i % 10 == 0) {
                largeLines2.push_back("Modified Line " + std::to_string(i));
            } else {
                largeLines2.push_back("Line " + std::to_string(i));
            }
        }
    }

    std::vector<std::string> largeLines1, largeLines2;
};

TEST_F(DiffPerformanceTest, LargeFileDiff) {
    auto start = std::chrono::high_resolution_clock::now();

    auto diff = unifiedDiff(largeLines1, largeLines2, "large1", "large2");

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 5000);  // 5 seconds max

    // Should produce reasonable diff output
    EXPECT_GT(diff.size(), 0);
}

// Test error handling and edge cases
class DiffErrorTest : public ::testing::Test {};

TEST_F(DiffErrorTest, NullPointerHandling) {
    // Test with valid inputs (null pointers not applicable for std::vector)
    std::vector<std::string> valid1 = {"test"};
    std::vector<std::string> valid2 = {"test2"};

    EXPECT_NO_THROW(
        { auto diff = unifiedDiff(valid1, valid2, "file1", "file2"); });
}

TEST_F(DiffErrorTest, VeryLongLines) {
    std::vector<std::string> longLines1 = {std::string(10000, 'a')};
    std::vector<std::string> longLines2 = {std::string(10000, 'b')};

    EXPECT_NO_THROW(
        { auto diff = unifiedDiff(longLines1, longLines2, "long1", "long2"); });
}

TEST_F(DiffErrorTest, SpecialCharacters) {
    std::vector<std::string> specialLines1 = {"line with\ttabs",
                                              "line with\nnewlines",
                                              "line with\rcarriage returns"};
    std::vector<std::string> specialLines2 = {"line with  spaces",
                                              "line with\nnewlines",
                                              "line with\rcarriage returns"};

    EXPECT_NO_THROW({
        auto diff =
            unifiedDiff(specialLines1, specialLines2, "special1", "special2");
    });
}

// Test context diff functionality
class ContextDiffTest : public ::testing::Test {
protected:
    void SetUp() override {
        contextLines1 = {"context line 1", "old line", "context line 2",
                         "another old line", "context line 3"};
        contextLines2 = {"context line 1", "new line", "context line 2",
                         "another new line", "context line 3"};
    }

    std::vector<std::string> contextLines1, contextLines2;
};

TEST_F(ContextDiffTest, BasicContextDiff) {
    auto diff =
        contextDiff(contextLines1, contextLines2, "context1", "context2");

    // Should contain context markers
    bool hasContext = false;
    for (const auto& line : diff) {
        if (line.starts_with("***") || line.starts_with("---")) {
            hasContext = true;
            break;
        }
    }

    EXPECT_TRUE(hasContext);
}

// Test Differ class functionality
class DifferTest : public ::testing::Test {
protected:
    void SetUp() override {
        differ1 = {"line1", "line2", "line3"};
        differ2 = {"line1", "modified_line2", "line3", "line4"};
        differ3 = {};
        differ4 = {"single_line"};
    }

    std::vector<std::string> differ1, differ2, differ3, differ4;
};

TEST_F(DifferTest, BasicCompare) {
    auto result = Differ::compare(differ1, differ2);

    // Should contain difference markers
    bool hasChanges = false;
    for (const auto& line : result) {
        if (line.starts_with("- ") || line.starts_with("+ ") ||
            line.starts_with("? ")) {
            hasChanges = true;
            break;
        }
    }

    EXPECT_TRUE(hasChanges);
}

TEST_F(DifferTest, IdenticalSequences) {
    auto result = Differ::compare(differ1, differ1);

    // Should show no changes (only context lines)
    bool hasChanges = false;
    for (const auto& line : result) {
        if (line.starts_with("- ") || line.starts_with("+ ")) {
            hasChanges = true;
            break;
        }
    }

    EXPECT_FALSE(hasChanges);
}

TEST_F(DifferTest, EmptySequences) {
    auto result = Differ::compare(differ3, differ3);
    EXPECT_TRUE(result.empty());

    // Empty vs non-empty
    auto result2 = Differ::compare(differ3, differ1);
    EXPECT_FALSE(result2.empty());
}

// Test HtmlDiff functionality
class HtmlDiffTest : public ::testing::Test {
protected:
    void SetUp() override {
        htmlLines1 = {"<html>", "<body>", "<p>Hello</p>", "</body>", "</html>"};
        htmlLines2 = {"<html>", "<body>", "<p>Hi there</p>", "</body>",
                      "</html>"};
    }

    std::vector<std::string> htmlLines1, htmlLines2;
};

TEST_F(HtmlDiffTest, BasicHtmlDiff) {
    auto result =
        HtmlDiff::makeFile(htmlLines1, htmlLines2, "Original", "Modified");

    EXPECT_TRUE(result.has_value());

    std::string html = result.value();
    EXPECT_FALSE(html.empty());

    // Should contain HTML structure
    EXPECT_TRUE(html.find("<table") != std::string::npos);
    EXPECT_TRUE(html.find("Original") != std::string::npos);
    EXPECT_TRUE(html.find("Modified") != std::string::npos);
}

TEST_F(HtmlDiffTest, HtmlEscaping) {
    std::vector<std::string> specialChars1 = {"<script>alert('test')</script>"};
    std::vector<std::string> specialChars2 = {
        "<script>alert('modified')</script>"};

    auto result =
        HtmlDiff::makeFile(specialChars1, specialChars2, "Test1", "Test2");

    EXPECT_TRUE(result.has_value());

    std::string html = result.value();
    // Should escape HTML special characters
    EXPECT_TRUE(html.find("&lt;script&gt;") != std::string::npos);
}

// Test getCloseMatches functionality
class CloseMatchesTest : public ::testing::Test {
protected:
    void SetUp() override {
        possibilities = {"apple",   "ample",  "apply",
                         "apricot", "banana", "grape"};
    }

    std::vector<std::string> possibilities;
};

TEST_F(CloseMatchesTest, BasicMatching) {
    auto matches = getCloseMatches("appel", possibilities);

    EXPECT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "apple");  // Should be the closest match
}

TEST_F(CloseMatchesTest, ExactMatch) {
    auto matches = getCloseMatches("apple", possibilities);

    EXPECT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "apple");  // Exact match should be first
}

TEST_F(CloseMatchesTest, NoMatches) {
    auto matches = getCloseMatches("xyz", possibilities, 3, 0.8);

    EXPECT_TRUE(matches.empty());  // No close matches with high cutoff
}

TEST_F(CloseMatchesTest, LimitResults) {
    auto matches = getCloseMatches("app", possibilities, 2);

    EXPECT_LE(matches.size(), 2);  // Should respect the limit
}

TEST_F(CloseMatchesTest, InvalidParameters) {
    // Test negative n parameter
    EXPECT_THROW(getCloseMatches("apple", possibilities, -1),
                 std::invalid_argument);

    // Test cutoff boundaries
    EXPECT_THROW(getCloseMatches("apple", possibilities, 3, -0.1),
                 std::invalid_argument);
    EXPECT_THROW(getCloseMatches("apple", possibilities, 3, 1.1),
                 std::invalid_argument);

    // Test with high cutoff
    auto matches2 = getCloseMatches("appel", possibilities, 3, 0.9);
    EXPECT_TRUE(matches2.empty());  // Should be too high to match
}

// Test diff statistics
class DiffStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        statsLines1 = {"line1", "line2", "line3"};
        statsLines2 = {"line1", "modified_line2", "line3", "line4"};
    }

    std::vector<std::string> statsLines1, statsLines2;
};

TEST_F(DiffStatsTest, BasicStats) {
    DiffStats stats;

    // Test default initialization
    EXPECT_EQ(stats.insertions, 0);
    EXPECT_EQ(stats.deletions, 0);
    EXPECT_EQ(stats.modifications, 0);
    EXPECT_DOUBLE_EQ(stats.similarity, 0.0);

    // Test toString method
    std::string statsStr = stats.toString();
    EXPECT_FALSE(statsStr.empty());
    EXPECT_TRUE(statsStr.find("insertions") != std::string::npos);
    EXPECT_TRUE(statsStr.find("deletions") != std::string::npos);
}

// Test diff options
class DiffOptionsTest : public ::testing::Test {};

TEST_F(DiffOptionsTest, DefaultOptions) {
    DiffOptions options;

    // Test default values
    EXPECT_TRUE(options.enableCaching);
    EXPECT_TRUE(options.useParallelProcessing);
    EXPECT_FALSE(options.lazyLoading);
    EXPECT_EQ(options.cacheSizeLimit, 100);
    EXPECT_EQ(options.largeFileThreshold, 1024 * 1024);
    EXPECT_EQ(options.algorithm, DiffAlgorithm::Default);
    EXPECT_EQ(options.logger, nullptr);
}

TEST_F(DiffOptionsTest, CustomOptions) {
    DiffOptions options;
    options.enableCaching = false;
    options.useParallelProcessing = false;
    options.lazyLoading = true;
    options.cacheSizeLimit = 50;
    options.algorithm = DiffAlgorithm::Myers;

    // Test custom values
    EXPECT_FALSE(options.enableCaching);
    EXPECT_FALSE(options.useParallelProcessing);
    EXPECT_TRUE(options.lazyLoading);
    EXPECT_EQ(options.cacheSizeLimit, 50);
    EXPECT_EQ(options.algorithm, DiffAlgorithm::Myers);
}

// Test different diff algorithms
class DiffAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        algoLines1 = {"a", "b", "c", "d", "e"};
        algoLines2 = {"a", "x", "c", "y", "e"};
    }

    std::vector<std::string> algoLines1, algoLines2;
};

TEST_F(DiffAlgorithmTest, DefaultAlgorithm) {
    DiffOptions options;
    options.algorithm = DiffAlgorithm::Default;

    EXPECT_NO_THROW(
        { auto diff = unifiedDiff(algoLines1, algoLines2, "algo1", "algo2"); });
}

TEST_F(DiffAlgorithmTest, MyersAlgorithm) {
    DiffOptions options;
    options.algorithm = DiffAlgorithm::Myers;

    EXPECT_NO_THROW({
        auto diff = unifiedDiff(algoLines1, algoLines2, "myers1", "myers2");
    });
}

// Test thread safety
class DiffThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        threadLines1 = {"thread", "safety", "test", "lines"};
        threadLines2 = {"thread", "safe", "test", "content"};
    }

    std::vector<std::string> threadLines1, threadLines2;
};

TEST_F(DiffThreadSafetyTest, ConcurrentDiffs) {
    const int numThreads = 4;
    std::vector<std::future<std::vector<std::string>>> futures;

    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            return unifiedDiff(threadLines1, threadLines2,
                               "thread" + std::to_string(i) + "_1",
                               "thread" + std::to_string(i) + "_2");
        }));
    }

    // Wait for all threads to complete
    for (auto& future : futures) {
        EXPECT_NO_THROW({
            auto result = future.get();
            EXPECT_GT(result.size(), 0);
        });
    }
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_DIFFLIB_HPP
