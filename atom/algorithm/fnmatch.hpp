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
[[nodiscard]] auto filter(const Range& names, Pattern&& pattern, int flags = 0)
    -> bool;

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

}  // namespace atom::algorithm

#endif  // ATOM_SYSTEM_FNMATCH_HPP
