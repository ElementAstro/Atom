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
#include "atom/utils/difflib.hpp"

namespace atom::utils::test {

class SequenceMatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common test strings
        str1 = "Hello World";
        str2 = "Hello Earth";
        str3 = "";
        str4 = "Completely different text";
        identical_strings = "Identical";
    }

    std::string str1;
    std::string str2;
    std::string str3;
    std::string str4;
    std::string identical_strings;
};

TEST_F(SequenceMatcherTest, Construction) {
    // Test valid construction
    EXPECT_NO_THROW(SequenceMatcher matcher(str1, str2));

    // Test construction with empty strings
    EXPECT_NO_THROW(SequenceMatcher matcher(str3, str3));

    // Test construction with one empty string
    EXPECT_NO_THROW(SequenceMatcher matcher(str1, str3));
}

TEST_F(SequenceMatcherTest, SetSeqs) {
    SequenceMatcher matcher(str1, str2);

    // Change sequences
    EXPECT_NO_THROW(matcher.setSeqs(str3, str4));
    EXPECT_NO_THROW(matcher.setSeqs(str4, str1));
    EXPECT_NO_THROW(matcher.setSeqs(identical_strings, identical_strings));
}

TEST_F(SequenceMatcherTest, Ratio) {
    // Test identical strings
    SequenceMatcher identicalMatcher(identical_strings, identical_strings);
    EXPECT_DOUBLE_EQ(identicalMatcher.ratio(), 1.0);

    // Test completely different strings
    SequenceMatcher differentMatcher(str1, str4);
    EXPECT_LT(differentMatcher.ratio(), 0.3);  // Should be low similarity

    // Test similar strings
    SequenceMatcher similarMatcher(str1, str2);
    double ratio = similarMatcher.ratio();
    EXPECT_GT(ratio, 0.5);  // Should have moderate similarity
    EXPECT_LT(ratio, 1.0);  // But not identical

    // Test with empty strings
    SequenceMatcher emptyMatcher(str3, str3);
    EXPECT_DOUBLE_EQ(emptyMatcher.ratio(),
                     1.0);  // Empty strings are considered identical

    // Test one empty string
    SequenceMatcher oneEmptyMatcher(str1, str3);
    EXPECT_DOUBLE_EQ(oneEmptyMatcher.ratio(),
                     0.0);  // No similarity with empty string
}

TEST_F(SequenceMatcherTest, GetMatchingBlocks) {
    SequenceMatcher matcher(str1, str2);
    auto blocks = matcher.getMatchingBlocks();

    // Check we have some matching blocks
    EXPECT_FALSE(blocks.empty());

    // The first block should match "Hello "
    ASSERT_GE(blocks.size(), 1);
    EXPECT_EQ(std::get<0>(blocks[0]), 0);  // Start in str1
    EXPECT_EQ(std::get<1>(blocks[0]), 0);  // Start in str2
    EXPECT_EQ(std::get<2>(blocks[0]), 6);  // Length of "Hello "

    // Identical strings should have one block with full length
    SequenceMatcher identicalMatcher(identical_strings, identical_strings);
    auto identicalBlocks = identicalMatcher.getMatchingBlocks();
    ASSERT_GE(identicalBlocks.size(), 1);
    EXPECT_EQ(std::get<0>(identicalBlocks[0]), 0);
    EXPECT_EQ(std::get<1>(identicalBlocks[0]), 0);
    EXPECT_EQ(std::get<2>(identicalBlocks[0]), identical_strings.length());
}

TEST_F(SequenceMatcherTest, GetOpcodes) {
    SequenceMatcher matcher(str1, str2);
    auto opcodes = matcher.getOpcodes();

    // Check we have some opcodes
    EXPECT_FALSE(opcodes.empty());

    // First opcode should be "equal" for "Hello " (6 chars)
    ASSERT_GE(opcodes.size(), 1);
    EXPECT_EQ(std::get<0>(opcodes[0]), "equal");
    EXPECT_EQ(std::get<1>(opcodes[0]), 0);  // Start in str1
    EXPECT_EQ(std::get<2>(opcodes[0]), 6);  // End in str1
    EXPECT_EQ(std::get<3>(opcodes[0]), 0);  // Start in str2
    EXPECT_EQ(std::get<4>(opcodes[0]), 6);  // End in str2

    // Should be a replace for "World" -> "Earth"
    ASSERT_GE(opcodes.size(), 2);
    EXPECT_EQ(std::get<0>(opcodes[1]), "replace");

    // Identical strings should have just one "equal" opcode
    SequenceMatcher identicalMatcher(identical_strings, identical_strings);
    auto identicalOpcodes = identicalMatcher.getOpcodes();
    ASSERT_EQ(identicalOpcodes.size(), 1);
    EXPECT_EQ(std::get<0>(identicalOpcodes[0]), "equal");
    EXPECT_EQ(std::get<1>(identicalOpcodes[0]), 0);
    EXPECT_EQ(std::get<2>(identicalOpcodes[0]), identical_strings.length());
    EXPECT_EQ(std::get<3>(identicalOpcodes[0]), 0);
    EXPECT_EQ(std::get<4>(identicalOpcodes[0]), identical_strings.length());
}

class DifferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        text1 = {"line1", "line2", "line3", "line4", "line5"};
        text2 = {"line1", "line2 modified", "line3", "new line", "line5"};
        empty = {};
    }

    std::vector<std::string> text1;
    std::vector<std::string> text2;
    std::vector<std::string> empty;
};

TEST_F(DifferTest, Compare) {
    auto result = Differ::compare(text1, text2);

    // Check we have some results
    EXPECT_FALSE(result.empty());

    // First line should be equal
    EXPECT_EQ(result[0], "  line1");

    // Second line should be different
    EXPECT_EQ(result[1], "- line2");
    EXPECT_EQ(result[2], "+ line2 modified");

    // Test with empty inputs
    auto emptyResult = Differ::compare(empty, empty);
    EXPECT_TRUE(emptyResult.empty());

    // Test with one empty input
    auto oneEmptyResult = Differ::compare(text1, empty);
    EXPECT_FALSE(oneEmptyResult.empty());
    for (const auto& line : oneEmptyResult) {
        EXPECT_EQ(line[0], '-');  // All lines should be deletions
    }
}

TEST_F(DifferTest, UnifiedDiff) {
    auto diff = Differ::compare(text1, text2);
    auto unifiedDiff = Differ::unifiedDiff(text1, text2, "file1", "file2");

    // Check for header lines
    ASSERT_GE(unifiedDiff.size(), 2);
    EXPECT_EQ(unifiedDiff[0], "--- file1");
    EXPECT_EQ(unifiedDiff[1], "+++ file2");

    // Check for hunk header
    bool foundHunkHeader = false;
    for (const auto& line : unifiedDiff) {
        if (line.find("@@") == 0) {
            foundHunkHeader = true;
            break;
        }
    }
    EXPECT_TRUE(foundHunkHeader);

    // Test invalid context value
    EXPECT_THROW(Differ::unifiedDiff(text1, text2, "file1", "file2", -1),
                 std::invalid_argument);
}

class HtmlDiffTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        text1 = {"line1", "line2", "line3", "line4", "line5"};
        text2 = {"line1", "line2 modified", "line3", "new line", "line5"};
        empty = {};
    }

    std::vector<std::string> text1;
    std::vector<std::string> text2;
    std::vector<std::string> empty;
};

TEST_F(HtmlDiffTest, MakeTable) {
    auto result = HtmlDiff::makeTable(text1, text2, "Original", "Modified");

    // Check result is valid
    EXPECT_TRUE(result.has_value());

    // Check table contains headers
    std::string table = result.value();
    EXPECT_TRUE(table.find("<table>") != std::string::npos);
    EXPECT_TRUE(table.find("Original") != std::string::npos);
    EXPECT_TRUE(table.find("Modified") != std::string::npos);

    // Check for HTML-escaped characters
    std::vector<std::string> v1 = {"<script>alert('xss');</script>"};
    std::vector<std::string> v2 = {"<b>Bold</b>"};
    auto escapeResult = HtmlDiff::makeTable(v1, v2);
    EXPECT_TRUE(escapeResult.has_value());
    EXPECT_TRUE(escapeResult.value().find("&lt;script&gt;") !=
                std::string::npos);
}

TEST_F(HtmlDiffTest, MakeFile) {
    auto result = HtmlDiff::makeFile(text1, text2, "Original", "Modified");

    // Check result is valid
    EXPECT_TRUE(result.has_value());

    // Check HTML structure
    std::string html = result.value();
    EXPECT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
    EXPECT_TRUE(html.find("<html>") != std::string::npos);
    EXPECT_TRUE(html.find("<head>") != std::string::npos);
    EXPECT_TRUE(html.find("<body>") != std::string::npos);
    EXPECT_TRUE(html.find("<table>") != std::string::npos);

    // Check for styling
    EXPECT_TRUE(html.find("diff-add") != std::string::npos);
    EXPECT_TRUE(html.find("diff-remove") != std::string::npos);

    // Test with empty inputs
    auto emptyResult = HtmlDiff::makeFile(empty, empty);
    EXPECT_TRUE(emptyResult.has_value());
}

class CloseMatchesTest : public ::testing::Test {
protected:
    void SetUp() override {
        possibilities = {"apple",   "banana",  "orange", "pear",
                         "apricot", "avocado", "grape"};
    }

    std::vector<std::string> possibilities;
};

TEST_F(CloseMatchesTest, BasicMatching) {
    // Test exact match
    auto matches1 = getCloseMatches("apple", possibilities);
    ASSERT_FALSE(matches1.empty());
    EXPECT_EQ(matches1[0], "apple");

    // Test close match
    auto matches2 = getCloseMatches("appel", possibilities);
    ASSERT_FALSE(matches2.empty());
    EXPECT_EQ(matches2[0], "apple");

    // Test with no good matches
    auto matches3 = getCloseMatches("xyzabc", possibilities, 3, 0.6);
    EXPECT_TRUE(matches3.empty());

    // Test with lower cutoff to get more matches
    auto matches4 = getCloseMatches("aple", possibilities, 3, 0.5);
    ASSERT_FALSE(matches4.empty());
}

TEST_F(CloseMatchesTest, Parameters) {
    // Test n parameter
    auto matches1 = getCloseMatches("a", possibilities, 2);
    EXPECT_LE(matches1.size(), 2);

    // Test invalid n
    EXPECT_THROW(getCloseMatches("apple", possibilities, 0),
                 std::invalid_argument);
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

TEST_F(CloseMatchesTest, EdgeCases) {
    // Empty word
    auto matches1 = getCloseMatches("", possibilities);
    EXPECT_TRUE(matches1.empty());

    // Empty possibilities
    std::vector<std::string> empty;
    auto matches2 = getCloseMatches("apple", empty);
    EXPECT_TRUE(matches2.empty());

    // Match empty string with empty string in possibilities
    std::vector<std::string> withEmpty = {"", "something"};
    auto matches3 = getCloseMatches("", withEmpty);
    ASSERT_EQ(matches3.size(), 1);
    EXPECT_EQ(matches3[0], "");

    // Very long string
    std::string longString(1000, 'a');
    std::vector<std::string> possibilities = {"a", "aa", "aaa"};
    auto matches4 = getCloseMatches(longString, possibilities);
    EXPECT_FALSE(matches4.empty());
}

// Performance test for large inputs
TEST_F(CloseMatchesTest, DISABLED_LargeInputPerformance) {
    // Note: This test is disabled by default due to potential long running time
    std::vector<std::string> largePossibilities;
    largePossibilities.reserve(10000);

    for (int i = 0; i < 10000; ++i) {
        std::string word = "word" + std::to_string(i);
        largePossibilities.push_back(word);
    }

    // Time the execution
    auto start = std::chrono::high_resolution_clock::now();
    auto matches = getCloseMatches("word5000", largePossibilities);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Time taken for large input: " << elapsed.count() << " ms\n";

    ASSERT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "word5000");
}

// System test - end-to-end workflow using multiple components
TEST_F(DifferTest, EndToEndWorkflow) {
    // Create input files
    std::vector<std::string> file1 = {"This is line 1", "This is line 2",
                                      "Common line", "Last line"};
    std::vector<std::string> file2 = {"This is line 1", "Modified line 2",
                                      "Common line", "New last line"};

    // Get differences
    auto diff = Differ::compare(file1, file2);

    // Check difference output
    EXPECT_FALSE(diff.empty());

    // Generate unified diff
    auto unifiedDiff =
        Differ::unifiedDiff(file1, file2, "file1.txt", "file2.txt");
    EXPECT_FALSE(unifiedDiff.empty());

    // Generate HTML representation
    auto htmlDiff = HtmlDiff::makeFile(file1, file2, "Original Version",
                                       "Modified Version");
    EXPECT_TRUE(htmlDiff.has_value());

    // Check if the HTML contains our line content
    std::string html = htmlDiff.value();
    EXPECT_TRUE(html.find("This is line 1") != std::string::npos);
    EXPECT_TRUE(html.find("Modified line 2") != std::string::npos);

    // Find close matches to a specific line
    std::vector<std::string> allLines;
    allLines.insert(allLines.end(), file1.begin(), file1.end());
    allLines.insert(allLines.end(), file2.begin(), file2.end());

    auto matches = getCloseMatches("Modified lin 2", allLines);
    EXPECT_FALSE(matches.empty());
    EXPECT_EQ(matches[0], "Modified line 2");
}

// Test Sequence concept compliance
struct CustomSequence {
    using value_type = int;
    using reference = int&;
    using const_reference = const int&;
    using size_type = std::size_t;
    using iterator = std::vector<int>::iterator;
    using const_iterator = std::vector<int>::const_iterator;

    std::vector<int> data;

    bool operator==(const CustomSequence& other) const {
        return data == other.data;
    }

    iterator begin() { return data.begin(); }
    iterator end() { return data.end(); }
    const_iterator begin() const { return data.begin(); }
    const_iterator end() const { return data.end(); }
    size_type size() const { return data.size(); }
};

TEST(SequenceConceptTest, Compliance) {
    // Test that std::vector complies with Sequence concept
    static_assert(Sequence<std::vector<int>>);

    // Test that std::string complies with Sequence concept
    static_assert(Sequence<std::string>);

    // Test that CustomSequence complies with Sequence concept
    static_assert(Sequence<CustomSequence>);

    // Test a type that doesn't comply
    struct NonSequence {
        int value;
    };
    static_assert(!Sequence<NonSequence>);
}

// Helper test function for debugging matching blocks
inline void printMatchingBlocks(
    const std::vector<std::tuple<int, int, int>>& blocks) {
    std::cout << "Matching blocks:\n";
    for (const auto& block : blocks) {
        std::cout << "  a[" << std::get<0>(block) << ":"
                  << (std::get<0>(block) + std::get<2>(block)) << "] == b["
                  << std::get<1>(block) << ":"
                  << (std::get<1>(block) + std::get<2>(block)) << "] (length "
                  << std::get<2>(block) << ")\n";
    }
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_DIFFLIB_HPP
