/*
 * string.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Some useful string functions

**************************************************/

#ifndef ATOM_UTILS_STRING_HPP
#define ATOM_UTILS_STRING_HPP

#include <list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atom::utils {
/**
 * @brief Checks if the given string contains any uppercase characters.
 *
 * @param str The string to check.
 * @return true if the string contains uppercase characters, otherwise false.
 */
[[nodiscard]] auto hasUppercase(std::string_view str) -> bool;

/**
 * @brief Converts the given string to snake_case format.
 *
 * @param str The string to convert.
 * @return The string converted to snake_case.
 */
[[nodiscard]] auto toUnderscore(std::string_view str) -> std::string;

/**
 * @brief Converts the given string to camelCase format.
 *
 * @param str The string to convert.
 * @return The string converted to camelCase.
 */
[[nodiscard]] auto toCamelCase(std::string_view str) -> std::string;

/**
 * @brief Encodes the given string using URL encoding.
 *
 * @param str The string to encode.
 * @return The URL encoded string.
 */
[[nodiscard]] auto urlEncode(std::string_view str) -> std::string;

/**
 * @brief Decodes the given URL encoded string.
 *
 * @param str The URL encoded string to decode.
 * @return The decoded string.
 */
[[nodiscard]] auto urlDecode(std::string_view str) -> std::string;

/**
 * @brief Checks if the given string starts with the specified prefix.
 *
 * @param str The string to check.
 * @param prefix The prefix to search for.
 * @return true if the string starts with the prefix, otherwise false.
 */
[[nodiscard]] auto startsWith(std::string_view str,
                              std::string_view prefix) -> bool;

/**
 * @brief Checks if the given string ends with the specified suffix.
 *
 * @param str The string to check.
 * @param suffix The suffix to search for.
 * @return true if the string ends with the suffix, otherwise false.
 */
[[nodiscard]] auto endsWith(std::string_view str,
                            std::string_view suffix) -> bool;

/**
 * @brief 将字符串分割为多个字符串。
 * @param input 输入字符串。
 * @param delimiter 分隔符。
 * @return 分割后的字符串数组。
 */
[[nodiscard("the result of splitString is not used")]] auto splitString(
    const std::string& str, char delimiter) -> std::vector<std::string>;

/**
 * @brief Concatenates an array of strings into a single string with a specified
 * delimiter.
 *
 * @param strings The array of strings to concatenate.
 * @param delimiter The delimiter to use for concatenation.
 * @return The concatenated string.
 */
[[nodiscard("the result of joinStrings is not used")]] auto joinStrings(
    const std::vector<std::string_view>& strings,
    const std::string_view& delimiter) -> std::string;

/**
 * @brief Replaces all occurrences of a substring with another substring in a
 * given text.
 *
 * @param text The text in which replacements will be made.
 * @param oldStr The substring to replace.
 * @param newStr The substring to replace with.
 * @return The text with replacements made.
 */
[[nodiscard("the result of replaceString is not used")]] auto replaceString(
    std::string_view text, std::string_view oldStr,
    std::string_view newStr) -> std::string;

/**
 * @brief Replaces multiple substrings with their corresponding replacements in
 * a given text.
 *
 * @param text The text in which replacements will be made.
 * @param replacements A vector of pairs, where each pair represents the
 * substring to replace and its replacement.
 * @return The text with replacements made.
 */
[[nodiscard("the result of replaceStrings is not used")]] auto replaceStrings(
    std::string_view text,
    const std::vector<std::pair<std::string_view, std::string_view>>&
        replacements) -> std::string;

/**
 * @brief Converts a vector of string_view to a vector of string.
 *
 * @param svv The vector of string_view to convert.
 * @return The converted vector of string.
 */
[[nodiscard("the result of SVVtoSV is not used")]]
auto SVVtoSV(const std::vector<std::string_view>& svv)
    -> std::vector<std::string>;

/**
 * @brief Explodes a string_view into a vector of string_view.
 *
 * @param text The string_view to explode.
 * @param symbol The symbol to use for exploding.
 * @return The exploded vector of string_view.
 */
[[nodiscard("the result of explode is not used")]]
auto explode(std::string_view text, char symbol) -> std::vector<std::string>;

/**
 * @brief Trims a string_view.
 *
 * @param line The string_view to trim.
 * @param symbols The symbols to trim.
 * @return The trimmed string_view.
 */
[[nodiscard("the result of trim is not used")]]
auto trim(std::string_view line,
          std::string_view symbols = " \n\r\t") -> std::string;

/**
 * @brief Converts a u8string to a wstring.
 *
 * @param u8str The u8string to convert.
 * @return The converted wstring.
 */
[[nodiscard("the result of stringToWString is not used")]]
auto stringToWString(const std::string& str) -> std::wstring;

/**
 * @brief Converts a wstring to a u8string.
 *
 * @param wstr The wstring to convert.
 * @return The converted u8string.
 */
[[nodiscard("the result of wstringToString is not used")]]
auto wstringToString(const std::wstring& wstr) -> std::string;

/**
 * @brief Converts a string to a long integer.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @param base The base of the number (default is 10).
 * @return The converted long integer.
 */
[[nodiscard("the result of stol is not used")]]
auto stod(std::string_view str, std::size_t* idx = nullptr) -> double;

/**
 * @brief Converts a string to a float.
 *
 * @param str The string to convert.
 * @param idx A pointer to the index of the first character after the number.
 * @return The converted float.
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
 */
[[nodiscard("the result of stol is not used")]]
auto stol(std::string_view str, std::size_t* idx = nullptr,
          int base = 10) -> long;

/**
 * @brief Splits a string into multiple strings.
 *
 * @param str The input string.
 * @param delimiter The delimiter.
 * @return The array of split strings.
 */
[[nodiscard("the result of nstrtok is not used")]]
auto nstrtok(std::string_view& str,
             const std::string_view& delims) -> std::optional<std::string_view>;

template <class Delimiter>
struct SplitString {
    SplitString(std::string_view str, Delimiter delimiter, bool trim = false,
                bool skipEmpty = false)
        : str_(str),
          delimiter_(delimiter),
          trim_(trim),
          skipEmpty_(skipEmpty) {}

    struct Sentinel {
        explicit Sentinel() = default;
    };

    struct Iterator {
        explicit Iterator(std::string_view str, Delimiter delimiter, bool trim,
                          bool skipEmpty) noexcept
            : str_(str),
              delimiter_(delimiter),
              trim_(trim),
              skipEmpty_(skipEmpty),
              ended_(false),
              toBeEnded_(false) {
            findNext();
        }

        auto operator*() const noexcept -> std::string_view { return current_; }

        auto operator++() -> Iterator& {
            findNext();
            return *this;
        }

        auto operator!=(Sentinel) const noexcept -> bool { return !ended_; }

        auto operator==(Sentinel) const noexcept -> bool { return ended_; }

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
            } else if constexpr (std::is_invocable_r_v<bool, Delimiter, char>) {
                for (size_t i = 0; i < str_.size(); ++i) {
                    if (delimiter_(str_[i])) {
                        return i;
                    }
                }
                return std::string_view::npos;
            } else {
                static_assert(!std::is_void_v<Delimiter>,
                              "Delimiter cannot be void");
            }
            return std::string_view::npos;
        }

        [[nodiscard]] auto delimiterLength() const -> size_t {
            if constexpr (std::is_same_v<Delimiter, std::string_view>) {
                return delimiter_.size();
            } else if constexpr (std::is_same_v<Delimiter, char>) {
                return 1;
            } else if constexpr (std::is_invocable_r_v<bool, Delimiter, char>) {
                return 1;
            } else {
                static_assert(!std::is_void_v<Delimiter>,
                              "Delimiter cannot be void");
            }
            return 0;
        }

        [[nodiscard]] auto trimWhitespace(std::string_view sv) const
            -> std::string_view {
            size_t start = 0;
            while (start < sv.size() &&
                   (std::isspace(static_cast<unsigned char>(sv[start])) != 0)) {
                ++start;
            }
            size_t end = sv.size();
            while (end > start && (std::isspace(static_cast<unsigned char>(
                                       sv[end - 1])) != 0)) {
                --end;
            }
            return sv.substr(start, end - start);
        }

        std::string_view str_;
        Delimiter delimiter_;
        std::string_view current_;
        bool trim_;
        bool skipEmpty_;
        bool ended_;
        bool toBeEnded_;
    };

    [[nodiscard]] auto begin() const noexcept -> Iterator {
        return Iterator(str_, delimiter_, trim_, skipEmpty_);
    }

    [[nodiscard]] auto end() const noexcept -> Sentinel { return Sentinel(); }

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
        for (auto it = begin(); it != end(); ++it, ++i) {
            if (i >= N) {
                break;
            }
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

inline auto split(std::string_view str, std::string_view delimiter,
                        bool trim = false, bool skipEmpty = false)
    -> SplitString<std::string_view> {
    return {str, delimiter, trim, skipEmpty};
}

inline auto split(std::string_view str, char delimiter, bool trim = false,
                        bool skipEmpty = false) -> SplitString<char> {
    return {str, delimiter, trim, skipEmpty};
}

template <typename Func>
inline auto split(std::string_view str, Func delimiter, bool trim = false,
                        bool skipEmpty = false) -> SplitString<Func> {
    return {str, delimiter, trim, skipEmpty};
}
}  // namespace atom::utils

#endif
