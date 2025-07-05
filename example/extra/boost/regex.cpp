#include "atom/extra/boost/regex.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

using namespace atom::extra::boost;

int main() {
    // Create a RegexWrapper instance with a specific pattern
    RegexWrapper regexWrapper(R"(\d+)");

    // Match a string against the regex pattern
    std::string str = "12345";
    bool isMatch = regexWrapper.match(str);
    std::cout << "Match: " << std::boolalpha << isMatch << std::endl;

    // Search for the first match in a string
    std::string searchStr = "abc 123 def 456";
    std::optional<std::string> firstMatch = regexWrapper.search(searchStr);
    if (firstMatch) {
        std::cout << "First match: " << *firstMatch << std::endl;
    } else {
        std::cout << "No match found" << std::endl;
    }

    // Search for all matches in a string
    std::vector<std::string> allMatches = regexWrapper.searchAll(searchStr);
    std::cout << "All matches: ";
    for (const auto& match : allMatches) {
        std::cout << match << " ";
    }
    std::cout << std::endl;

    // Replace all matches in a string
    std::string replaceStr = "abc 123 def 456";
    std::string replacedStr = regexWrapper.replace(replaceStr, "number");
    std::cout << "Replaced string: " << replacedStr << std::endl;

    // Split a string by the regex pattern
    std::string splitStr = "abc 123 def 456";
    std::vector<std::string> splitParts = regexWrapper.split(splitStr);
    std::cout << "Split parts: ";
    for (const auto& part : splitParts) {
        std::cout << part << " ";
    }
    std::cout << std::endl;

    // Match groups in a string
    RegexWrapper groupRegexWrapper(R"((\d+)-(\d+))");
    std::string groupStr = "123-456";
    auto matchGroups = groupRegexWrapper.matchGroups(groupStr);
    std::cout << "Match groups: ";
    for (const auto& [fullMatch, groups] : matchGroups) {
        std::cout << "Full match: " << fullMatch << ", Groups: ";
        for (const auto& group : groups) {
            std::cout << group << " ";
        }
        std::cout << std::endl;
    }

    // Apply a function to each match
    regexWrapper.forEachMatch(searchStr, [](const boost::smatch& match) {
        std::cout << "Match found: " << match.str() << std::endl;
    });

    // Get the regex pattern
    std::string pattern = regexWrapper.getPattern();
    std::cout << "Regex pattern: " << pattern << std::endl;

    // Set a new regex pattern
    regexWrapper.setPattern(R"(\w+)");
    std::cout << "New regex pattern: " << regexWrapper.getPattern()
              << std::endl;

    // Get named captures from a match
    RegexWrapper namedCaptureRegexWrapper(R"((?<first>\d+)-(?<second>\d+))");
    std::string namedCaptureStr = "123-456";
    auto namedCaptures =
        namedCaptureRegexWrapper.namedCaptures(namedCaptureStr);
    std::cout << "Named captures: ";
    for (const auto& [name, value] : namedCaptures) {
        std::cout << name << ": " << value << " ";
    }
    std::cout << std::endl;

    // Check if a string is a valid match
    bool isValid = regexWrapper.isValid(searchStr);
    std::cout << "Is valid match: " << std::boolalpha << isValid << std::endl;

    // Replace matches using a callback function
    auto callback = [](const boost::smatch& match) { return "number"; };
    std::string callbackReplacedStr =
        regexWrapper.replaceCallback(replaceStr, callback);
    std::cout << "Callback replaced string: " << callbackReplacedStr
              << std::endl;

    // Escape special characters in a string for regex
    std::string specialStr = R"([.*+?^${}()|[\]\\])";
    std::string escapedStr = RegexWrapper::escapeString(specialStr);
    std::cout << "Escaped string: " << escapedStr << std::endl;

    // Benchmark the match operation
    auto benchmarkTime = regexWrapper.benchmarkMatch(searchStr, 1000);
    std::cout << "Benchmark time: " << benchmarkTime.count() << " ns"
              << std::endl;

    // Check if a regex pattern is valid
    bool isValidPattern = RegexWrapper::isValidRegex(R"(\d+)");
    std::cout << "Is valid regex pattern: " << std::boolalpha << isValidPattern
              << std::endl;

    return 0;
}
