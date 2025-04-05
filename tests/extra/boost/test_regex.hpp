#ifndef ATOM_EXTRA_BOOST_TEST_REGEX_HPP
#define ATOM_EXTRA_BOOST_TEST_REGEX_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "atom/extra/boost/regex.hpp"

namespace atom::extra::boost::test {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;

class RegexWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup basic regex wrapper for testing
        simplePattern = std::make_unique<RegexWrapper>("\\w+");
        emailPattern = std::make_unique<RegexWrapper>(
            "([a-zA-Z0-9._%-]+)@([a-zA-Z0-9.-]+)\\.([a-zA-Z]{2,6})");
        ipPattern = std::make_unique<RegexWrapper>(
            "(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})");

        // Case-insensitive pattern
        caseInsensitivePattern = std::make_unique<RegexWrapper>(
            "hello", ::boost::regex_constants::icase);

        // Invalid pattern (for testing exception cases)
        try {
            invalidPattern = std::make_unique<RegexWrapper>("[");
        } catch (const ::boost::regex_error&) {
            // Expected exception, just continue
        }
    }

    void TearDown() override {
        simplePattern.reset();
        emailPattern.reset();
        ipPattern.reset();
        caseInsensitivePattern.reset();
        invalidPattern.reset();
    }

    std::unique_ptr<RegexWrapper> simplePattern;
    std::unique_ptr<RegexWrapper> emailPattern;
    std::unique_ptr<RegexWrapper> ipPattern;
    std::unique_ptr<RegexWrapper> caseInsensitivePattern;
    std::unique_ptr<RegexWrapper> invalidPattern;

    // Test data
    const std::string testText =
        "Hello, my email is example@example.com and my IP is 192.168.1.1";
    const std::string multiLineText =
        "First line with word1 and word2.\n"
        "Second line with word3 and word4.\n"
        "Email: another@example.org";
};

// Test constructor
TEST_F(RegexWrapperTest, Constructor) {
    // Test with valid pattern
    EXPECT_NO_THROW(RegexWrapper("\\w+"));
    EXPECT_NO_THROW(RegexWrapper(".+"));

    // Test with invalid pattern (should throw)
    EXPECT_THROW(RegexWrapper("["), ::boost::regex_error);

    // Test with flags
    EXPECT_NO_THROW(RegexWrapper("\\w+", ::boost::regex_constants::icase));
    EXPECT_NO_THROW(RegexWrapper("\\w+", ::boost::regex_constants::nosubs));
}

// Test match method
TEST_F(RegexWrapperTest, Match) {
    // Test with matching input
    EXPECT_TRUE(RegexWrapper("^Hello$").match("Hello"));
    EXPECT_TRUE(RegexWrapper("^\\d+$").match("12345"));

    // Test with non-matching input
    EXPECT_FALSE(RegexWrapper("^Hello$").match("Hello World"));
    EXPECT_FALSE(RegexWrapper("^\\d+$").match("12345a"));

    // Test with empty string
    EXPECT_FALSE(simplePattern->match(""));
    EXPECT_TRUE(RegexWrapper("^$").match(""));

    // Test with case-insensitive pattern
    EXPECT_TRUE(caseInsensitivePattern->match("hello"));
    EXPECT_TRUE(caseInsensitivePattern->match("HELLO"));
    EXPECT_TRUE(caseInsensitivePattern->match("Hello"));

    // Test with std::string, string literals, and string_view
    EXPECT_TRUE(simplePattern->match(std::string("word")));
    EXPECT_TRUE(simplePattern->match("word"));
    EXPECT_TRUE(simplePattern->match(std::string_view("word")));
}

// Test search method
TEST_F(RegexWrapperTest, Search) {
    // Test search with matches
    auto result = simplePattern->search(testText);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "Hello");

    // Test email pattern
    result = emailPattern->search(testText);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "example@example.com");

    // Test IP pattern
    result = ipPattern->search(testText);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(*result, "192.168.1.1");

    // Test with no match
    result = RegexWrapper("notfound").search(testText);
    EXPECT_FALSE(result.has_value());

    // Test with empty string
    result = simplePattern->search("");
    EXPECT_FALSE(result.has_value());
}

// Test searchAll method
TEST_F(RegexWrapperTest, SearchAll) {
    // Test with multiple matches
    auto results = simplePattern->searchAll(testText);
    EXPECT_GT(results.size(), 5);
    EXPECT_THAT(results, Contains("Hello"));
    EXPECT_THAT(results, Contains("my"));
    EXPECT_THAT(results, Contains("email"));
    EXPECT_THAT(results, Contains("is"));
    EXPECT_THAT(results, Contains("example"));

    // Test with multi-line text
    results = simplePattern->searchAll(multiLineText);
    EXPECT_THAT(results, Contains("First"));
    EXPECT_THAT(results, Contains("line"));
    EXPECT_THAT(results, Contains("word1"));
    EXPECT_THAT(results, Contains("word2"));
    EXPECT_THAT(results, Contains("Second"));
    EXPECT_THAT(results, Contains("word3"));
    EXPECT_THAT(results, Contains("word4"));
    EXPECT_THAT(results, Contains("Email"));
    EXPECT_THAT(results, Contains("another"));

    // Test email pattern
    results = emailPattern->searchAll(multiLineText);
    EXPECT_THAT(results, ElementsAre("another@example.org"));

    // Test with no matches
    results = RegexWrapper("notfound").searchAll(testText);
    EXPECT_TRUE(results.empty());

    // Test with empty string
    results = simplePattern->searchAll("");
    EXPECT_TRUE(results.empty());
}

// Test replace method
TEST_F(RegexWrapperTest, Replace) {
    // Simple replacement
    auto replaced = RegexWrapper("\\d+").replace(
        "There are 123 apples and 456 oranges", "X");
    EXPECT_EQ(replaced, "There are X apples and X oranges");

    // Replace with capture groups
    replaced = RegexWrapper("(\\w+)@(\\w+)\\.com")
                   .replace("Contact me at user@example.com", "$2@$1.com");
    EXPECT_EQ(replaced, "Contact me at example@user.com");

    // Replace all occurrences
    replaced =
        RegexWrapper("\\s+").replace("This   has   multiple   spaces", " ");
    EXPECT_EQ(replaced, "This has multiple spaces");

    // Replace with empty string
    replaced = RegexWrapper("\\d").replace("abc123def", "");
    EXPECT_EQ(replaced, "abcdef");

    // Replace when no match
    replaced = RegexWrapper("notfound").replace(testText, "replacement");
    EXPECT_EQ(replaced, testText);  // Should remain unchanged

    // Test with empty string
    replaced = simplePattern->replace("", "replacement");
    EXPECT_EQ(replaced, "");  // Should remain empty
}

// Test split method
TEST_F(RegexWrapperTest, Split) {
    // Split by space
    auto parts = RegexWrapper("\\s+").split("This is a test");
    EXPECT_THAT(parts, ElementsAre("This", "is", "a", "test"));

    // Split by comma
    parts = RegexWrapper(",\\s*").split("apple, orange, banana, grape");
    EXPECT_THAT(parts, ElementsAre("apple", "orange", "banana", "grape"));

    // Split with no delimiter matches
    parts = RegexWrapper("notfound").split("This is a test");
    EXPECT_THAT(parts, ElementsAre("This is a test"));

    // Split empty string
    parts = RegexWrapper("\\s+").split("");
    EXPECT_THAT(parts, ElementsAre(""));

    // Split with multiple delimiters
    parts = RegexWrapper("[,;]\\s*").split("apple, orange; banana, grape");
    EXPECT_THAT(parts, ElementsAre("apple", "orange", "banana", "grape"));
}

// Test matchGroups method
TEST_F(RegexWrapperTest, MatchGroups) {
    // Test with email pattern
    auto groups = emailPattern->matchGroups(testText);
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].first, "example@example.com");
    EXPECT_THAT(groups[0].second, ElementsAre("example", "example", "com"));

    // Test with IP pattern
    groups = ipPattern->matchGroups(testText);
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].first, "192.168.1.1");
    EXPECT_THAT(groups[0].second, ElementsAre("192", "168", "1", "1"));

    // Test with multiple matches
    std::string text = "Contact me at user1@example.com or user2@example.org";
    groups = emailPattern->matchGroups(text);
    EXPECT_EQ(groups.size(), 2);
    EXPECT_EQ(groups[0].first, "user1@example.com");
    EXPECT_THAT(groups[0].second, ElementsAre("user1", "example", "com"));
    EXPECT_EQ(groups[1].first, "user2@example.org");
    EXPECT_THAT(groups[1].second, ElementsAre("user2", "example", "org"));

    // Test with no match
    groups = RegexWrapper("notfound").matchGroups(testText);
    EXPECT_TRUE(groups.empty());

    // Test with empty string
    groups = emailPattern->matchGroups("");
    EXPECT_TRUE(groups.empty());
}

// Test forEachMatch method
TEST_F(RegexWrapperTest, ForEachMatch) {
    // Count words using forEachMatch
    int wordCount = 0;
    simplePattern->forEachMatch(
        testText, [&wordCount](const ::boost::smatch&) { wordCount++; });
    EXPECT_GT(wordCount, 5);

    // Collect matches using forEachMatch
    std::vector<std::string> words;
    simplePattern->forEachMatch(testText,
                                [&words](const ::boost::smatch& match) {
                                    words.push_back(match.str());
                                });
    EXPECT_GT(words.size(), 5);
    EXPECT_THAT(words, Contains("Hello"));
    EXPECT_THAT(words, Contains("my"));
    EXPECT_THAT(words, Contains("email"));

    // Process captured groups using forEachMatch
    std::vector<std::string> localParts;
    std::vector<std::string> domains;
    emailPattern->forEachMatch(
        multiLineText, [&](const ::boost::smatch& match) {
            localParts.push_back(match[1].str());
            domains.push_back(match[2].str() + "." + match[3].str());
        });
    EXPECT_THAT(localParts, ElementsAre("another"));
    EXPECT_THAT(domains, ElementsAre("example.org"));

    // Test with no matches
    int count = 0;
    RegexWrapper("notfound")
        .forEachMatch(testText, [&count](const ::boost::smatch&) { count++; });
    EXPECT_EQ(count, 0);

    // Test with empty string
    count = 0;
    simplePattern->forEachMatch("",
                                [&count](const ::boost::smatch&) { count++; });
    EXPECT_EQ(count, 0);
}

// Test getPattern and setPattern methods
TEST_F(RegexWrapperTest, PatternManagement) {
    // Test getPattern
    EXPECT_EQ(simplePattern->getPattern(), "\\w+");
    EXPECT_EQ(emailPattern->getPattern(),
              "([a-zA-Z0-9._%-]+)@([a-zA-Z0-9.-]+)\\.([a-zA-Z]{2,6})");
    EXPECT_EQ(ipPattern->getPattern(),
              "(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})");

    // Test setPattern
    RegexWrapper regex("initial");
    EXPECT_EQ(regex.getPattern(), "initial");

    regex.setPattern("updated");
    EXPECT_EQ(regex.getPattern(), "updated");

    // Test functionality after setPattern
    regex.setPattern("\\d+");
    EXPECT_TRUE(regex.match("12345"));
    EXPECT_FALSE(regex.match("abcde"));

    // Test setPattern with flags
    regex.setPattern("hello", ::boost::regex_constants::icase);
    EXPECT_TRUE(regex.match("HELLO"));
    EXPECT_TRUE(regex.match("hello"));
    EXPECT_TRUE(regex.match("Hello"));

    // Test setPattern with invalid pattern
    EXPECT_THROW(regex.setPattern("["), ::boost::regex_error);
}

// Test namedCaptures method
TEST_F(RegexWrapperTest, NamedCaptures) {
    // Test with email pattern
    auto captures = emailPattern->namedCaptures("user@example.com");
    EXPECT_EQ(captures.size(), 3);
    EXPECT_EQ(captures["1"], "user");
    EXPECT_EQ(captures["2"], "example");
    EXPECT_EQ(captures["3"], "com");

    // Test with IP pattern
    captures = ipPattern->namedCaptures("192.168.1.1");
    EXPECT_EQ(captures.size(), 4);
    EXPECT_EQ(captures["1"], "192");
    EXPECT_EQ(captures["2"], "168");
    EXPECT_EQ(captures["3"], "1");
    EXPECT_EQ(captures["4"], "1");

    // Test with no match
    captures = emailPattern->namedCaptures("not an email");
    EXPECT_TRUE(captures.empty());

    // Test with empty string
    captures = emailPattern->namedCaptures("");
    EXPECT_TRUE(captures.empty());
}

// Test isValid method
TEST_F(RegexWrapperTest, IsValid) {
    // Test with valid inputs
    EXPECT_TRUE(simplePattern->isValid("word"));
    EXPECT_TRUE(emailPattern->isValid("user@example.com"));
    EXPECT_TRUE(ipPattern->isValid("192.168.1.1"));

    // Test with invalid inputs
    EXPECT_FALSE(emailPattern->isValid("not an email"));
    EXPECT_FALSE(ipPattern->isValid("not an ip"));

    // Test with empty string
    EXPECT_FALSE(simplePattern->isValid(""));
    EXPECT_FALSE(emailPattern->isValid(""));
}

// Test replaceCallback method
TEST_F(RegexWrapperTest, ReplaceCallback) {
    // Replace numbers with their square
    auto replaced = RegexWrapper("\\d+").replaceCallback(
        "Numbers: 1, 2, 3, 4, 5", [](const ::boost::smatch& match) {
            int num = std::stoi(match.str());
            return std::to_string(num * num);
        });
    EXPECT_EQ(replaced, "Numbers: 1, 4, 9, 16, 25");

    // Convert email addresses to uppercase
    replaced = emailPattern->replaceCallback(
        "Contact: user1@example.com or user2@example.org",
        [](const ::boost::smatch& match) {
            std::string result = match.str();
            for (char& c : result) {
                c = std::toupper(c);
            }
            return result;
        });
    EXPECT_EQ(replaced, "Contact: USER1@EXAMPLE.COM or USER2@EXAMPLE.ORG");

    // Replace with position-dependent values
    replaced = RegexWrapper("\\w+").replaceCallback(
        "One Two Three Four",
        [count = 0](const ::boost::smatch& match) mutable {
            count++;
            return std::to_string(count);
        });
    EXPECT_EQ(replaced, "1 2 3 4");

    // Test with no matches
    replaced = RegexWrapper("notfound")
                   .replaceCallback(testText, [](const ::boost::smatch&) {
                       return "replacement";
                   });
    EXPECT_EQ(replaced, testText);  // Should remain unchanged

    // Test with empty string
    replaced = simplePattern->replaceCallback(
        "", [](const ::boost::smatch&) { return "replacement"; });
    EXPECT_EQ(replaced, "");  // Should remain empty
}

// Test escapeString method
TEST_F(RegexWrapperTest, EscapeString) {
    // Escape special characters
    EXPECT_EQ(RegexWrapper::escapeString("a.b"), "a\\.b");
    EXPECT_EQ(RegexWrapper::escapeString("a+b"), "a\\+b");
    EXPECT_EQ(RegexWrapper::escapeString("a*b"), "a\\*b");
    EXPECT_EQ(RegexWrapper::escapeString("a?b"), "a\\?b");
    EXPECT_EQ(RegexWrapper::escapeString("a|b"), "a\\|b");
    EXPECT_EQ(RegexWrapper::escapeString("a(b)c"), "a\\(b\\)c");
    EXPECT_EQ(RegexWrapper::escapeString("a[b]c"), "a\\[b\\]c");
    EXPECT_EQ(RegexWrapper::escapeString("a{b}c"), "a\\{b\\}c");
    EXPECT_EQ(RegexWrapper::escapeString("a^b$c"), "a\\^b\\$c");
    EXPECT_EQ(RegexWrapper::escapeString("a\\b"), "a\\\\b");

    // Test with string containing no special characters
    EXPECT_EQ(RegexWrapper::escapeString("abcdef"), "abcdef");

    // Test with empty string
    EXPECT_EQ(RegexWrapper::escapeString(""), "");

    // Verify that escaped strings work in regexes
    std::string patternStr = "user." + RegexWrapper::escapeString("[special]+");
    RegexWrapper pattern(patternStr);
    EXPECT_TRUE(pattern.match("user.[special]+"));
    EXPECT_FALSE(pattern.match("user.whatever"));
}

// Test benchmarkMatch method
TEST_F(RegexWrapperTest, BenchmarkMatch) {
    // Simple benchmark test
    auto duration = simplePattern->benchmarkMatch("word", 10);
    EXPECT_GT(duration.count(), 0);

    // Different input lengths
    auto shortDuration = simplePattern->benchmarkMatch("word", 10);
    auto longDuration =
        simplePattern->benchmarkMatch(std::string(1000, 'a'), 10);

    // Different iteration counts
    auto fewIterations = simplePattern->benchmarkMatch("word", 10);
    auto moreIterations = simplePattern->benchmarkMatch("word", 100);

    // Just making sure the benchmark runs without errors
    // We don't make assertions about actual performance
}

// Test isValidRegex method
TEST_F(RegexWrapperTest, IsValidRegex) {
    // Test with valid patterns
    EXPECT_TRUE(RegexWrapper::isValidRegex("\\w+"));
    EXPECT_TRUE(RegexWrapper::isValidRegex("[a-z]+"));
    EXPECT_TRUE(RegexWrapper::isValidRegex("(abc|def)"));

    // Test with invalid patterns
    EXPECT_FALSE(RegexWrapper::isValidRegex("["));
    EXPECT_FALSE(RegexWrapper::isValidRegex("("));
    EXPECT_FALSE(RegexWrapper::isValidRegex("\\"));

    // Test with empty string
    EXPECT_TRUE(RegexWrapper::isValidRegex(""));
}

// Test countMatches method
TEST_F(RegexWrapperTest, CountMatches) {
    // Count words
    EXPECT_GT(simplePattern->countMatches(testText), 5);

    // Count email addresses
    EXPECT_EQ(emailPattern->countMatches(testText), 1);
    EXPECT_EQ(emailPattern->countMatches("no emails here"), 0);
    EXPECT_EQ(emailPattern->countMatches("user1@example.com user2@example.org"),
              2);

    // Count in multi-line text
    EXPECT_EQ(emailPattern->countMatches(multiLineText), 1);

    // Count with no matches
    EXPECT_EQ(RegexWrapper("notfound").countMatches(testText), 0);

    // Count with empty string
    EXPECT_EQ(simplePattern->countMatches(""), 0);
}

// Test validateAndCompile method
TEST_F(RegexWrapperTest, ValidateAndCompile) {
    // Duplicate of isValidRegex tests to ensure they behave the same
    EXPECT_TRUE(RegexWrapper::validateAndCompile("\\w+"));
    EXPECT_TRUE(RegexWrapper::validateAndCompile("[a-z]+"));
    EXPECT_TRUE(RegexWrapper::validateAndCompile("(abc|def)"));

    EXPECT_FALSE(RegexWrapper::validateAndCompile("["));
    EXPECT_FALSE(RegexWrapper::validateAndCompile("("));
    EXPECT_FALSE(RegexWrapper::validateAndCompile("\\"));

    EXPECT_TRUE(RegexWrapper::validateAndCompile(""));
}

// Test edge cases
TEST_F(RegexWrapperTest, EdgeCases) {
    // Really long pattern
    std::string longPattern(1000, 'a');
    EXPECT_NO_THROW(RegexWrapper varname(longPattern));

    // Really long input
    std::string longInput(10000, 'a');
    EXPECT_TRUE(simplePattern->match(longInput));

    // Unicode pattern and input
    EXPECT_NO_THROW(RegexWrapper("\\p{L}+"));
    RegexWrapper unicodePattern("\\p{L}+");
    EXPECT_TRUE(unicodePattern.match("HelloМирÖäüß"));

    // Empty pattern
    RegexWrapper emptyPattern("");
    EXPECT_TRUE(emptyPattern.match(""));
    EXPECT_FALSE(emptyPattern.match("a"));

    // Large number of capture groups
    std::string manyGroups;
    for (int i = 0; i < 20; i++) {
        manyGroups += "(\\d)";
    }
    RegexWrapper groupPattern(manyGroups);
    auto captures = groupPattern.namedCaptures("12345678901234567890");
    EXPECT_EQ(captures.size(), 20);
}

}  // namespace atom::extra::boost::test

#endif  // ATOM_EXTRA_BOOST_TEST_REGEX_HPP