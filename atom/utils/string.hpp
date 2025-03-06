/*
 * string.hpp
 *
 * Copyright (C) 2023-2024 Max Q. <contact@lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Some useful string functions

**************************************************/

#ifndef ATOM_UTILS_STRING_HPP
#define ATOM_UTILS_STRING_HPP

#include <array>
#include <list>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace atom::utils {

// Define concepts for string types
template <typename T>
concept StringLike = std::is_convertible_v<T, std::string_view>;

template <typename F>
concept DelimiterPredicate =
    std::is_invocable_r_v<bool, F, char> || std::is_same_v<F, char> ||
    std::is_same_v<F, std::string_view>;

/**
 * @brief Checks if the given string contains any uppercase characters.
 *
 * @param str The string to check.
 * @return true if the string contains uppercase characters, otherwise false.
 * @throws None
 */
[[nodiscard]] auto hasUppercase(std::string_view str) -> bool;

/**
 * @brief Converts the given string to snake_case format.
 *
 * @param str The string to convert.
 * @return The string converted to snake_case.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard]] auto toUnderscore(std::string_view str) -> std::string;

/**
 * @brief Converts the given string to camelCase format.
 *
 * @param str The string to convert.
 * @return The string converted to camelCase.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard]] auto toCamelCase(std::string_view str) -> std::string;

/**
 * @brief Encodes the given string using URL encoding.
 *
 * @param str The string to encode.
 * @return The URL encoded string.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard]] auto urlEncode(std::string_view str) -> std::string;

/**
 * @brief Decodes the given URL encoded string.
 *
 * @param str The URL encoded string to decode.
 * @return The decoded string.
 * @throws std::invalid_argument If the input contains invalid escape sequences.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard]] auto urlDecode(std::string_view str) -> std::string;

/**
 * @brief Checks if the given string starts with the specified prefix.
 *
 * @param str The string to check.
 * @param prefix The prefix to search for.
 * @return true if the string starts with the prefix, otherwise false.
 * @throws None
 */
[[nodiscard]] auto startsWith(std::string_view str,
                              std::string_view prefix) -> bool;

/**
 * @brief Checks if the given string ends with the specified suffix.
 *
 * @param str The string to check.
 * @param suffix The suffix to search for.
 * @return true if the string ends with the suffix, otherwise false.
 * @throws None
 */
[[nodiscard]] auto endsWith(std::string_view str,
                            std::string_view suffix) -> bool;

/**
 * @brief Splits a string into multiple strings.
 *
 * @param str The input string.
 * @param delimiter The delimiter.
 * @return The array of split strings.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of splitString is not used")]]
auto splitString(std::string_view str,
                 char delimiter) -> std::vector<std::string>;

/**
 * @brief Concatenates an array of strings into a single string with a specified
 * delimiter.
 *
 * @param strings The array of strings to concatenate.
 * @param delimiter The delimiter to use for concatenation.
 * @return The concatenated string.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of joinStrings is not used")]]
auto joinStrings(std::span<const std::string_view> strings,
                 std::string_view delimiter) -> std::string;

/**
 * @brief Replaces all occurrences of a substring with another substring in a
 * given text.
 *
 * @param text The text in which replacements will be made.
 * @param oldStr The substring to replace.
 * @param newStr The substring to replace with.
 * @return The text with replacements made.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of replaceString is not used")]]
auto replaceString(std::string_view text, std::string_view oldStr,
                   std::string_view newStr) -> std::string;

/**
 * @brief Replaces multiple substrings with their corresponding replacements in
 * a given text.
 *
 * @param text The text in which replacements will be made.
 * @param replacements A vector of pairs, where each pair represents the
 * substring to replace and its replacement.
 * @return The text with replacements made.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of replaceStrings is not used")]]
auto replaceStrings(
    std::string_view text,
    std::span<const std::pair<std::string_view, std::string_view>> replacements)
    -> std::string;

/**
 * @brief Converts a vector of string_view to a vector of string.
 *
 * @param svv The vector of string_view to convert.
 * @return The converted vector of string.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of SVVtoSV is not used")]]
auto SVVtoSV(std::span<const std::string_view> svv) -> std::vector<std::string>;

/**
 * @brief Explodes a string_view into a vector of string.
 *
 * @param text The string_view to explode.
 * @param symbol The symbol to use for exploding.
 * @return The exploded vector of string.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of explode is not used")]]
auto explode(std::string_view text, char symbol) -> std::vector<std::string>;

/**
 * @brief Trims a string_view.
 *
 * @param line The string_view to trim.
 * @param symbols The symbols to trim.
 * @return The trimmed string.
 * @throws std::bad_alloc If memory allocation fails.
 */
[[nodiscard("the result of trim is not used")]]
auto trim(std::string_view line,
          std::string_view symbols = " \n\r\t") -> std::string;

auto nstrtok(std::string_view& str,
             const std::string_view& delims) -> std::optional<std::string_view>;

auto parallelReplaceString(std::string_view text, std::string_view oldStr,
                           std::string_view newStr,
                           size_t threshold = 10000) -> std::string;

auto parallelSVVtoSV(std::span<const std::string_view> svv,
                     size_t threshold = 1000) -> std::vector<std::string>;

auto toLower(std::string_view str) -> std::string;
auto toUpper(std::string_view str) -> std::string;
/**
 * @brief Converts a string to a wstring.
 *
 * @param str The string to convert.
 * @return The converted wstring.
 * @throws std::bad_alloc If memory allocation fails.
 * @throws std::range_error If the conversion fails.
 */
[[nodiscard("the result of stringToWString is not used")]]
auto stringToWString(std::string_view str) -> std::wstring;

/**
 * @brief Converts a wstring to a string.
 *
 * @param wstr The wstring to convert.
 * @return The converted string.
 * @throws std::bad_alloc If memory allocation fails.
 * @throws std::range_error If the conversion fails.
 */
[[nodiscard("the result of wstringToString is not used")]]
auto wstringToString(std::wstring_view wstr) -> std::string;

/**
 * @brief Converts a string to a double.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @return The converted double.
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range.
 */
[[nodiscard("the result of stod is not used")]]
auto stod(std::string_view str, std::size_t* idx = nullptr) -> double;

/**
 * @brief Converts a string to a float.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @return The converted float.
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range.
 */
[[nodiscard("the result of stof is not used")]]
auto stof(std::string_view str, std::size_t* idx = nullptr) -> float;

/**
 * @brief Converts a string to an integer.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @param base The base of the number (default is 10).
 * @return The converted integer.
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range.
 */
[[nodiscard("the result of stoi is not used")]]
auto stoi(std::string_view str, std::size_t* idx = nullptr,
          int base = 10) -> int;

/**
 * @brief Converts a string to a long integer.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @param base The base of the number (default is 10).
 * @return The converted long integer.
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range.
 */
[[nodiscard("the result of stol is not used")]]
auto stol(std::string_view str, std::size_t* idx = nullptr,
          int base = 10) -> long;

/**
 * @brief Extracts tokens from string, using the delimiter provided.
 *
 * @param str The string to extract tokens from (modified by the function).
 * @param delims The delimiters to use.
 * @return The next token, or std::nullopt if there are no more tokens.
 * @throws None
 */
[[nodiscard("the result of splitTokens is not used")]]
auto splitTokens(std::string_view& str, const std::string_view& delims)
    -> std::optional<std::string_view>;

// Modern C++20 split implementation with concepts
template <DelimiterPredicate Delimiter>
struct SplitString {
    SplitString(std::string_view str, Delimiter delimiter, bool trim = false,
                bool skipEmpty = false)
        : str_(str),
          delimiter_(std::move(delimiter)),
          trim_(trim),
          skipEmpty_(skipEmpty) {}

    struct Sentinel {
        explicit Sentinel() = default;
    };

    struct Iterator {
        explicit Iterator(std::string_view str, Delimiter delimiter, bool trim,
                          bool skipEmpty)
            : str_(str),
              delimiter_(std::move(delimiter)),
              trim_(trim),
              skipEmpty_(skipEmpty),
              ended_(false),
              toBeEnded_(false) {
            findNext();
        }

        auto operator*() const -> std::string_view { return current_; }

        auto operator++() -> Iterator& {
            findNext();
            return *this;
        }

        auto operator!=(const Sentinel&) const -> bool { return !ended_; }

        auto operator==(const Sentinel&) const -> bool { return ended_; }

    private:
        void findNext() {
            do {
                auto pos = findDelimiter();
                if (pos == std::string_view::npos) {
                    current_ = str_;
                    str_ = {};
                    ended_ = toBeEnded_;
                    toBeEnded_ = true;
                } else {
                    current_ = str_.substr(0, pos);
                    str_ = str_.substr(pos + delimiterLength());
                }

                if (trim_) {
                    current_ = trimWhitespace(current_);
                }
            } while (skipEmpty_ && current_.empty() && !ended_);
        }

        [[nodiscard]] auto findDelimiter() const -> size_t {
            if constexpr (std::is_same_v<Delimiter, std::string_view>) {
                return str_.find(delimiter_);
            } else if constexpr (std::is_same_v<Delimiter, char>) {
                return str_.find(delimiter_);
            } else if constexpr (DelimiterPredicate<Delimiter>) {
                for (size_t i = 0; i < str_.size(); ++i) {
                    if (delimiter_(str_[i])) {
                        return i;
                    }
                }
                return std::string_view::npos;
            }
            return std::string_view::npos;
        }

        [[nodiscard]] auto delimiterLength() const -> size_t {
            if constexpr (std::is_same_v<Delimiter, std::string_view>) {
                return delimiter_.size();
            } else if constexpr (std::is_same_v<Delimiter, char>) {
                return 1;
            } else if constexpr (DelimiterPredicate<Delimiter>) {
                return 1;
            }
            return 0;
        }

        [[nodiscard]] auto trimWhitespace(std::string_view sv) const
            -> std::string_view {
            const auto isspace_fn = [](unsigned char c) {
                return std::isspace(c) != 0;
            };
            auto start = std::ranges::find_if_not(sv, isspace_fn);
            if (start == sv.end())
                return {};

            auto end =
                std::ranges::find_if_not(sv | std::views::reverse, isspace_fn)
                    .base();
            return std::string_view(&*start, end - start);
        }

        std::string_view str_;
        Delimiter delimiter_;
        std::string_view current_;
        bool trim_;
        bool skipEmpty_;
        bool ended_;
        bool toBeEnded_;
    };

    [[nodiscard]] auto begin() const -> Iterator {
        return Iterator(str_, delimiter_, trim_, skipEmpty_);
    }

    [[nodiscard]] auto end() const -> Sentinel { return Sentinel(); }

    [[nodiscard]] auto collectVector() const -> std::vector<std::string> {
        std::vector<std::string> result;
        for (auto&& part : *this) {
            result.emplace_back(part);
        }
        return result;
    }

    [[nodiscard]] auto collectList() const -> std::list<std::string> {
        std::list<std::string> result;
        for (auto&& part : *this) {
            result.emplace_back(part);
        }
        return result;
    }

    template <std::size_t N>
    auto collectArray() const -> std::array<std::string, N> {
        std::array<std::string, N> result{};
        std::size_t i = 0;
        for (auto it = begin(); it != end() && i < N; ++it, ++i) {
            result[i] = std::string(*it);
        }
        return result;
    }

private:
    std::string_view str_;
    Delimiter delimiter_;
    bool trim_;
    bool skipEmpty_;
};

/**
 * @brief Splits a string by a given delimiter.
 *
 * @param str The string to split.
 * @param delimiter The delimiter to use.
 * @param trim Whether to trim whitespace from each part.
 * @param skipEmpty Whether to skip empty parts.
 * @return SplitString object that can be iterated over or collected.
 * @throws None
 */
[[nodiscard]] inline auto split(
    std::string_view str, std::string_view delimiter, bool trim = false,
    bool skipEmpty = false) -> SplitString<std::string_view> {
    return SplitString<std::string_view>{str, delimiter, trim, skipEmpty};
}

/**
 * @brief Splits a string by a given character.
 *
 * @param str The string to split.
 * @param delimiter The delimiter to use.
 * @param trim Whether to trim whitespace from each part.
 * @param skipEmpty Whether to skip empty parts.
 * @return SplitString object that can be iterated over or collected.
 * @throws None
 */
[[nodiscard]] inline auto split(std::string_view str, char delimiter,
                                bool trim = false,
                                bool skipEmpty = false) -> SplitString<char> {
    return SplitString<char>{str, delimiter, trim, skipEmpty};
}

/**
 * @brief Splits a string using a predicate function.
 *
 * @param str The string to split.
 * @param delimiter The predicate to determine delimiters.
 * @param trim Whether to trim whitespace from each part.
 * @param skipEmpty Whether to skip empty parts.
 * @return SplitString object that can be iterated over or collected.
 * @throws None
 */
template <DelimiterPredicate Func>
[[nodiscard]] inline auto split(std::string_view str, Func delimiter,
                                bool trim = false,
                                bool skipEmpty = false) -> SplitString<Func> {
    return SplitString<Func>{str, std::move(delimiter), trim, skipEmpty};
}

}  // namespace atom::utils

#endif
