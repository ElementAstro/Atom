/*
 * static_string.hpp
 *
 * Copyright (C) 2023
 */

/*************************************************

Date: 2023-12-17

Description: An optimized Static String implementation with additional features
using modern C++.

**************************************************/

#ifndef ATOM_EXPERIMENT_SSTRING_HPP
#define ATOM_EXPERIMENT_SSTRING_HPP

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

/**
 * @brief A class representing a static string with a fixed maximum size,
 * optimized using modern C++ features and providing additional functionalities.
 *
 * @tparam N The maximum size of the string (excluding the null terminator).
 */
template <std::size_t N>
class StaticString {
public:
    using value_type = char;
    using size_type = std::size_t;

    /**
     * @brief Default constructor. Constructs an empty StaticString.
     */
    constexpr StaticString() noexcept : size_(0) { data_.fill('\0'); }

    /**
     * @brief Constructor accepting a C-style string literal.
     *
     * @param str The C-style string literal to initialize the StaticString
     * with.
     */
    constexpr StaticString(const char (&str)[N + 1]) noexcept : size_(N) {
        std::copy_n(str, N + 1, data_.begin());
    }

    /**
     * @brief Constructor accepting a std::string_view.
     *
     * @param str The string_view to initialize the StaticString with.
     */
    constexpr StaticString(std::string_view str) : size_(str.size()) {
        if (str.size() > N) {
            throw std::runtime_error(
                "String size exceeds StaticString capacity");
        }
        std::copy_n(str.data(), str.size(), data_.begin());
        data_[str.size()] = '\0';
    }

    constexpr StaticString(const std::array<char, N + 1>& arr) noexcept
        : size_(N) {
        std::copy_n(arr.begin(), N + 1, data_.begin());
    }

    /**
     * @brief Returns the size of the string.
     *
     * @return The size of the string.
     */
    [[nodiscard]] constexpr auto size() const noexcept -> size_type {
        return size_;
    }

    /**
     * @brief Checks if the string is empty.
     *
     * @return True if the string is empty, false otherwise.
     */
    [[nodiscard]] constexpr auto empty() const noexcept -> bool {
        return size_ == 0;
    }

    /**
     * @brief Returns a pointer to the underlying C-style string.
     *
     * @return A pointer to the underlying C-style string.
     */
    [[nodiscard]] constexpr auto c_str() const noexcept -> const char* {
        return data_.data();
    }

    /**
     * @brief Returns an iterator to the beginning of the string.
     *
     * @return An iterator to the beginning of the string.
     */
    [[nodiscard]] constexpr auto begin() noexcept -> char* {
        return data_.data();
    }

    /**
     * @brief Returns a const iterator to the beginning of the string.
     *
     * @return A const iterator to the beginning of the string.
     */
    [[nodiscard]] constexpr auto begin() const noexcept -> const char* {
        return data_.data();
    }

    /**
     * @brief Returns an iterator to the end of the string.
     *
     * @return An iterator to the end of the string.
     */
    [[nodiscard]] constexpr auto end() noexcept -> char* {
        return data_.data() + size_;
    }

    /**
     * @brief Returns a const iterator to the end of the string.
     *
     * @return A const iterator to the end of the string.
     */
    [[nodiscard]] constexpr auto end() const noexcept -> const char* {
        return data_.data() + size_;
    }

    /**
     * @brief Accesses the character at the specified position.
     *
     * @param index The position of the character to access.
     * @return A reference to the character at the specified position.
     */
    constexpr auto operator[](size_type index) noexcept -> char& {
        return data_[index];
    }

    /**
     * @brief Accesses the character at the specified position.
     *
     * @param index The position of the character to access.
     * @return A const reference to the character at the specified position.
     */
    constexpr auto operator[](size_type index) const noexcept -> const char& {
        return data_[index];
    }

    /**
     * @brief Appends a character to the end of the StaticString.
     *
     * @param ch The character to append.
     * @return A reference to the modified StaticString.
     */
    constexpr auto push_back(char ch) -> StaticString& {
        if (size_ >= N) {
            throw std::runtime_error("StaticString overflow on push_back");
        }
        data_[size_++] = ch;
        data_[size_] = '\0';
        return *this;
    }

    /**
     * @brief Appends a string to the end of the StaticString.
     *
     * @param str The string to append.
     * @return A reference to the modified StaticString.
     */
    constexpr auto append(std::string_view str) -> StaticString& {
        if (size_ + str.size() > N) {
            throw std::runtime_error("StaticString overflow on append");
        }
        std::copy_n(str.data(), str.size(), data_.data() + size_);
        size_ += str.size();
        data_[size_] = '\0';
        return *this;
    }

    /**
     * @brief Returns a substring of the StaticString.
     *
     * @param pos The starting position of the substring.
     * @param count The length of the substring.
     * @return A new StaticString containing the substring.
     */
    constexpr auto substr(size_type pos = 0,
                          size_type count = npos) const -> StaticString {
        if (pos > size_) {
            throw std::out_of_range("Substring position out of range");
        }
        size_type len = std::min(count, size_ - pos);
        StaticString result;
        result.size_ = len;
        std::copy_n(data_.data() + pos, len, result.data_.begin());
        result.data_[len] = '\0';
        return result;
    }

    /**
     * @brief Finds a character in the StaticString.
     *
     * @param ch The character to find.
     * @param pos The position to start the search.
     * @return The position of the character, or npos if not found.
     */
    constexpr auto find(char ch,
                        size_type pos = 0) const noexcept -> size_type {
        for (size_type i = pos; i < size_; ++i) {
            if (data_[i] == ch) {
                return i;
            }
        }
        return npos;
    }

    /**
     * @brief Replaces a portion of the StaticString with another string.
     *
     * @param pos The starting position of the portion to replace.
     * @param count The length of the portion to replace.
     * @param str The string to replace with.
     * @return A reference to the modified StaticString.
     */
    constexpr auto replace(size_type pos, size_type count,
                           std::string_view str) -> StaticString& {
        if (pos > size_) {
            throw std::out_of_range("Replace position out of range");
        }
        size_type end_pos = pos + count;
        if (end_pos > size_) {
            end_pos = size_;
        }
        size_type new_size = size_ - (end_pos - pos) + str.size();
        if (new_size > N) {
            throw std::runtime_error("StaticString overflow on replace");
        }
        // Move the tail
        std::move_backward(data_.data() + end_pos, data_.data() + size_,
                           data_.data() + new_size);
        // Copy the new string
        std::copy_n(str.data(), str.size(), data_.data() + pos);
        size_ = new_size;
        data_[size_] = '\0';
        return *this;
    }

    /**
     * @brief Comparison operator ==
     *
     * @param other The StaticString to compare with.
     * @return True if the strings are equal, false otherwise.
     */
    constexpr bool operator==(const StaticString& other) const noexcept {
        return size_ == other.size_ &&
               std::equal(begin(), end(), other.begin());
    }

    /**
     * @brief Comparison operator !=
     *
     * @param other The StaticString to compare with.
     * @return True if the strings are not equal, false otherwise.
     */
    constexpr bool operator!=(const StaticString& other) const noexcept {
        return !(*this == other);
    }

    constexpr auto operator+=(char ch) -> StaticString& {
        return push_back(ch);
    }

    constexpr auto operator+=(std::string_view str) -> StaticString& {
        return append(str);
    }

    constexpr auto operator+=(const StaticString& other) -> StaticString& {
        return append(other);
    }

    /**
     * @brief Concatenation operator +
     *
     * @param other The StaticString to concatenate.
     * @return A new StaticString with concatenated content.
     */
    template <std::size_t M>
    constexpr auto operator+(const StaticString<M>& other) const
        -> StaticString<N + M> {
        StaticString<N + M> result;
        result.size_ = size_ + other.size();
        if (result.size_ > result.capacity()) {
            throw std::runtime_error("StaticString overflow on concatenation");
        }
        std::copy_n(data_.data(), size_, result.data_.begin());
        std::copy_n(other.data_.data(), other.size(),
                    result.data_.begin() + size_);
        result.data_[result.size_] = '\0';
        return result;
    }

    /**
     * @brief Returns the maximum capacity of the StaticString.
     *
     * @return The capacity of the StaticString.
     */
    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
        return N;
    }

    /**
     * @brief The value used to represent an invalid position.
     */
    static constexpr size_type npos = static_cast<size_type>(-1);

    size_type size_;  ///< The current size of the string.
    std::array<char, N + 1>
        data_{};  ///< The underlying data storage for the string.
};

#endif  // ATOM_EXPERIMENT_SSTRING_HPP