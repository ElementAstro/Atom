#include "atom/extra/boost/regex.hpp"

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace atom::extra::boost;

void demonstratePatternBuilder() {
    std::cout << "=== Pattern Builder ===" << std::endl;

    PatternBuilder builder;

    // Build a pattern for email validation
    std::string email_pattern = builder
        .startOfLine()
        .group("[a-zA-Z0-9._%+-]+")
        .literal("@")
        .group("[a-zA-Z0-9.-]+")
        .literal(".")
        .group("[a-zA-Z]{2,}")
        .endOfLine()
        .build();

    std::cout << "Email pattern: " << email_pattern << std::endl;

    RegexWrapper email_regex(email_pattern);

    std::vector<std::string> test_emails = {
        "user@example.com",
        "test.email+tag@domain.co.uk",
        "invalid.email",
        "another@test.org"
    };

    for (const auto& email : test_emails) {
        bool is_valid = email_regex.match(email);
        std::cout << "Email '" << email << "' is "
                  << (is_valid ? "valid" : "invalid") << std::endl;
    }

    // Reset and build a phone number pattern
    builder.reset();
    std::string phone_pattern = builder
        .startOfLine()
        .literal("\\+").optional()
        .group("\\d{1,3}")
        .literal("[-\\s]").optional()
        .group("\\d{3,4}")
        .literal("[-\\s]").optional()
        .group("\\d{3,4}")
        .literal("[-\\s]").optional()
        .group("\\d{3,4}")
        .endOfLine()
        .build();

    std::cout << "\nPhone pattern: " << phone_pattern << std::endl;
}

void demonstrateBasicRegexOperations() {
    std::cout << "\n=== Basic Regex Operations ===" << std::endl;

    RegexWrapper word_regex(R"(\b\w+\b)");

    std::string text = "The quick brown fox jumps over the lazy dog.";
    std::cout << "Text: " << text << std::endl;

    // Search for first match
    auto first_match = word_regex.search(text);
    if (first_match) {
        std::cout << "First word: " << *first_match << std::endl;
    }

    // Search for all matches
    auto all_words = word_regex.searchAll(text);
    std::cout << "All words (" << all_words.size() << "): ";
    for (const auto& word : all_words) {
        std::cout << word << " ";
    }
    std::cout << std::endl;

    // Count matches
    size_t word_count = word_regex.countMatches(text);
    std::cout << "Word count: " << word_count << std::endl;

    // Replace words
    std::string replaced = word_regex.replace(text, "[WORD]");
    std::cout << "Replaced: " << replaced << std::endl;
}

void demonstrateDetailedMatching() {
    std::cout << "\n=== Detailed Matching ===" << std::endl;

    RegexWrapper number_regex(R"((\d+)\.(\d+))");

    std::string text = "Version 1.2 and 3.14 and 42.0 are numbers.";
    std::cout << "Text: " << text << std::endl;

    auto detailed_matches = number_regex.searchDetailed(text);

    std::cout << "Detailed matches:" << std::endl;
    for (const auto& match : detailed_matches) {
        std::cout << "  Match: '" << match.match << "' at position " << match.position
                  << " (length: " << match.length << ")" << std::endl;
        std::cout << "  Groups: ";
        for (const auto& group : match.groups) {
            std::cout << "'" << group << "' ";
        }
        std::cout << std::endl;
        std::cout << "  Match time: " << match.match_time.count() << " nanoseconds" << std::endl;
    }
}

void demonstrateGroupMatching() {
    std::cout << "\n=== Group Matching ===" << std::endl;

    RegexWrapper date_regex(R"((\d{4})-(\d{2})-(\d{2}))");

    std::string text = "Important dates: 2023-12-25 and 2024-01-01.";
    std::cout << "Text: " << text << std::endl;

    auto group_matches = date_regex.matchGroups(text);

    std::cout << "Date matches with groups:" << std::endl;
    for (const auto& [full_match, groups] : group_matches) {
        std::cout << "  Full match: " << full_match << std::endl;
        if (groups.size() >= 3) {
            std::cout << "    Year: " << groups[0] << std::endl;
            std::cout << "    Month: " << groups[1] << std::endl;
            std::cout << "    Day: " << groups[2] << std::endl;
        }
    }
}

void demonstrateCallbackReplacement() {
    std::cout << "\n=== Callback Replacement ===" << std::endl;

    RegexWrapper number_regex(R"(\d+)");

    std::string text = "I have 5 apples and 10 oranges.";
    std::cout << "Original: " << text << std::endl;

    // Replace numbers with their doubled values
    std::string doubled = number_regex.replaceCallback(text,
        [](const boost::smatch& match) {
            int value = std::stoi(match.str());
            return std::to_string(value * 2);
        });

    std::cout << "Doubled: " << doubled << std::endl;

    // Replace with uppercase words
    RegexWrapper word_regex(R"(\b\w+\b)");
    std::string uppercased = word_regex.replaceCallback(text,
        [](const boost::smatch& match) {
            std::string word = match.str();
            std::transform(word.begin(), word.end(), word.begin(), ::toupper);
            return word;
        });

    std::cout << "Uppercased: " << uppercased << std::endl;
}

void demonstrateParallelProcessing() {
    std::cout << "\n=== Parallel Processing ===" << std::endl;

    RegexWrapper email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");

    std::vector<std::string> texts = {
        "Contact us at support@example.com for help.",
        "Send reports to admin@company.org and backup@company.org.",
        "No emails in this text.",
        "Multiple emails: user1@test.com, user2@test.com, user3@test.com.",
        "Another email: info@website.net"
    };

    std::cout << "Processing " << texts.size() << " texts in parallel..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto results = email_regex.parallelSearchAll(std::span<const std::string>(texts));
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    for (size_t i = 0; i < texts.size(); ++i) {
        std::cout << "Text " << i + 1 << ": " << results[i].size() << " emails found" << std::endl;
        for (const auto& email : results[i]) {
            std::cout << "  " << email << std::endl;
        }
    }

    std::cout << "Parallel processing time: " << duration.count() << " microseconds" << std::endl;
}

void demonstratePerformanceAndCaching() {
    std::cout << "\n=== Performance and Caching ===" << std::endl;

    RegexWrapper::resetStatistics();

    // Create multiple regex objects with the same pattern to test caching
    std::string pattern = R"(\b\w{5,}\b)"; // Words with 5+ characters

    std::vector<RegexWrapper> regexes;
    for (int i = 0; i < 10; ++i) {
        regexes.emplace_back(pattern);
    }

    std::string test_text = "This is a comprehensive test of the enhanced regex functionality with caching.";

    // Perform multiple operations
    for (const auto& regex : regexes) {
        auto results = regex.searchAll(test_text);
        auto count = regex.countMatches(test_text);
        (void)results; // Suppress unused variable warning
        (void)count;   // Suppress unused variable warning
    }

    auto stats = RegexWrapper::getStatistics();
    std::cout << "Performance Statistics:" << std::endl;
    std::cout << "  Total matches: " << stats.total_matches << std::endl;
    std::cout << "  Cache hits: " << stats.cache_hits << std::endl;
    std::cout << "  Cache misses: " << stats.cache_misses << std::endl;
    std::cout << "  Compilation time: " << stats.compilation_time_ns << " nanoseconds" << std::endl;

    if (stats.cache_hits + stats.cache_misses > 0) {
        double hit_ratio = static_cast<double>(stats.cache_hits) / (stats.cache_hits + stats.cache_misses) * 100.0;
        std::cout << "  Cache hit ratio: " << std::fixed << std::setprecision(1) << hit_ratio << "%" << std::endl;
    }
}

void demonstrateBenchmarking() {
    std::cout << "\n=== Benchmarking ===" << std::endl;

    RegexWrapper complex_regex(R"((?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\]))");

    std::string test_email = "test.email@example.com";

    auto avg_time = complex_regex.benchmarkMatch(test_email, 10000);
    std::cout << "Complex email regex benchmark:" << std::endl;
    std::cout << "  Average match time: " << avg_time.count() << " nanoseconds" << std::endl;
    std::cout << "  Matches per second: " << std::fixed << std::setprecision(0)
              << (1000000000.0 / avg_time.count()) << std::endl;
}

int main() {
    try {
        demonstratePatternBuilder();
        demonstrateBasicRegexOperations();
        demonstrateDetailedMatching();
        demonstrateGroupMatching();
        demonstrateCallbackReplacement();
        demonstrateParallelProcessing();
        demonstratePerformanceAndCaching();
        demonstrateBenchmarking();

        std::cout << "\n=== All regex tests completed successfully! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
