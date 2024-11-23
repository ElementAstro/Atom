/*
 * string.hpp
 *
 * Copyright (C) 2023-2024 Max Qian
 */

/*************************************************

Date: 2024-2-10

Description: A super enhanced string class.

**************************************************/

#ifndef ATOM_TYPE_STRING_HPP
#define ATOM_TYPE_STRING_HPP

#include <algorithm>
#include <cstdarg>
#include <format>
#include <functional>
#include <iostream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "atom/macro.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#endif

/**
 * @brief A super enhanced string class.
 */
class String {
public:
    /**
     * @brief Default constructor.
     */
    String() = default;

    /**
     * @brief Constructor from C-style string.
     */
    String(const char* str)
#ifdef ATOM_USE_BOOST
        : m_data_(str ? str : "")
#else
        : m_data_(str)
#endif
    {
    }

    /**
     * @brief Constructor from std::string_view.
     */
    String(std::string_view str) : m_data_(str) {}

    /**
     * @brief Constructor from std::string.
     */
    String(std::string str) : m_data_(std::move(str)) {}

    /**
     * @brief Copy constructor.
     */
    String(const String& other) = default;

    /**
     * @brief Move constructor.
     */
    String(String&& other) noexcept = default;

    /**
     * @brief Copy assignment.
     */
    auto operator=(const String& other) -> String& = default;

    /**
     * @brief Move assignment.
     */
    auto operator=(String&& other) noexcept -> String& = default;

    /**
     * @brief Equality comparison.
     */
    auto operator==(const String& other) const -> bool = default;

    /**
     * @brief Three-way comparison (C++20).
     */
    auto operator<=>(const String& other) const = default;

    /**
     * @brief Concatenation with another String.
     */
    auto operator+=(const String& other) -> String& {
        m_data_ += other.m_data_;
        return *this;
    }

    /**
     * @brief Concatenation with C-style string.
     */
    auto operator+=(const char* str) -> String& {
        m_data_ += str;
        return *this;
    }

    /**
     * @brief Concatenation with a single character.
     */
    auto operator+=(char c) -> String& {
        m_data_ += c;
        return *this;
    }

    /**
     * @brief Get C-style string.
     */
    [[nodiscard]] auto cStr() const -> const char* { return m_data_.c_str(); }

    /**
     * @brief Get length of the string.
     */
    [[nodiscard]] auto length() const -> size_t { return m_data_.length(); }

    /**
     * @brief Get substring.
     */
    [[nodiscard]] auto substr(
        size_t pos, size_t count = std::string::npos) const -> String {
        return m_data_.substr(pos, count);
    }

    /**
     * @brief Find a substring.
     */
    [[nodiscard]] auto find(const String& str, size_t pos = 0) const -> size_t {
        return m_data_.find(str.m_data_, pos);
    }

    /**
     * @brief Replace first occurrence of oldStr with newStr.
     */
    auto replace(const String& oldStr, const String& newStr) -> bool {
        if (size_t pos = m_data_.find(oldStr.m_data_);
            pos != std::string::npos) {
            m_data_.replace(pos, oldStr.length(), newStr.m_data_);
            return true;
        }
        return false;
    }

    /**
     * @brief Replace all occurrences of oldStr with newStr.
     */
    auto replaceAll(const String& oldStr, const String& newStr) -> size_t {
        size_t count = 0;
        size_t pos = 0;

        while ((pos = m_data_.find(oldStr.m_data_, pos)) != std::string::npos) {
            m_data_.replace(pos, oldStr.length(), newStr.m_data_);
            pos += newStr.length();
            ++count;
        }

        return count;
    }

    /**
     * @brief Convert string to uppercase.
     */
    [[nodiscard]] auto toUpper() const -> String {
        String result;
#ifdef ATOM_USE_BOOST
        result.m_data_ = boost::to_upper_copy(m_data_);
#else
        std::transform(m_data_.begin(), m_data_.end(),
                       std::back_inserter(result.m_data_),
                       [](unsigned char c) { return std::toupper(c); });
#endif
        return result;
    }

    /**
     * @brief Convert string to lowercase.
     */
    [[nodiscard]] auto toLower() const -> String {
        String result;
#ifdef ATOM_USE_BOOST
        result.m_data_ = boost::to_lower_copy(m_data_);
#else
        std::transform(m_data_.begin(), m_data_.end(),
                       std::back_inserter(result.m_data_),
                       [](unsigned char c) { return std::tolower(c); });
#endif
        return result;
    }

    /**
     * @brief Split the string by a delimiter.
     */
    [[nodiscard]] auto split(const String& delimiter) const
        -> std::vector<String> {
        if (delimiter.empty()) {
            return {*this};
        }
        if (m_data_.empty()) {
            return {};
        }
        std::vector<String> tokens;
#ifdef ATOM_USE_BOOST
        std::vector<std::string> temp_tokens;
        boost::split(temp_tokens, m_data_, boost::is_any_of(delimiter.m_data_),
                     boost::token_compress_on);
        std::transform(temp_tokens.begin(), temp_tokens.end(),
                       std::back_inserter(tokens),
                       [](const std::string& s) { return String(s); });
#else
        size_t start = 0;
        size_t end = m_data_.find(delimiter.m_data_);

        while (end != std::string::npos) {
            tokens.emplace_back(substr(start, end - start));
            start = end + delimiter.length();
            end = m_data_.find(delimiter.m_data_, start);
        }

        tokens.emplace_back(substr(start));
#endif
        return tokens;
    }

    /**
     * @brief Join a vector of strings with a separator.
     */
    static auto join(const std::vector<String>& strings,
                     const String& separator) -> String {
#ifdef ATOM_USE_BOOST
        std::vector<std::string> temp_strings;
        temp_strings.reserve(strings.size());
        for (const auto& s : strings) {
            temp_strings.emplace_back(s.m_data_);
        }
        return String(boost::algorithm::join(temp_strings, separator.m_data_));
#else
        String result;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) {
                result += separator;
            }
            result += strings[i];
        }
        return result;
#endif
    }

    /**
     * @brief Trim whitespace from both ends.
     */
    void trim() {
#ifdef ATOM_USE_BOOST
        boost::trim(m_data_);
#else
        ltrim();
        rtrim();
#endif
    }

    /**
     * @brief Left trim.
     */
    void ltrim() {
#ifdef ATOM_USE_BOOST
        boost::trim_left(m_data_);
#else
        m_data_.erase(
            m_data_.begin(),
            std::find_if(m_data_.begin(), m_data_.end(),
                         [](unsigned char c) { return !std::isspace(c); }));
#endif
    }

    /**
     * @brief Right trim.
     */
    void rtrim() {
#ifdef ATOM_USE_BOOST
        boost::trim_right(m_data_);
#else
        m_data_.erase(
            std::find_if(m_data_.rbegin(), m_data_.rend(),
                         [](unsigned char c) { return !std::isspace(c); })
                .base(),
            m_data_.end());
#endif
    }

    /**
     * @brief Reverse the string.
     */
    [[nodiscard]] auto reverse() const -> String {
        String result(m_data_);
        std::reverse(result.m_data_.begin(), result.m_data_.end());
        return result;
    }

    /**
     * @brief Case-insensitive comparison.
     */
    [[nodiscard]] auto equalsIgnoreCase(const String& other) const -> bool {
#ifdef ATOM_USE_BOOST
        return boost::iequals(m_data_, other.m_data_);
#else
        return std::equal(m_data_.begin(), m_data_.end(), other.m_data_.begin(),
                          other.m_data_.end(), [](char a, char b) {
                              return std::tolower(a) == std::tolower(b);
                          });
#endif
    }

    /**
     * @brief Check if string starts with a prefix.
     */
    [[nodiscard]] auto startsWith(const String& prefix) const -> bool {
#ifdef ATOM_USE_BOOST
        return boost::starts_with(m_data_, prefix.m_data_);
#else
        return m_data_.starts_with(prefix.m_data_);
#endif
    }

    /**
     * @brief Check if string ends with a suffix.
     */
    [[nodiscard]] auto endsWith(const String& suffix) const -> bool {
#ifdef ATOM_USE_BOOST
        return boost::ends_with(m_data_, suffix.m_data_);
#else
        return m_data_.ends_with(suffix.m_data_);
#endif
    }

    /**
     * @brief Check if the string contains a substring.
     */
    [[nodiscard]] auto contains(const String& str) const -> bool {
#ifdef ATOM_USE_BOOST
        return boost::contains(m_data_, str.m_data_);
#else
        return m_data_.find(str.m_data_) != std::string::npos;
#endif
    }

    /**
     * @brief Check if the string contains a specific character.
     */
    [[nodiscard]] auto contains(char c) const -> bool {
        return m_data_.find(c) != std::string::npos;
    }

    /**
     * @brief Replace all occurrences of a character with another character.
     */
    auto replace(char oldChar, char newChar) -> size_t {
        size_t count = 0;
#ifdef ATOM_USE_BOOST
        std::string oldStr(1, oldChar);
        std::string newStr(1, newChar);
        count = boost::algorithm::replace_all(m_data_, oldStr, newStr);
#else
        for (auto& c : m_data_) {
            if (c == oldChar) {
                c = newChar;
                ++count;
            }
        }
#endif
        return count;
    }

    /**
     * @brief Insert a character at a specific position.
     */
    auto insert(size_t pos, char ch) -> String& {
        m_data_.insert(pos, 1, ch);
        return *this;
    }

    /**
     * @brief Insert a string at a specific position.
     */
    auto insert(size_t pos, const String& str) -> String& {
        m_data_.insert(pos, str.m_data_);
        return *this;
    }

    /**
     * @brief Remove a specific character from the string.
     */
    auto remove(char ch) -> size_t {
        size_t count = 0;
#ifdef ATOM_USE_BOOST
        auto originalSize = m_data_.size();
        m_data_.erase(std::remove(m_data_.begin(), m_data_.end(), ch),
                      m_data_.end());
        count = originalSize - m_data_.size();
#else
        count = std::erase(m_data_, ch);
#endif
        return count;
    }

    /**
     * @brief Remove all occurrences of a substring.
     */
    auto removeAll(const String& str) -> size_t {
        size_t count = 0;
        size_t pos = 0;

        while ((pos = m_data_.find(str.m_data_, pos)) != std::string::npos) {
            m_data_.erase(pos, str.length());
            ++count;
        }

        return count;
    }

    /**
     * @brief Erase a specific range of characters.
     */
    auto erase(size_t pos, size_t count = std::string::npos) -> String& {
        m_data_.erase(pos, count);
        return *this;
    }
    
    /**
     * @brief Get the underlying data as a std::string.
     */
    [[nodiscard]] auto data() const -> std::string { return m_data_; }

    /**
     * @brief Check if the string is empty.
     */
    [[nodiscard]] auto empty() const -> bool { return m_data_.empty(); }

    /**
     * @brief Pad the string from the left with a specific character.
     */
    auto padLeft(size_t totalLength, char paddingChar = ' ') -> String& {
        if (m_data_.length() < totalLength) {
            m_data_.insert(m_data_.begin(), totalLength - m_data_.length(),
                           paddingChar);
        }
        return *this;
    }

    /**
     * @brief Pad the string from the right with a specific character.
     */
    auto padRight(size_t totalLength, char paddingChar = ' ') -> String& {
        if (m_data_.length() < totalLength) {
            m_data_.append(totalLength - m_data_.length(), paddingChar);
        }
        return *this;
    }

    /**
     * @brief Remove a specific prefix from the string.
     */
    auto removePrefix(const String& prefix) -> bool {
        if (startsWith(prefix)) {
            m_data_.erase(0, prefix.length());
            return true;
        }
        return false;
    }

    /**
     * @brief Remove a specific suffix from the string.
     */
    auto removeSuffix(const String& suffix) -> bool {
        if (endsWith(suffix)) {
            m_data_.erase(m_data_.length() - suffix.length());
            return true;
        }
        return false;
    }

    /**
     * @brief Compress multiple consecutive spaces into a single space.
     */
    void compressSpaces() {
        auto newEnd =
            std::unique(m_data_.begin(), m_data_.end(), [](char lhs, char rhs) {
                return (std::isspace(lhs) != 0) && (std::isspace(rhs) != 0);
            });
        m_data_.erase(newEnd, m_data_.end());
    }

    /**
     * @brief Reverse the order of words in the string.
     */
    ATOM_NODISCARD auto reverseWords() const -> String {
        auto words = split(" ");
        std::ranges::reverse(words);
        return join(words, " ");
    }

    auto replaceRegex(const std::string& pattern,
                      const std::string& replacement) -> String {
#ifdef ATOM_USE_BOOST
        boost::regex rex(pattern);
        return boost::regex_replace(m_data_, rex, replacement);
#else
        std::regex rex(pattern);
        return std::regex_replace(m_data_, rex, replacement);
#endif
    }

    /**
     * @brief Format a string.
     */
    template <typename... Args>
    static auto format(std::string_view format_str,
                       Args&&... args) -> std::string {
        return std::vformat(format_str, std::make_format_args(args...));
    }

    static constexpr size_t NPOS = std::string::npos;

private:
    std::string m_data_;
};

/**
 * @brief Concatenation operator for String class.
 */
inline auto operator+(const String& lhs, const String& rhs) -> String {
    String result(lhs);
    result += rhs;
    return result;
}

/**
 * @brief Output stream operator for String class.
 */
inline auto operator<<(std::ostream& os, const String& str) -> std::ostream& {
    os << str.data();
    return os;
}

#ifdef ATOM_USE_BOOST
/**
 * @brief Specialization of std::hash for String class using Boost.
 */
namespace std {
template <>
struct hash<String> {
    size_t operator()(const String& str) const noexcept {
        return boost::hash_value(str.data());
    }
};
}  // namespace std
#else
/**
 * @brief Specialization of std::hash for String class.
 */
namespace std {
template <>
struct hash<String> {
    size_t operator()(const String& str) const noexcept {
        return std::hash<std::string>()(str.data());
    }
};
}  // namespace std
#endif

#endif  // ATOM_TYPE_STRING_HPP