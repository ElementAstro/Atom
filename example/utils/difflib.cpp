#include "atom/utils/difflib.hpp"

#include <iostream>
#include <vector>
#include <string>

using namespace atom::utils;

int main() {
    // Example strings for SequenceMatcher
    std::string str1 = "Hello, World!";
    std::string str2 = "Hello, C++ World!";

    // Create a SequenceMatcher instance
    SequenceMatcher matcher(str1, str2);

    // Get the similarity ratio between the two strings
    double similarityRatio = matcher.ratio();
    std::cout << "Similarity ratio: " << similarityRatio << std::endl;

    // Get the matching blocks between the two strings
    auto matchingBlocks = matcher.getMatchingBlocks();
    std::cout << "Matching blocks: ";
    for (const auto& block : matchingBlocks) {
        std::cout << "(" << std::get<0>(block) << ", " << std::get<1>(block) << ", " << std::get<2>(block) << ") ";
    }
    std::cout << std::endl;

    // Get the opcodes for the differences between the two strings
    auto opcodes = matcher.getOpcodes();
    std::cout << "Opcodes: ";
    for (const auto& opcode : opcodes) {
        std::cout << "(" << std::get<0>(opcode) << ", " << std::get<1>(opcode) << ", " << std::get<2>(opcode) << ", " << std::get<3>(opcode) << ", " << std::get<4>(opcode) << ") ";
    }
    std::cout << std::endl;

    // Example vectors for Differ
    std::vector<std::string> vec1 = {"line1", "line2", "line3"};
    std::vector<std::string> vec2 = {"line1", "line2 modified", "line3"};

    // Compare two vectors of strings
    auto diffResult = Differ::compare(vec1, vec2);
    std::cout << "Diff result: ";
    for (const auto& line : diffResult) {
        std::cout << line << " ";
    }
    std::cout << std::endl;

    // Generate a unified diff between two vectors of strings
    auto unifiedDiffResult = Differ::unifiedDiff(vec1, vec2, "original", "modified");
    std::cout << "Unified diff result: ";
    for (const auto& line : unifiedDiffResult) {
        std::cout << line << " ";
    }
    std::cout << std::endl;

    // Example vectors for HtmlDiff
    std::vector<std::string> fromlines = {"line1", "line2", "line3"};
    std::vector<std::string> tolines = {"line1", "line2 modified", "line3"};

    // Generate an HTML file showing the differences between two vectors of strings
    std::string htmlFile = HtmlDiff::makeFile(fromlines, tolines, "Original", "Modified");
    std::cout << "HTML file diff: " << htmlFile << std::endl;

    // Generate an HTML table showing the differences between two vectors of strings
    std::string htmlTable = HtmlDiff::makeTable(fromlines, tolines, "Original", "Modified");
    std::cout << "HTML table diff: " << htmlTable << std::endl;

    // Example word and possibilities for getCloseMatches
    std::string word = "hello";
    std::vector<std::string> possibilities = {"hallo", "hullo", "hell"};

    // Get close matches for a word from a list of possibilities
    auto closeMatches = getCloseMatches(word, possibilities);
    std::cout << "Close matches: ";
    for (const auto& match : closeMatches) {
        std::cout << match << " ";
    }
    std::cout << std::endl;

    return 0;
}