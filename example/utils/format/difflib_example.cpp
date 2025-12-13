/**
 * @file difflib_example.cpp
 * @brief Examples for atom::utils difflib utilities
 */

#include "atom/utils/format/difflib.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateSequenceMatcher() {
    printSection("1. SequenceMatcher Basic Usage");

    std::string str1 = "Hello World";
    std::string str2 = "Hello World!";

    std::cout << "String 1: \"" << str1 << "\"" << std::endl;
    std::cout << "String 2: \"" << str2 << "\"" << std::endl;

    SequenceMatcher matcher(str1, str2);

    double ratio = matcher.ratio();
    std::cout << "\nSimilarity ratio: " << (ratio * 100) << "%" << std::endl;
}

void demonstrateSimilarityRatio() {
    printSection("2. Similarity Ratio Comparison");

    std::vector<std::pair<std::string, std::string>> pairs = {
        {"hello", "hello"},
        {"hello", "hallo"},
        {"hello", "world"},
        {"programming", "programmer"},
        {"algorithm", "logarithm"},
        {"", "test"},
        {"same", "same"}
    };

    std::cout << "Comparing string pairs:" << std::endl;
    for (const auto& [s1, s2] : pairs) {
        SequenceMatcher matcher(s1, s2);
        double ratio = matcher.ratio();
        std::cout << "  \"" << s1 << "\" vs \"" << s2 << "\": "
                  << static_cast<int>(ratio * 100) << "%" << std::endl;
    }
}

void demonstrateMatchingBlocks() {
    printSection("3. Matching Blocks");

    std::string str1 = "abcdefg";
    std::string str2 = "abcXdefYg";

    std::cout << "String 1: \"" << str1 << "\"" << std::endl;
    std::cout << "String 2: \"" << str2 << "\"" << std::endl;

    SequenceMatcher matcher(str1, str2);
    auto blocks = matcher.getMatchingBlocks();

    std::cout << "\nMatching blocks:" << std::endl;
    for (const auto& block : blocks) {
        std::cout << "  Position (" << block.a << ", " << block.b
                  << "), length: " << block.size << std::endl;
    }
}

void demonstrateOpcodes() {
    printSection("4. Edit Operations (Opcodes)");

    std::string str1 = "Hello World";
    std::string str2 = "Hello There";

    std::cout << "From: \"" << str1 << "\"" << std::endl;
    std::cout << "To:   \"" << str2 << "\"" << std::endl;

    SequenceMatcher matcher(str1, str2);
    auto opcodes = matcher.getOpcodes();

    std::cout << "\nEdit operations:" << std::endl;
    for (const auto& op : opcodes) {
        std::cout << "  " << op.tag << ": ";
        std::cout << "a[" << op.i1 << ":" << op.i2 << "] -> ";
        std::cout << "b[" << op.j1 << ":" << op.j2 << "]" << std::endl;
    }
}

void demonstrateDiffStats() {
    printSection("5. Diff Statistics");

    std::string original = "The quick brown fox jumps over the lazy dog";
    std::string modified = "The quick red fox leaps over the lazy cat";

    std::cout << "Original: \"" << original << "\"" << std::endl;
    std::cout << "Modified: \"" << modified << "\"" << std::endl;

    SequenceMatcher matcher(original, modified);
    auto stats = matcher.getStats();

    std::cout << "\nDiff Statistics:" << std::endl;
    std::cout << "  Insertions: " << stats.insertions << std::endl;
    std::cout << "  Deletions: " << stats.deletions << std::endl;
    std::cout << "  Modifications: " << stats.modifications << std::endl;
    std::cout << "  Similarity: " << (stats.similarity * 100) << "%" << std::endl;
}

void demonstrateDiffAlgorithms() {
    printSection("6. Different Diff Algorithms");

    std::string str1 = "ABCDEFGHIJ";
    std::string str2 = "ABXDEFYHIJ";

    std::cout << "String 1: \"" << str1 << "\"" << std::endl;
    std::cout << "String 2: \"" << str2 << "\"" << std::endl;

    DiffOptions defaultOpts;
    defaultOpts.algorithm = DiffAlgorithm::Default;
    SequenceMatcher matcher1(str1, str2, defaultOpts);
    std::cout << "\nDefault algorithm ratio: " << (matcher1.ratio() * 100) << "%" << std::endl;

    DiffOptions myersOpts;
    myersOpts.algorithm = DiffAlgorithm::Myers;
    SequenceMatcher matcher2(str1, str2, myersOpts);
    std::cout << "Myers algorithm ratio: " << (matcher2.ratio() * 100) << "%" << std::endl;
}

void demonstrateTextDiff() {
    printSection("7. Text File Diff Example");

    std::vector<std::string> file1 = {
        "line 1: hello",
        "line 2: world",
        "line 3: foo",
        "line 4: bar"
    };

    std::vector<std::string> file2 = {
        "line 1: hello",
        "line 2: universe",
        "line 3: foo",
        "line 4: baz",
        "line 5: new line"
    };

    std::cout << "File 1 contents:" << std::endl;
    for (const auto& line : file1) {
        std::cout << "  " << line << std::endl;
    }

    std::cout << "\nFile 2 contents:" << std::endl;
    for (const auto& line : file2) {
        std::cout << "  " << line << std::endl;
    }

    std::cout << "\nLine-by-line comparison:" << std::endl;
    size_t maxLines = std::max(file1.size(), file2.size());
    for (size_t i = 0; i < maxLines; ++i) {
        std::string l1 = i < file1.size() ? file1[i] : "(missing)";
        std::string l2 = i < file2.size() ? file2[i] : "(missing)";

        if (l1 == l2) {
            std::cout << "  [=] " << l1 << std::endl;
        } else {
            std::cout << "  [-] " << l1 << std::endl;
            std::cout << "  [+] " << l2 << std::endl;
        }
    }
}

void demonstrateFuzzyMatching() {
    printSection("8. Fuzzy String Matching");

    std::string query = "progamming";
    std::vector<std::string> candidates = {
        "programming", "program", "programmer", "processing",
        "profiling", "prompting", "printing"
    };

    std::cout << "Query: \"" << query << "\"" << std::endl;
    std::cout << "\nFinding best matches:" << std::endl;

    std::vector<std::pair<std::string, double>> scores;
    for (const auto& candidate : candidates) {
        SequenceMatcher matcher(query, candidate);
        scores.emplace_back(candidate, matcher.ratio());
    }

    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (const auto& [word, score] : scores) {
        std::cout << "  \"" << word << "\": " << static_cast<int>(score * 100) << "%" << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Difflib Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateSequenceMatcher();
        demonstrateSimilarityRatio();
        demonstrateMatchingBlocks();
        demonstrateOpcodes();
        demonstrateDiffStats();
        demonstrateDiffAlgorithms();
        demonstrateTextDiff();
        demonstrateFuzzyMatching();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All difflib examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
