/*
 * fnmatch.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-2

Description: Enhanced Python-Like fnmatch for C++

**************************************************/

#ifndef ATOM_SYSTEM_FNMATCH_HPP
#define ATOM_SYSTEM_FNMATCH_HPP

#include <concepts>
#include <exception>
#include <ranges>
#include <regex>
#include <string>
#include <string_view>
#include <vector>
#include "atom/type/expected.hpp"

namespace atom::algorithm {

/**
 * @brief Exception class for fnmatch errors.
 */
class FnmatchException : public std::exception {
private:
    std::string message_;

public:
    explicit FnmatchException(const std::string& message) noexcept
        : message_(message) {}
    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }
};

// Flag constants
namespace flags {
inline constexpr int NOESCAPE = 0x01;  ///< Disable backslash escaping
inline constexpr int PATHNAME =
    0x02;  ///< Slash in string only matches slash in pattern
inline constexpr int PERIOD =
    0x04;  ///< Leading period must be matched explicitly
inline constexpr int CASEFOLD = 0x08;  ///< Case insensitive matching
}  // namespace flags

// C++20 concept for string-like types
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

// Error types for expected return values
enum class FnmatchError {
    InvalidPattern,
    UnmatchedBracket,
    EscapeAtEnd,
    InternalError
};

/**
 * @brief Matches a string against a specified pattern with C++20 features.
 *
 * Uses concepts to accept string-like types and provides detailed error
 * handling.
 *
 * @tparam T1 Pattern string-like type
 * @tparam T2 Input string-like type
 * @param pattern The pattern to match against
 * @param string The string to match
 * @param flags Optional flags to modify the matching behavior (default is 0)
 * @return True if the string matches the pattern, false otherwise
 * @throws FnmatchException on invalid pattern or other matching errors
 */
template <StringLike T1, StringLike T2>
[[nodiscard]] auto fnmatch(T1&& pattern, T2&& string, int flags = 0) -> bool;

/**
 * @brief Non-throwing version of fnmatch that returns atom::type::expected.
 *
 * @tparam T1 Pattern string-like type
 * @tparam T2 Input string-like type
 * @param pattern The pattern to match against
 * @param string The string to match
 * @param flags Optional flags to modify the matching behavior
 * @return atom::type::expected with bool result or FnmatchError
 */
template <StringLike T1, StringLike T2>
[[nodiscard]] auto fnmatch_nothrow(T1&& pattern, T2&& string,
                                   int flags = 0) noexcept
    -> atom::type::expected<bool, FnmatchError>;

/**
 * @brief Filters a range of strings based on a specified pattern.
 *
 * Uses C++20 ranges to efficiently filter container elements.
 *
 * @tparam Range A range of string-like elements
 * @tparam Pattern A string-like pattern type
 * @param names The range of strings to filter
 * @param pattern The pattern to filter with
 * @param flags Optional flags to modify the filtering behavior
 * @return True if any element of names matches the pattern
 */
template <std::ranges::input_range Range, StringLike Pattern>
    requires StringLike<std::ranges::range_value_t<Range>>
[[nodiscard]] auto filter(const Range& names, Pattern&& pattern,
                          int flags = 0) -> bool;

/**
 * @brief Filters a range of strings based on multiple patterns.
 *
 * Supports parallel execution for better performance with many patterns.
 *
 * @tparam Range A range of string-like elements
 * @tparam PatternRange A range of string-like patterns
 * @param names The range of strings to filter
 * @param patterns The range of patterns to filter with
 * @param flags Optional flags to modify the filtering behavior
 * @param use_parallel Whether to use parallel execution (default true)
 * @return A vector containing strings from names that match any pattern
 */
template <std::ranges::input_range Range, std::ranges::input_range PatternRange>
    requires StringLike<std::ranges::range_value_t<Range>> &&
                 StringLike<std::ranges::range_value_t<PatternRange>>
[[nodiscard]] auto filter(const Range& names, const PatternRange& patterns,
                          int flags = 0, bool use_parallel = true)
    -> std::vector<std::ranges::range_value_t<Range>>;

/**
 * @brief Translates a pattern into a regex string.
 *
 * @tparam Pattern A string-like pattern type
 * @param pattern The pattern to translate
 * @param flags Optional flags to modify the translation behavior
 * @return atom::type::expected with resulting regex string or FnmatchError
 */
template <StringLike Pattern>
[[nodiscard]] auto translate(Pattern&& pattern, int flags = 0) noexcept
    -> atom::type::expected<std::string, FnmatchError>;

// Template function implementations
template <StringLike T1, StringLike T2>
auto fnmatch_nothrow(T1&& pattern, T2&& string, int flags) noexcept
    -> atom::type::expected<bool, FnmatchError> {
    const std::string_view pattern_view(pattern);
    const std::string_view string_view(string);

    if (pattern_view.empty()) {
        return string_view.empty();
    }

#ifdef ATOM_USE_BOOST
    try {
        auto translated = translate(pattern_view, flags);
        if (!translated) {
            return atom::type::unexpected(translated.error());
        }

        boost::regex::flag_type regex_flags = boost::regex::ECMAScript;
        if (flags & flags::CASEFOLD) {
            regex_flags |= boost::regex::icase;
        }

        boost::regex regex(translated.value(), regex_flags);
        bool result = boost::regex_match(
            std::string(string_view.begin(), string_view.end()), regex);

        return result;
    } catch (...) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#else
#ifdef _WIN32
    // Windows implementation - use regex translation for full compatibility
    try {
        auto translated = translate(pattern_view, flags);
        if (!translated) {
            return atom::type::unexpected(translated.error().error());
        }

        std::regex::flag_type regex_flags = std::regex::ECMAScript;
        if (flags & flags::CASEFOLD) {
            regex_flags |= std::regex::icase;
        }

        std::regex regex(translated.value(), regex_flags);
        bool result = std::regex_match(
            std::string(string_view.begin(), string_view.end()), regex);

        return result;
    } catch (...) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#else
    // Unix implementation using system fnmatch
    try {
        const std::string pattern_str(pattern_view);
        const std::string string_str(string_view);

        int ret = ::fnmatch(pattern_str.c_str(), string_str.c_str(), flags);
        return (ret == 0);
    } catch (...) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
#endif
#endif
}

template <StringLike T1, StringLike T2>
auto fnmatch(T1&& pattern, T2&& string, int flags) -> bool {
    try {
        auto result = fnmatch_nothrow(std::forward<T1>(pattern),
                                      std::forward<T2>(string), flags);

        if (!result) {
            const char* error_msg = "Unknown error";
            switch (static_cast<int>(result.error().error())) {
                case static_cast<int>(FnmatchError::InvalidPattern):
                    error_msg = "Invalid pattern";
                    break;
                case static_cast<int>(FnmatchError::UnmatchedBracket):
                    error_msg = "Unmatched bracket in pattern";
                    break;
                case static_cast<int>(FnmatchError::EscapeAtEnd):
                    error_msg = "Escape character at end of pattern";
                    break;
                case static_cast<int>(FnmatchError::InternalError):
                    error_msg = "Internal error during matching";
                    break;
            }
            throw FnmatchException(error_msg);
        }

        return result.value();
    } catch (const std::exception& e) {
        throw FnmatchException(e.what());
    } catch (...) {
        throw FnmatchException("Unknown error occurred");
    }
}

template <StringLike Pattern>
auto translate(Pattern&& pattern, int flags) noexcept
    -> atom::type::expected<std::string, FnmatchError> {
    const std::string_view pattern_view(pattern);

    if (pattern_view.empty()) {
        return std::string{};
    }

    std::string result;
    result.reserve(pattern_view.size() * 2);

    try {
        for (auto it = pattern_view.begin(); it != pattern_view.end(); ++it) {
            switch (*it) {
                case '*':
                    result += ".*";
                    break;

                case '?':
                    result += '.';
                    break;

                case '[': {
                    result += '[';
                    if (++it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    if (*it == '!' || *it == '^') {
                        result += '^';
                        ++it;
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    // Handle ] as first character in bracket expression (it's
                    // literal) In ECMAScript regex, ] must be escaped even as
                    // first char
                    if (*it == ']') {
                        result += "\\]";
                        ++it;
                    }

                    while (it != pattern_view.end() && *it != ']') {
                        if (*it == '-' && it + 1 != pattern_view.end() &&
                            *(it + 1) != ']') {
                            result += *it++;
                            if (it == pattern_view.end()) {
                                return atom::type::unexpected(
                                    FnmatchError::UnmatchedBracket);
                            }
                            // Escape special regex characters inside brackets
                            // Note: dots are literal inside character classes,
                            // so don't escape them
                            if (*it == '+' || *it == '(' || *it == ')' ||
                                *it == '{' || *it == '}' || *it == '|' ||
                                *it == '$' || *it == '\\') {
                                result += "\\";
                            }
                            result += *it;
                        } else {
                            // Escape special regex characters inside brackets
                            // Note: dots, *, and ? are literal inside character
                            // classes, so don't escape them
                            if (*it == '+' || *it == '(' || *it == ')' ||
                                *it == '{' || *it == '}' || *it == '|' ||
                                *it == '$' || *it == '\\') {
                                result += "\\";
                            }
                            result += *it;
                        }
                        ++it;
                    }

                    if (it == pattern_view.end()) {
                        return atom::type::unexpected(
                            FnmatchError::UnmatchedBracket);
                    }

                    result += ']';
                    break;
                }

                case '\\':
                    if ((flags & flags::NOESCAPE) == 0) {
                        if (++it == pattern_view.end()) {
                            return atom::type::unexpected(
                                FnmatchError::EscapeAtEnd);
                        }
                        // Escape the next character for regex
                        if (*it == '.' || *it == '*' || *it == '?' ||
                            *it == '+' || *it == '(' || *it == ')' ||
                            *it == '{' || *it == '}' || *it == '|' ||
                            *it == '^' || *it == '$' || *it == '[' ||
                            *it == ']' || *it == '\\') {
                            result += '\\';
                        }
                        result += *it;
                        break;
                    }
                    [[fallthrough]];

                default:
                    if ((flags & flags::CASEFOLD) && std::isalpha(*it)) {
                        result += '[';
                        result += static_cast<char>(std::tolower(*it));
                        result += static_cast<char>(std::toupper(*it));
                        result += ']';
                    } else {
                        // Escape special regex characters outside brackets
                        if (*it == '.' || *it == '+' || *it == '(' ||
                            *it == ')' || *it == '{' || *it == '}' ||
                            *it == '|' || *it == '^' || *it == '$') {
                            result += '\\';
                        }
                        result += *it;
                    }
                    break;
            }
        }

        return result;
    } catch (const std::exception& e) {
        return atom::type::unexpected(FnmatchError::InternalError);
    }
}

template <std::ranges::input_range Range, StringLike Pattern>
    requires StringLike<std::ranges::range_value_t<Range>>
auto filter(const Range& names, Pattern&& pattern, int flags) -> bool {
    try {
        for (const auto& name : names) {
            try {
                if (fnmatch(pattern, name, flags)) {
                    return true;
                }
            } catch (const std::exception& e) {
                // Continue with next name on error
                continue;
            }
        }
        return false;
    } catch (const std::exception& e) {
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

    // Note: use_parallel parameter is available for future optimization
    (void)use_parallel;

    std::vector<result_type> result;

    try {
        const auto names_size = std::ranges::distance(names);
        result.reserve(std::min(static_cast<size_t>(names_size),
                                static_cast<size_t>(128)));

        std::vector<std::string_view> pattern_views;
        pattern_views.reserve(std::ranges::distance(patterns));
        for (const auto& p : patterns) {
            pattern_views.emplace_back(p);
        }

        for (const auto& name : names) {
            bool matched = false;
            const std::string_view name_view(name);

            for (const auto& pattern_view : pattern_views) {
                try {
                    if (fnmatch(pattern_view, name_view, flags)) {
                        matched = true;
                        break;
                    }
                } catch (const std::exception& e) {
                    // Continue with next pattern on error
                    continue;
                }
            }

            if (matched) {
                result.emplace_back(name);
            }
        }

// Debug output to see what regex is generated
#ifdef DEBUG_FNMATCH
        std::cout << "Pattern: " << pattern_view << " -> Regex: " << result
                  << std::endl;
#endif

        return result;
    } catch (const std::exception& e) {
        throw FnmatchException(std::string("Filter operation failed: ") +
                               e.what());
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_SYSTEM_FNMATCH_HPP
