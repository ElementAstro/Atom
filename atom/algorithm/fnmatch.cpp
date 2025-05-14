/*
 * fnmatch.cpp
 *
 * Copyright (C) 2023-2024 MaxQ <lightapttech.com>
 */

/*************************************************

Date: 2024-5-2

Description: Enhanced Python-Like fnmatch for C++

**************************************************/

#include "fnmatch.hpp"

#include <algorithm>
#include <cctype>
#include <execution>
#include <list>
#include <memory>
#include <mutex>
#include <regex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#else
#include <fnmatch.h>
#endif

#include "atom/log/loguru.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/regex.hpp>
#endif

#ifdef __SSE4_2__
#include <smmintrin.h>
#endif

namespace atom::algorithm {

// Cached pattern storage for performance optimization
namespace {
// LRU cache for compiled patterns
class PatternCache {
private:
    struct CacheEntry {
        std::string pattern;
        int flags;
        std::shared_ptr<std::regex> regex;
        std::chrono::steady_clock::time_point last_used;
    };

    // Maximum number of cached patterns
    static constexpr size_t MAX_CACHE_SIZE = 128;

    mutable std::mutex cache_mutex_;
    std::list<CacheEntry> entries_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> lookup_;

public:
    PatternCache() = default;

    // Get or create a regex for a pattern
    std::shared_ptr<std::regex> get_regex(std::string_view pattern, int flags) {
        const std::string pattern_key =
            std::string(pattern) + ":" + std::to_string(flags);

        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto it = lookup_.find(pattern_key);
        if (it != lookup_.end()) {
            // Move to front (most recently used)
            auto entry_it = it->second;
            entry_it->last_used = std::chrono::steady_clock::now();
            entries_.splice(entries_.begin(), entries_, entry_it);
            return entry_it->regex;
        }

        // Create new regex
        std::string regex_str;
        auto result = translate(pattern, flags);
        if (!result) {
            // Handle translation error
            throw FnmatchException("Failed to translate pattern to regex");
        }

        regex_str = std::move(result.value());

        std::shared_ptr<std::regex> new_regex;
        try {
            int regex_flags = std::regex::ECMAScript;
            if (flags & flags::CASEFOLD) {
                regex_flags |= std::regex::icase;
            }
            new_regex = std::make_shared<std::regex>(
                regex_str, static_cast<std::regex::flag_type>(regex_flags));
        } catch (const std::regex_error& e) {
            throw FnmatchException("Invalid regex pattern: " +
                                   std::string(e.what()));
        }

        // Add to cache
        CacheEntry entry{.pattern = std::string(pattern),
                         .flags = flags,
                         .regex = new_regex,
                         .last_used = std::chrono::steady_clock::now()};

        entries_.push_front(entry);
        lookup_[pattern_key] = entries_.begin();

        // Enforce cache size limit
        if (entries_.size() > MAX_CACHE_SIZE) {
            auto oldest = std::prev(entries_.end());
            lookup_.erase(oldest->pattern + ":" +
                          std::to_string(oldest->flags));
            entries_.pop_back();
        }

        return new_regex;
    }
};

[[maybe_unused]] PatternCache& get_pattern_cache() {
    static PatternCache cache;
    return cache;
}

}  // namespace

// Implementation of main functions

template <StringLike T1, StringLike T2>
auto fnmatch(T1&& pattern, T2&& string, int flags) -> bool {
    LOG_F(INFO, "fnmatch called with pattern: {}, string: {}, flags: {}",
          std::string_view(pattern), std::string_view(string), flags);

    try {
        // Use the nothrow version and handle possible errors
        auto result = fnmatch_nothrow(std::forward<T1>(pattern),
                                      std::forward<T2>(string), flags);

        if (!result) {
            const char* error_msg = "Unknown error";
            switch (result.error()) {
                case FnmatchError::InvalidPattern:
                    error_msg = "Invalid pattern";
                    break;
                case FnmatchError::UnmatchedBracket:
                    error_msg = "Unmatched bracket in pattern";
                    break;
                case FnmatchError::EscapeAtEnd:
                    error_msg = "Escape character at end of pattern";
                    break;
                case FnmatchError::InternalError:
                    error_msg = "Internal error during matching";
                    break;
            }
            throw FnmatchException(error_msg);
        }

        return result.value();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in fnmatch: {}", e.what());
        throw FnmatchException(e.what());
    } catch (...) {  // 移到最后
        throw FnmatchException("Unknown error occurred");
    }
}

template <StringLike T1, StringLike T2>
auto fnmatch_nothrow(T1&& pattern, T2&& string, int flags) noexcept
    -> atom::type::expected<bool, FnmatchError> {
    const std::string_view pattern_view(pattern);
    const std::string_view string_view(string);

    // Input validation
    if (pattern_view.empty()) {
        return string_view.empty();  // Empty pattern matches only empty string
    }

#ifdef ATOM_USE_BOOST
    try {
        // Boost regex implementation
        auto translated = translate(pattern_view, flags);
        if (!translated) {
            return atom::type::unexpected(translated.error());
        }

        boost::regex::flag_type regex_flags = boost::regex::ECMAScript;
        if (flags & flags::CASEFOLD) {
            regex_flags |= boost::regex::icase;
        }

        // Use boost regex for matching
        boost::regex regex(translated.value(), regex_flags);
        bool result = boost::regex_match(
            std::string(string_view.begin(), string_view.end()), regex);

        LOG_F(INFO, "Boost regex match result: {}", result ? "True" : "False");
        return result;
    } catch (...) {
        LOG_F(ERROR, "Exception in Boost regex implementation");
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#else
#ifdef _WIN32
    try {
        // First, try to use compiled regex for better performance
        auto regex = get_pattern_cache().get_regex(pattern_view, flags);

        // Match with compiled regex
        if (std::regex_match(
                std::string(string_view.begin(), string_view.end()), *regex)) {
            LOG_F(INFO, "Regex match successful");
            return true;
        }

        return false;
    } catch (...) {
        // Fall back to manual implementation on regex failure
        LOG_F(WARNING, "Regex failed, falling back to manual implementation");

        auto p = pattern_view.begin();
        auto s = string_view.begin();

        while (p != pattern_view.end() && s != string_view.end()) {
            const char current_char = *p;
            switch (current_char) {
                case '?': {
                    LOG_F(INFO, "Wildcard '?' encountered.");
                    ++s;
                    ++p;
                    break;
                }
                case '*': {
                    LOG_F(INFO, "Wildcard '*' encountered.");
                    if (++p == pattern_view.end()) {
                        LOG_F(INFO,
                              "Trailing '*' matches the rest of the string.");
                        return true;
                    }

                    // Check for wildcard characters in the remaining pattern
                    auto check_wildcards = [](auto start, auto end) {
                        return std::any_of(start, end, [](char c) {
                            return c == '*' || c == '?' || c == '[';
                        });
                    };

                    if (!check_wildcards(p, pattern_view.end())) {
                        const auto suffix_len =
                            static_cast<std::ptrdiff_t>(pattern_view.end() - p);
                        const auto remaining_len =
                            static_cast<std::ptrdiff_t>(string_view.end() - s);
                        if (suffix_len > remaining_len) {
                            return false;
                        }

                        const bool match = std::equal(
                            pattern_view.end() - suffix_len, pattern_view.end(),
                            string_view.end() - suffix_len,
                            [flags](char a, char b) {
                                return (flags & flags::CASEFOLD)
                                           ? (std::tolower(a) ==
                                              std::tolower(b))
                                           : (a == b);
                            });
                        return match;
                    }

                    while (s != string_view.end()) {
                        auto inner_result = fnmatch_nothrow(
                            std::string_view(p, pattern_view.end() - p),
                            std::string_view(s, string_view.end() - s), flags);

                        if (!inner_result) {
                            return inner_result;
                        }

                        if (inner_result.value()) {
                            return true;
                        }
                        ++s;
                    }
                    LOG_F(INFO, "No match found after '*'.");
                    return false;
                }
                case '[': {
                    // ... existing bracket matching code ...
                    ++p;  // Skip '[' for next iteration
                    break;
                }
                case '\\': {
                    if ((flags & flags::NOESCAPE) == 0) {
                        if (++p == pattern_view.end()) {
                            LOG_F(ERROR,
                                  "Escape character '\\' at end of pattern.");
                            return atom::type::unexpected(
                                FnmatchError::EscapeAtEnd);
                        }
                    }
                    // Intentionally fall through to default case
                    [[fallthrough]];
                }
                default: {
                    if ((flags & flags::CASEFOLD)
                            ? (std::tolower(*p) != std::tolower(*s))
                            : (*p != *s)) {
                        LOG_F(INFO,
                              "Literal character mismatch: pattern '{}' vs "
                              "string '{}'",
                              *p, *s);
                        return false;
                    }
                    ++s;
                    ++p;
                    break;
                }
            }
        }

        // Handle remaining pattern (e.g., trailing '*')
        while (p != pattern_view.end() && *p == '*') {
            ++p;
        }

        const bool result = p == pattern_view.end() && s == string_view.end();
        LOG_F(INFO, "Match result: {}", result ? "True" : "False");
        return result;
    }
#else
    try {
        LOG_F(INFO, "Using system fnmatch.");
        // Convert string_view to null-terminated strings for system fnmatch
        const std::string pattern_str(pattern_view);
        const std::string string_str(string_view);

        int ret = ::fnmatch(pattern_str.c_str(), string_str.c_str(), flags);
        bool result = (ret == 0);
        LOG_F(INFO, "System fnmatch result: {}", result ? "True" : "False");
        return result;
    } catch (...) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#endif
#endif
}

template <std::ranges::input_range Range, StringLike Pattern>
    requires StringLike<std::ranges::range_value_t<Range>>
auto filter(const Range& names, Pattern&& pattern, int flags) -> bool {
    LOG_F(INFO, "Filter called with pattern: {} and range of names.",
          std::string_view(pattern));

    try {
        // Use C++20 ranges for cleaner, more efficient code
        return std::ranges::any_of(names, [&pattern, flags](const auto& name) {
            try {
                bool match = fnmatch(pattern, name, flags);
                LOG_F(INFO, "Checking if \"{}\" matches pattern \"{}\": {}",
                      std::string_view(name), std::string_view(pattern),
                      match ? "Yes" : "No");
                return match;
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception while matching name: {}", e.what());
                return false;  // Skip names that cause exceptions
            }
        });
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in filter: {}", e.what());
        throw FnmatchException(std::string("Filter operation failed: ") +
                               e.what());
    }
}

template <std::ranges::input_range Range, std::ranges::input_range PatternRange>
    requires StringLike<std::ranges::range_value_t<Range>> &&
             StringLike<std::ranges::range_value_t<PatternRange>>
auto filter(const Range& names, const PatternRange& patterns, int flags,
            bool use_parallel)
    -> std::vector<std::ranges::range_value_t<Range>> {
    using result_type = std::ranges::range_value_t<Range>;
    LOG_F(INFO, "Filter called with multiple patterns and {} names.",
          std::ranges::distance(names));

    std::vector<result_type> result;

    try {
        // Pre-allocate with a reasonable capacity to avoid reallocations
        const auto names_size = std::ranges::distance(names);
        result.reserve(std::min(static_cast<size_t>(names_size),
                                static_cast<size_t>(128)));

        // Prepare patterns for parallel matching
        std::vector<std::string_view> pattern_views;
        pattern_views.reserve(std::ranges::distance(patterns));
        for (const auto& p : patterns) {
            pattern_views.emplace_back(p);
        }

        // Create synchronized result vector for parallel execution
        std::mutex result_mutex;

        // Process names
        auto process_name = [&](const auto& name) {
            bool matched = false;
            const std::string_view name_view(name);

            // Determine execution policy based on parameter and pattern count
            if (use_parallel && pattern_views.size() > 4) {
                // Use parallel execution for many patterns
                matched = std::any_of(
                    std::execution::par_unseq, pattern_views.begin(),
                    pattern_views.end(),
                    [&name_view, flags](const std::string_view& pattern) {
                        auto match_result =
                            fnmatch_nothrow(pattern, name_view, flags);
                        return match_result && match_result.value();
                    });
            } else {
                // Sequential for few patterns
                matched = std::ranges::any_of(
                    pattern_views,
                    [&name_view, flags](const std::string_view& pattern) {
                        auto match_result =
                            fnmatch_nothrow(pattern, name_view, flags);
                        return match_result && match_result.value();
                    });
            }

            if (matched) {
                LOG_F(INFO, "Name \"{}\" matches at least one pattern.",
                      name_view);
                std::lock_guard<std::mutex> lock(result_mutex);
                result.push_back(name);
            }
        };

        // Execute with appropriate policy
        if (use_parallel && names_size > 100) {
            std::for_each(std::execution::par_unseq, std::ranges::begin(names),
                          std::ranges::end(names), process_name);
        } else {
            std::ranges::for_each(names, process_name);
        }

        LOG_F(INFO, "Filter result contains {} matched names.", result.size());
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in multiple patterns filter: {}", e.what());
        throw FnmatchException(std::string("Multi-pattern filter failed: ") +
                               e.what());
    }
}

template <StringLike Pattern>
auto translate(Pattern&& pattern, int flags) noexcept
    -> atom::type::expected<std::string, FnmatchError> {
    const std::string_view pattern_view(pattern);
    LOG_F(INFO, "Translating pattern: {} with flags: {}", pattern_view, flags);

    std::string result;
    result.reserve(pattern_view.size() * 2);

    try {
        for (auto it = pattern_view.begin(); it != pattern_view.end(); ++it) {
            switch (*it) {
                case '*':
                    LOG_F(INFO, "Translating '*' to '.*'");
                    result += ".*";
                    break;

                case '?':
                    LOG_F(INFO, "Translating '?' to '.'");
                    result += '.';
                    break;

                case '[': {
                    result += '[';
                    if (++it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    // Handle character class negation
                    if (*it == '!' || *it == '^') {
                        result += '^';
                        ++it;
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    // Handle special case of [] or [!] patterns
                    if (*it == ']') {
                        result += *it;
                        ++it;
                        if (it == pattern_view.end()) {
                            return atom::type::unexpected(
                                FnmatchError::UnmatchedBracket);
                        }
                    }

                    // Process character class contents without using lastChar
                    while (it != pattern_view.end() && *it != ']') {
                        if (*it == '-' && it + 1 != pattern_view.end() &&
                            *(it + 1) != ']') {
                            result += *it++;
                            if (it == pattern_view.end()) {
                                return atom::type::unexpected(
                                    FnmatchError::UnmatchedBracket);
                            }
                            result += *it;
                        } else {
                            result += *it;
                        }
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    result += ']';
                    break;
                }

                case '\\':
                    LOG_F(INFO, "Processing escape character '\\'");
                    if ((flags & flags::NOESCAPE) == 0) {
                        if (++it == pattern_view.end()) {
                            LOG_F(ERROR,
                                  "Escape character '\\' at end of pattern.");
                            return atom::type::unexpected(
                                FnmatchError::EscapeAtEnd);
                        }
                    }
                    [[fallthrough]];

                default:
                    if ((flags & flags::CASEFOLD) && std::isalpha(*it)) {
                        LOG_F(INFO,
                              "Translating alphabetic character with case "
                              "folding: {}",
                              *it);
                        result += '[';
                        result += static_cast<char>(std::tolower(*it));
                        result += static_cast<char>(std::toupper(*it));
                        result += ']';
                    } else {
                        LOG_F(INFO, "Translating literal character: {}", *it);
                        result += *it;
                    }
                    break;
            }
        }
        LOG_F(INFO, "Translation successful. Resulting regex: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in translate: {}", e.what());
        return atom::type::unexpected(FnmatchError::InternalError);
    }
}

}  // namespace atom::algorithm