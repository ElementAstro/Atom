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
#include <execution>
#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef ATOM_USE_SIMD
#include "../utils/simd_wrapper.hpp"
#endif

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <boost/regex.hpp>
#endif

/**
 * @brief Custom exception class for String operations
 */
class StringException : public std::runtime_error {
public:
    explicit StringException(const std::string& message)
        : std::runtime_error(message) {}
    explicit StringException(const char* message)
        : std::runtime_error(message) {}
};

/**
 * @brief A super enhanced string class with comprehensive functionality.
 */
class String {
public:
    /**
     * @brief Default constructor.
     */
    constexpr String() noexcept = default;

    /**
     * @brief Constructor from C-style string.
     * @param str C-style string (null-terminated)
     * @throws StringException if memory allocation fails
     */
    explicit String(const char* str) noexcept(false) : m_data_(str ? str : "") {
        try {
            if (str == nullptr) {
                m_data_.clear();
            }
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed in String constructor");
        }
    }

    /**
     * @brief Constructor from std::string_view.
     * @param str String view to initialize from
     * @throws StringException if memory allocation fails
     */
    explicit String(std::string_view str) noexcept(false) {
        try {
            m_data_ = std::string(str);
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed in String constructor");
        }
    }

    /**
     * @brief Constructor from std::string.
     * @param str String to initialize from
     * @throws StringException if memory allocation fails
     */
    explicit String(std::string str) noexcept(false) {
        try {
            m_data_ = std::move(str);
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed in String constructor");
        }
    }

    String(const String& other) = default;
    String(String&& other) noexcept = default;
    String& operator=(const String& other) = default;
    String& operator=(String&& other) noexcept = default;
    ~String() = default;

    /**
     * @brief Equality comparison operator.
     */
    auto operator==(const String& other) const noexcept -> bool {
        return m_data_ == other.m_data_;
    }

    /**
     * @brief Three-way comparison operator (C++20).
     */
    auto operator<=>(const String& other) const noexcept {
        return m_data_ <=> other.m_data_;
    }

    /**
     * @brief Concatenation with another String.
     * @throws StringException if memory allocation fails
     */
    auto operator+=(const String& other) -> String& {
        try {
            m_data_ += other.m_data_;
            return *this;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during string concatenation");
        }
    }

    /**
     * @brief Concatenation with C-style string.
     * @throws StringException if memory allocation fails or if str is nullptr
     */
    auto operator+=(const char* str) -> String& {
        if (str == nullptr) {
            throw StringException("Attempted to concatenate with nullptr");
        }
        try {
            m_data_ += str;
            return *this;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during string concatenation");
        }
    }

    /**
     * @brief Concatenation with a single character.
     * @throws StringException if memory allocation fails
     */
    auto operator+=(char c) -> String& {
        try {
            m_data_ += c;
            return *this;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during character concatenation");
        }
    }

    /**
     * @brief Get C-style string.
     * @return Pointer to null-terminated string
     */
    [[nodiscard]] auto cStr() const noexcept -> const char* {
        return m_data_.c_str();
    }

    /**
     * @brief Get length of the string.
     * @return Number of characters in the string
     */
    [[nodiscard]] auto length() const noexcept -> size_t {
        return m_data_.length();
    }

    /**
     * @brief Get size of the string (alias for length).
     * @return Number of characters in the string
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        return m_data_.size();
    }

    /**
     * @brief Get capacity of the underlying string.
     * @return Current capacity of the string
     */
    [[nodiscard]] auto capacity() const noexcept -> size_t {
        return m_data_.capacity();
    }

    /**
     * @brief Reserve memory for the string.
     * @param newCapacity The new capacity to reserve
     * @throws StringException if memory allocation fails
     */
    void reserve(size_t newCapacity) {
        try {
            m_data_.reserve(newCapacity);
        } catch (const std::length_error&) {
            throw StringException(
                "Requested capacity exceeds maximum string size");
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during reserve operation");
        }
    }

    /**
     * @brief Get substring.
     * @param pos Starting position
     * @param count Number of characters to include
     * @return Substring
     * @throws StringException if pos is out of range
     */
    [[nodiscard]] auto substr(size_t pos,
                              size_t count = std::string::npos) const
        -> String {
        try {
            if (pos > m_data_.length()) {
                throw StringException("Substring position out of range");
            }
            return String(m_data_.substr(pos, count));
        } catch (const std::out_of_range&) {
            throw StringException("Substring position out of range");
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during substring operation");
        }
    }

    /**
     * @brief Find a substring.
     * @param str String to find
     * @param pos Position to start searching from
     * @return Position of the found substring or NPOS if not found
     */
    [[nodiscard]] auto find(const String& str, size_t pos = 0) const noexcept
        -> size_t {
        if (pos >= m_data_.length() || str.empty()) {
            return NPOS;
        }
        return m_data_.find(str.m_data_, pos);
    }

    /**
     * @brief Find a substring with SIMD acceleration for large strings if
     * available.
     * @param str String to find
     * @param pos Position to start searching from
     * @return Position of the found substring or NPOS if not found
     */
    [[nodiscard]] auto findOptimized(const String& str,
                                     size_t pos = 0) const noexcept -> size_t {
        if (m_data_.length() < 64 || str.length() < 2 ||
            pos >= m_data_.length()) {
            return find(str, pos);
        }

#ifdef ATOM_USE_SIMD
        const char* haystack = m_data_.c_str() + pos;
        const char* needle = str.cStr();
        const size_t haystackLen = m_data_.length() - pos;
        const size_t needleLen = str.length();

#if defined(SIMD_HAS_AVX2) && SIMD_HAS_AVX2
        simd::int8x16_t firstCharVec = simd::Vec<int8_t, 16>(needle[0]);
        const char* current = haystack;
        const char* end = haystack + haystackLen - needleLen + 1;

        while (current < end) {
            simd::int8x16_t haystackVec = simd::Vec<int8_t, 16>::loadu(
                reinterpret_cast<const int8_t*>(current));
            auto mask = haystackVec == firstCharVec;
            auto bitmask = simd::VecTraits<int8_t, 16>::mask_to_vector(mask);
            unsigned int matches = static_cast<unsigned int>(
                simd::Vec<int8_t, 16>(bitmask).horizontal_sum());

            if (matches) {
                for (int i = 0; i < 16 && current + i < end; ++i) {
                    if (current[i] == needle[0]) {
                        if (std::memcmp(current + i + 1, needle + 1,
                                        needleLen - 1) == 0) {
                            return (current - haystack) + pos + i;
                        }
                    }
                }
            }
            current += 16;
        }

        while (current <= haystack + haystackLen - needleLen) {
            if (std::memcmp(current, needle, needleLen) == 0) {
                return (current - haystack) + pos;
            }
            ++current;
        }
        return NPOS;
#endif
#endif
        return m_data_.find(str.m_data_, pos);
    }

    /**
     * @brief Replace first occurrence of oldStr with newStr.
     * @param oldStr String to replace
     * @param newStr Replacement string
     * @return True if replacement was made, false otherwise
     * @throws StringException if memory allocation fails
     */
    auto replace(const String& oldStr, const String& newStr) -> bool {
        if (oldStr.empty()) {
            return false;
        }
        try {
            if (size_t pos = m_data_.find(oldStr.m_data_);
                pos != std::string::npos) {
                m_data_.replace(pos, oldStr.length(), newStr.m_data_);
                return true;
            }
            return false;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during replace operation");
        }
    }

    /**
     * @brief Replace all occurrences of oldStr with newStr.
     * @param oldStr String to replace
     * @param newStr Replacement string
     * @return Number of replacements made
     * @throws StringException if memory allocation fails or if oldStr is empty
     */
    auto replaceAll(const String& oldStr, const String& newStr) -> size_t {
        if (oldStr.empty()) {
            throw StringException("Cannot replace empty string");
        }
        try {
            size_t count = 0;
            size_t pos = 0;
            const size_t oldLen = oldStr.length();
            const size_t newLen = newStr.length();

            if (newLen > oldLen) {
                size_t estimatedGrowth = 0;
                size_t tempPos = 0;
                while ((tempPos = m_data_.find(oldStr.m_data_, tempPos)) !=
                       std::string::npos) {
                    estimatedGrowth += (newLen - oldLen);
                    tempPos += oldLen;
                }
                if (estimatedGrowth > 0) {
                    m_data_.reserve(m_data_.length() + estimatedGrowth);
                }
            }

            while ((pos = m_data_.find(oldStr.m_data_, pos)) !=
                   std::string::npos) {
                m_data_.replace(pos, oldLen, newStr.m_data_);
                pos += newLen;
                ++count;
            }
            return count;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during replaceAll operation");
        }
    }

    /**
     * @brief Convert string to uppercase.
     * @return New string with all characters converted to uppercase
     * @throws StringException if memory allocation fails
     */
    [[nodiscard]] auto toUpper() const -> String {
        try {
            String result;
            result.m_data_.reserve(m_data_.length());
#ifdef ATOM_USE_BOOST
            result.m_data_ = boost::to_upper_copy(m_data_);
#else
            std::transform(std::execution::par_unseq, m_data_.begin(),
                           m_data_.end(), std::back_inserter(result.m_data_),
                           [](unsigned char c) { return std::toupper(c); });
#endif
            return result;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during toUpper operation");
        }
    }

    /**
     * @brief Convert string to lowercase.
     * @return New string with all characters converted to lowercase
     * @throws StringException if memory allocation fails
     */
    [[nodiscard]] auto toLower() const -> String {
        try {
            String result;
            result.m_data_.reserve(m_data_.length());
#ifdef ATOM_USE_BOOST
            result.m_data_ = boost::to_lower_copy(m_data_);
#else
            std::transform(std::execution::par_unseq, m_data_.begin(),
                           m_data_.end(), std::back_inserter(result.m_data_),
                           [](unsigned char c) { return std::tolower(c); });
#endif
            return result;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during toLower operation");
        }
    }

    /**
     * @brief Split the string by a delimiter.
     * @param delimiter String to split by
     * @return Vector of substrings
     * @throws StringException if memory allocation fails
     */
    [[nodiscard]] auto split(const String& delimiter) const
        -> std::vector<String> {
        try {
            if (delimiter.empty()) {
                return {*this};
            }
            if (m_data_.empty()) {
                return {};
            }

            std::vector<String> tokens;
#ifdef ATOM_USE_BOOST
            std::vector<std::string> temp_tokens;
            boost::split(temp_tokens, m_data_,
                         boost::is_any_of(delimiter.m_data_),
                         boost::token_compress_on);
            tokens.reserve(temp_tokens.size());
            std::transform(temp_tokens.begin(), temp_tokens.end(),
                           std::back_inserter(tokens),
                           [](const std::string& s) { return String(s); });
#else
            size_t count = 1;
            size_t pos = 0;
            while ((pos = m_data_.find(delimiter.m_data_, pos)) !=
                   std::string::npos) {
                ++count;
                pos += delimiter.length();
            }
            tokens.reserve(count);

            size_t start = 0;
            size_t end = m_data_.find(delimiter.m_data_);
            while (end != std::string::npos) {
                tokens.push_back(substr(start, end - start));
                start = end + delimiter.length();
                end = m_data_.find(delimiter.m_data_, start);
            }
            tokens.push_back(substr(start));
#endif
            return tokens;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during split operation");
        }
    }

    /**
     * @brief Join a vector of strings with a separator.
     * @param strings Vector of strings to join
     * @param separator String to insert between elements
     * @return Joined string
     * @throws StringException if memory allocation fails
     */
    static auto join(const std::vector<String>& strings,
                     const String& separator) -> String {
        try {
            if (strings.empty()) {
                return String();
            }

            size_t totalSize = 0;
            for (const auto& s : strings) {
                totalSize += s.length();
            }
            totalSize += separator.length() * (strings.size() - 1);

            String result;
            result.m_data_.reserve(totalSize);

#ifdef ATOM_USE_BOOST
            std::vector<std::string> temp_strings;
            temp_strings.reserve(strings.size());
            for (const auto& s : strings) {
                temp_strings.emplace_back(s.m_data_);
            }
            return String(
                boost::algorithm::join(temp_strings, separator.m_data_));
#else
            for (size_t i = 0; i < strings.size(); ++i) {
                if (i > 0) {
                    result.m_data_ += separator.m_data_;
                }
                result.m_data_ += strings[i].m_data_;
            }
            return result;
#endif
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during join operation");
        }
    }

    /**
     * @brief Trim whitespace from both ends.
     * @throws StringException if memory allocation fails
     */
    void trim() {
        try {
#ifdef ATOM_USE_BOOST
            boost::trim(m_data_);
#else
            ltrim();
            rtrim();
#endif
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during trim operation");
        }
    }

    /**
     * @brief Left trim.
     * @throws StringException if memory allocation fails
     */
    void ltrim() {
        try {
#ifdef ATOM_USE_BOOST
            boost::trim_left(m_data_);
#else
            m_data_.erase(
                m_data_.begin(),
                std::find_if(m_data_.begin(), m_data_.end(),
                             [](unsigned char c) { return !std::isspace(c); }));
#endif
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during ltrim operation");
        }
    }

    /**
     * @brief Right trim.
     * @throws StringException if memory allocation fails
     */
    void rtrim() {
        try {
#ifdef ATOM_USE_BOOST
            boost::trim_right(m_data_);
#else
            m_data_.erase(
                std::find_if(m_data_.rbegin(), m_data_.rend(),
                             [](unsigned char c) { return !std::isspace(c); })
                    .base(),
                m_data_.end());
#endif
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during rtrim operation");
        }
    }

    /**
     * @brief Reverse the string.
     * @return New string with reversed characters
     * @throws StringException if memory allocation fails
     */
    [[nodiscard]] auto reverse() const -> String {
        try {
            String result(m_data_);
            std::reverse(result.m_data_.begin(), result.m_data_.end());
            return result;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during reverse operation");
        }
    }

    /**
     * @brief Case-insensitive comparison.
     * @param other String to compare with
     * @return True if strings are equal ignoring case
     */
    [[nodiscard]] auto equalsIgnoreCase(const String& other) const noexcept
        -> bool {
        if (m_data_.length() != other.m_data_.length()) {
            return false;
        }
#ifdef ATOM_USE_BOOST
        return boost::iequals(m_data_, other.m_data_);
#else
        return std::equal(
            m_data_.begin(), m_data_.end(), other.m_data_.begin(),
            other.m_data_.end(), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) ==
                       std::tolower(static_cast<unsigned char>(b));
            });
#endif
    }

    /**
     * @brief Check if string starts with a prefix.
     * @param prefix Prefix to check
     * @return True if string starts with the prefix
     */
    [[nodiscard]] auto startsWith(const String& prefix) const noexcept -> bool {
        if (prefix.length() > m_data_.length()) {
            return false;
        }
#ifdef ATOM_USE_BOOST
        return boost::starts_with(m_data_, prefix.m_data_);
#else
        return m_data_.starts_with(prefix.m_data_);
#endif
    }

    /**
     * @brief Check if string ends with a suffix.
     * @param suffix Suffix to check
     * @return True if string ends with the suffix
     */
    [[nodiscard]] auto endsWith(const String& suffix) const noexcept -> bool {
        if (suffix.length() > m_data_.length()) {
            return false;
        }
#ifdef ATOM_USE_BOOST
        return boost::ends_with(m_data_, suffix.m_data_);
#else
        return m_data_.ends_with(suffix.m_data_);
#endif
    }

    /**
     * @brief Check if the string contains a substring.
     * @param str Substring to check
     * @return True if string contains the substring
     */
    [[nodiscard]] auto contains(const String& str) const noexcept -> bool {
        if (str.empty() || m_data_.empty()) {
            return str.empty();
        }
#ifdef ATOM_USE_BOOST
        return boost::contains(m_data_, str.m_data_);
#else
        return m_data_.find(str.m_data_) != std::string::npos;
#endif
    }

    /**
     * @brief Check if the string contains a specific character.
     * @param c Character to check
     * @return True if string contains the character
     */
    [[nodiscard]] auto contains(char c) const noexcept -> bool {
        return m_data_.find(c) != std::string::npos;
    }

    /**
     * @brief Replace all occurrences of a character with another character.
     * @param oldChar Character to replace
     * @param newChar Replacement character
     * @return Number of replacements made
     */
    auto replace(char oldChar, char newChar) noexcept -> size_t {
        size_t count = 0;
        for (auto& c : m_data_) {
            if (c == oldChar) {
                c = newChar;
                ++count;
            }
        }
        return count;
    }

    /**
     * @brief Insert a character at a specific position.
     * @param pos Position to insert at
     * @param ch Character to insert
     * @return Reference to this String
     * @throws std::out_of_range if pos is out of range
     * @throws StringException if memory allocation fails
     */
    auto insert(size_t pos, char ch) -> String& {
        try {
            if (pos > m_data_.length()) {
                throw std::out_of_range("Insert position out of range");
            }
            m_data_.insert(pos, 1, ch);
            return *this;
        } catch (const std::out_of_range& e) {
            throw StringException(std::string("Invalid position: ") + e.what());
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during insert operation");
        }
    }

    /**
     * @brief Insert a string at a specific position.
     * @param pos Position to insert at
     * @param str String to insert
     * @return Reference to this String
     * @throws std::out_of_range if pos is out of range
     * @throws StringException if memory allocation fails
     */
    auto insert(size_t pos, const String& str) -> String& {
        try {
            if (pos > m_data_.length()) {
                throw std::out_of_range("Insert position out of range");
            }
            m_data_.insert(pos, str.m_data_);
            return *this;
        } catch (const std::out_of_range& e) {
            throw StringException(std::string("Invalid position: ") + e.what());
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during insert operation");
        }
    }

    /**
     * @brief Remove a specific character from the string.
     * @param ch Character to remove
     * @return Number of characters removed
     */
    auto remove(char ch) noexcept -> size_t { return std::erase(m_data_, ch); }

    /**
     * @brief Remove all occurrences of a substring.
     * @param str Substring to remove
     * @return Number of occurrences removed
     * @throws StringException if memory allocation fails
     */
    auto removeAll(const String& str) -> size_t {
        try {
            if (str.empty() || m_data_.empty()) {
                return 0;
            }
            size_t count = 0;
            size_t pos = 0;
            const size_t strLen = str.length();
            while ((pos = m_data_.find(str.m_data_, pos)) !=
                   std::string::npos) {
                m_data_.erase(pos, strLen);
                ++count;
            }
            return count;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during removeAll operation");
        }
    }

    /**
     * @brief Erase a specific range of characters.
     * @param pos Starting position
     * @param count Number of characters to erase
     * @return Reference to this String
     * @throws StringException if pos is out of range
     */
    auto erase(size_t pos, size_t count = std::string::npos) -> String& {
        try {
            if (pos > m_data_.length()) {
                throw StringException("Erase position out of range");
            }
            m_data_.erase(pos, count);
            return *this;
        } catch (const std::out_of_range&) {
            throw StringException("Erase position out of range");
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during erase operation");
        }
    }

    /**
     * @brief Get the underlying data as a std::string.
     * @return Copy of the underlying string
     */
    [[nodiscard]] auto data() const noexcept -> std::string { return m_data_; }

    /**
     * @brief Get a reference to the underlying data.
     * @return Reference to the underlying string
     */
    [[nodiscard]] auto dataRef() noexcept -> std::string& { return m_data_; }

    /**
     * @brief Get a const reference to the underlying data.
     * @return Const reference to the underlying string
     */
    [[nodiscard]] auto dataRef() const noexcept -> const std::string& {
        return m_data_;
    }

    /**
     * @brief Check if the string is empty.
     * @return True if string is empty
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return m_data_.empty();
    }

    /**
     * @brief Clear the string content.
     */
    void clear() noexcept { m_data_.clear(); }

    /**
     * @brief Pad the string from the left with a specific character.
     * @param totalLength Target length after padding
     * @param paddingChar Character to pad with
     * @return Reference to this String
     * @throws StringException if memory allocation fails
     */
    auto padLeft(size_t totalLength, char paddingChar = ' ') -> String& {
        try {
            if (m_data_.length() < totalLength) {
                m_data_.insert(m_data_.begin(), totalLength - m_data_.length(),
                               paddingChar);
            }
            return *this;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during padLeft operation");
        }
    }

    /**
     * @brief Pad the string from the right with a specific character.
     * @param totalLength Target length after padding
     * @param paddingChar Character to pad with
     * @return Reference to this String
     * @throws StringException if memory allocation fails
     */
    auto padRight(size_t totalLength, char paddingChar = ' ') -> String& {
        try {
            if (m_data_.length() < totalLength) {
                m_data_.append(totalLength - m_data_.length(), paddingChar);
            }
            return *this;
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during padRight operation");
        }
    }

    /**
     * @brief Remove a specific prefix from the string.
     * @param prefix Prefix to remove
     * @return True if prefix was removed
     */
    auto removePrefix(const String& prefix) noexcept -> bool {
        if (startsWith(prefix)) {
            m_data_.erase(0, prefix.length());
            return true;
        }
        return false;
    }

    /**
     * @brief Remove a specific suffix from the string.
     * @param suffix Suffix to remove
     * @return True if suffix was removed
     */
    auto removeSuffix(const String& suffix) noexcept -> bool {
        if (endsWith(suffix)) {
            m_data_.erase(m_data_.length() - suffix.length());
            return true;
        }
        return false;
    }

    /**
     * @brief Compress multiple consecutive spaces into a single space.
     * @throws StringException if memory allocation fails
     */
    void compressSpaces() {
        try {
            auto newEnd = std::unique(
                m_data_.begin(), m_data_.end(), [](char lhs, char rhs) {
                    return (std::isspace(static_cast<unsigned char>(lhs)) !=
                            0) &&
                           (std::isspace(static_cast<unsigned char>(rhs)) != 0);
                });
            m_data_.erase(newEnd, m_data_.end());
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during compressSpaces operation");
        }
    }

    /**
     * @brief Reverse the order of words in the string.
     * @return New string with words in reverse order
     * @throws StringException if memory allocation fails
     */
    [[nodiscard]] auto reverseWords() const -> String {
        try {
            auto words = split(String(" "));
            std::ranges::reverse(words);
            return join(words, String(" "));
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during reverseWords operation");
        }
    }

    /**
     * @brief Replace text using regex pattern.
     * @param pattern Regex pattern
     * @param replacement Replacement string
     * @return New string with replacements
     * @throws StringException if regex compilation fails or memory allocation
     * fails
     */
    auto replaceRegex(const std::string& pattern,
                      const std::string& replacement) -> String {
        try {
#ifdef ATOM_USE_BOOST
            boost::regex rex(pattern);
            return String(boost::regex_replace(m_data_, rex, replacement));
#else
            std::regex rex(pattern);
            return String(std::regex_replace(m_data_, rex, replacement));
#endif
        } catch (const std::regex_error& e) {
            throw StringException(std::string("Regex error: ") + e.what());
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during regex operation");
        }
    }

    /**
     * @brief Format a string with type-safe formatting.
     * @param format_str Format string
     * @param args Format arguments
     * @return Formatted string
     * @throws StringException if formatting fails
     */
    template <typename... Args>
    static auto format(std::string_view format_str, Args&&... args) -> String {
        try {
            return String(
                std::vformat(format_str, std::make_format_args(args...)));
        } catch (const std::format_error& e) {
            throw StringException(std::string("Format error: ") + e.what());
        } catch (const std::bad_alloc&) {
            throw StringException(
                "Memory allocation failed during format operation");
        }
    }

    /**
     * @brief Safe version of format that returns an optional.
     * @param format_str Format string
     * @param args Format arguments
     * @return Optional containing formatted string, or empty if formatting
     * failed
     */
    template <typename... Args>
    static auto formatSafe(std::string_view format_str, Args&&... args) noexcept
        -> std::optional<String> {
        try {
            return String(
                std::vformat(format_str, std::make_format_args(args...)));
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Access character at specific position with bounds checking.
     * @param pos Position to access
     * @return Reference to the character
     * @throws StringException if pos is out of range
     */
    auto at(size_t pos) -> char& {
        try {
            return m_data_.at(pos);
        } catch (const std::out_of_range&) {
            throw StringException("String index out of range");
        }
    }

    /**
     * @brief Access character at specific position with bounds checking (const
     * version).
     * @param pos Position to access
     * @return Reference to the character
     * @throws StringException if pos is out of range
     */
    auto at(size_t pos) const -> const char& {
        try {
            return m_data_.at(pos);
        } catch (const std::out_of_range&) {
            throw StringException("String index out of range");
        }
    }

    /**
     * @brief Access character at specific position without bounds checking.
     * @param pos Position to access
     * @return Reference to the character
     * @note Undefined behavior if pos is out of range
     */
    auto operator[](size_t pos) noexcept -> char& { return m_data_[pos]; }

    /**
     * @brief Access character at specific position without bounds checking
     * (const version).
     * @param pos Position to access
     * @return Reference to the character
     * @note Undefined behavior if pos is out of range
     */
    auto operator[](size_t pos) const noexcept -> const char& {
        return m_data_[pos];
    }

    /**
     * @brief Calculate hash value for the string.
     * @return Hash value
     */
    [[nodiscard]] auto hash() const noexcept -> size_t {
#ifdef ATOM_USE_BOOST
        return boost::hash_value(m_data_);
#else
        return std::hash<std::string>{}(m_data_);
#endif
    }

    /**
     * @brief Swap contents with another String.
     * @param other String to swap with
     */
    void swap(String& other) noexcept { m_data_.swap(other.m_data_); }

    static constexpr size_t NPOS = std::string::npos;

private:
    std::string m_data_;
};

/**
 * @brief Concatenation operator for String class.
 * @param lhs Left-hand string
 * @param rhs Right-hand string
 * @return Concatenated string
 * @throws StringException if memory allocation fails
 */
[[nodiscard]] inline auto operator+(const String& lhs, const String& rhs)
    -> String {
    try {
        String result(lhs);
        result += rhs;
        return result;
    } catch (const std::bad_alloc&) {
        throw StringException(
            "Memory allocation failed during string concatenation");
    }
}

/**
 * @brief Output stream operator for String class.
 * @param os Output stream
 * @param str String to output
 * @return Reference to the output stream
 */
inline auto operator<<(std::ostream& os, const String& str) -> std::ostream& {
    os << str.data();
    return os;
}

/**
 * @brief Input stream operator for String class.
 * @param is Input stream
 * @param str String to read into
 * @return Reference to the input stream
 */
inline auto operator>>(std::istream& is, String& str) -> std::istream& {
    std::string temp;
    is >> temp;
    if (is) {
        try {
            str = String(temp);
        } catch (...) {
            is.setstate(std::ios_base::failbit);
        }
    }
    return is;
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

/**
 * @brief Global swap function for ADL.
 */
inline void swap(String& lhs, String& rhs) noexcept { lhs.swap(rhs); }

#endif  // ATOM_TYPE_STRING_HPP