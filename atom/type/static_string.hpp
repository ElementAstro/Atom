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

#include <immintrin.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace detail {

class SimdHelper {
public:
    static inline size_t find_char(const char* data, size_t size,
                                   char ch) noexcept {
        if (size < 16) {
            for (size_t i = 0; i < size; ++i) {
                if (data[i] == ch)
                    return i;
            }
            return static_cast<size_t>(-1);
        }

        size_t i = 0;
#ifdef __AVX2__
        const __m256i target = _mm256_set1_epi8(ch);
        for (; i + 32 <= size; i += 32) {
            const __m256i chunk =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            const __m256i eq = _mm256_cmpeq_epi8(chunk, target);
            const uint32_t mask = _mm256_movemask_epi8(eq);
            if (mask != 0) {
                return i + __builtin_ctz(mask);
            }
        }
#elif defined(__SSE2__)
        const __m128i target = _mm_set1_epi8(ch);
        for (; i + 16 <= size; i += 16) {
            const __m128i chunk =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
            const __m128i eq = _mm_cmpeq_epi8(chunk, target);
            const uint16_t mask = _mm_movemask_epi8(eq);
            if (mask != 0) {
                return i + __builtin_ctz(mask);
            }
        }
#endif

        for (; i < size; ++i) {
            if (data[i] == ch)
                return i;
        }
        return static_cast<size_t>(-1);
    }

    static inline bool equal(const char* a, const char* b,
                             size_t size) noexcept {
        if (size < 16) {
            return std::memcmp(a, b, size) == 0;
        }

        size_t i = 0;
#ifdef __AVX2__
        for (; i + 32 <= size; i += 32) {
            const __m256i chunk_a =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
            const __m256i chunk_b =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
            const __m256i eq = _mm256_cmpeq_epi8(chunk_a, chunk_b);
            if (_mm256_movemask_epi8(eq) != 0xFFFFFFFF) {
                return false;
            }
        }
#elif defined(__SSE2__)
        for (; i + 16 <= size; i += 16) {
            const __m128i chunk_a =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(a + i));
            const __m128i chunk_b =
                _mm_loadu_si128(reinterpret_cast<const __m128i*>(b + i));
            const __m128i eq = _mm_cmpeq_epi8(chunk_a, chunk_b);
            if (_mm_movemask_epi8(eq) != 0xFFFF) {
                return false;
            }
        }
#endif

        return std::memcmp(a + i, b + i, size - i) == 0;
    }
};

constexpr size_t parallel_threshold = 1024;

}  // namespace detail

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
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = pointer;
    using const_iterator = const_pointer;

    static constexpr size_type npos = static_cast<size_type>(-1);

    /**
     * @brief Default constructor. Constructs an empty StaticString.
     */
    constexpr StaticString() noexcept : size_(0) { data_.fill('\0'); }

    /**
     * @brief Constructor accepting a C-style string literal.
     *
     * @param str The C-style string literal to initialize the StaticString
     * with.
     * @throws std::invalid_argument If str is null.
     * @throws std::runtime_error If the string size exceeds capacity.
     */
    constexpr StaticString(const char* str) {
        if (!str) {
            throw std::invalid_argument(
                "Null pointer passed to StaticString constructor");
        }

        const size_type len = std::strlen(str);
        if (len > N) {
            throw std::runtime_error(
                "String size exceeds StaticString capacity");
        }

        size_ = len;
        std::copy_n(str, len, data_.begin());
        data_[len] = '\0';
    }

    /**
     * @brief Constructor accepting a string literal.
     *
     * @tparam M Size of the string literal including null terminator.
     * @param str The string literal to initialize the StaticString with.
     */
    template <size_type M>
    constexpr StaticString(const char (&str)[M]) noexcept {
        static_assert(M <= N + 1,
                      "String literal exceeds StaticString capacity");
        size_ = M - 1;
        std::copy_n(str, M, data_.begin());
    }

    /**
     * @brief Constructor accepting a std::string_view.
     *
     * @param str The string_view to initialize the StaticString with.
     * @throws std::runtime_error If the string size exceeds capacity.
     */
    constexpr StaticString(std::string_view str) {
        if (str.size() > N) {
            throw std::runtime_error(
                "String size exceeds StaticString capacity");
        }
        size_ = str.size();
        std::copy_n(str.data(), str.size(), data_.begin());
        data_[str.size()] = '\0';
    }

    /**
     * @brief Constructor accepting an array.
     *
     * @param arr The array to initialize the StaticString with.
     */
    constexpr StaticString(const std::array<char, N + 1>& arr) noexcept
        : data_(arr) {
        size_ = 0;
        while (size_ < N && arr[size_] != '\0') {
            ++size_;
        }
    }

    constexpr StaticString(const StaticString& other) noexcept = default;
    constexpr StaticString(StaticString&& other) noexcept
        : size_(other.size_), data_(std::move(other.data_)) {
        other.size_ = 0;
        other.data_.fill('\0');
    }

    constexpr StaticString& operator=(const StaticString& other) noexcept =
        default;
    constexpr StaticString& operator=(StaticString&& other) noexcept {
        if (this != &other) {
            size_ = other.size_;
            data_ = std::move(other.data_);
            other.size_ = 0;
            other.data_.fill('\0');
        }
        return *this;
    }

    ~StaticString() noexcept = default;

    /**
     * @brief Returns the size of the string.
     *
     * @return The size of the string.
     */
    [[nodiscard]] constexpr auto size() const noexcept -> size_type {
        return size_;
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
     * @brief Returns the underlying data array.
     *
     * @return A pointer to the underlying data array.
     */
    [[nodiscard]] constexpr auto data() noexcept -> char* {
        return data_.data();
    }

    /**
     * @brief Returns the underlying data array (const version).
     *
     * @return A const pointer to the underlying data array.
     */
    [[nodiscard]] constexpr auto data() const noexcept -> const char* {
        return data_.data();
    }

    [[nodiscard]] constexpr auto begin() noexcept -> iterator {
        return data_.data();
    }
    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator {
        return data_.data();
    }
    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator {
        return data_.data();
    }
    [[nodiscard]] constexpr auto end() noexcept -> iterator {
        return data_.data() + size_;
    }
    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        return data_.data() + size_;
    }
    [[nodiscard]] constexpr auto cend() const noexcept -> const_iterator {
        return data_.data() + size_;
    }

    /**
     * @brief Accesses the character at the specified position with bounds
     * checking.
     *
     * @param index The position of the character to access.
     * @return A reference to the character at the specified position.
     * @throws std::out_of_range If the index is out of range.
     */
    constexpr auto at(size_type index) -> reference {
        if (index >= size_) {
            throw std::out_of_range("StaticString index out of range");
        }
        return data_[index];
    }

    /**
     * @brief Accesses the character at the specified position with bounds
     * checking (const version).
     *
     * @param index The position of the character to access.
     * @return A const reference to the character at the specified position.
     * @throws std::out_of_range If the index is out of range.
     */
    constexpr auto at(size_type index) const -> const_reference {
        if (index >= size_) {
            throw std::out_of_range("StaticString index out of range");
        }
        return data_[index];
    }

    constexpr auto operator[](size_type index) noexcept -> reference {
        return data_[index];
    }
    constexpr auto operator[](size_type index) const noexcept
        -> const_reference {
        return data_[index];
    }

    /**
     * @brief Access the first character of the string.
     *
     * @return A reference to the first character.
     * @throws std::out_of_range If the string is empty.
     */
    constexpr auto front() -> reference {
        if (empty()) {
            throw std::out_of_range(
                "Cannot access front() of empty StaticString");
        }
        return data_[0];
    }

    /**
     * @brief Access the first character of the string (const version).
     *
     * @return A const reference to the first character.
     * @throws std::out_of_range If the string is empty.
     */
    constexpr auto front() const -> const_reference {
        if (empty()) {
            throw std::out_of_range(
                "Cannot access front() of empty StaticString");
        }
        return data_[0];
    }

    /**
     * @brief Access the last character of the string.
     *
     * @return A reference to the last character.
     * @throws std::out_of_range If the string is empty.
     */
    constexpr auto back() -> reference {
        if (empty()) {
            throw std::out_of_range(
                "Cannot access back() of empty StaticString");
        }
        return data_[size_ - 1];
    }

    /**
     * @brief Access the last character of the string (const version).
     *
     * @return A const reference to the last character.
     * @throws std::out_of_range If the string is empty.
     */
    constexpr auto back() const -> const_reference {
        if (empty()) {
            throw std::out_of_range(
                "Cannot access back() of empty StaticString");
        }
        return data_[size_ - 1];
    }

    /**
     * @brief Clears the contents of the string.
     */
    constexpr void clear() noexcept {
        size_ = 0;
        data_[0] = '\0';
    }

    /**
     * @brief Appends a character to the end of the StaticString.
     *
     * @param ch The character to append.
     * @return A reference to the modified StaticString.
     * @throws std::runtime_error If the operation would exceed capacity.
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
     * @brief Removes the last character of the string.
     *
     * @throws std::runtime_error If the string is empty.
     */
    constexpr void pop_back() {
        if (empty()) {
            throw std::runtime_error("Cannot pop_back from empty StaticString");
        }
        data_[--size_] = '\0';
    }

    /**
     * @brief Appends a string to the end of the StaticString.
     *
     * @param str The string to append.
     * @return A reference to the modified StaticString.
     * @throws std::runtime_error If the operation would exceed capacity.
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
     * @brief Appends another StaticString to this one.
     *
     * @tparam M Size of the other StaticString
     * @param other The StaticString to append
     * @return A reference to the modified StaticString.
     * @throws std::runtime_error If the operation would exceed capacity.
     */
    template <std::size_t M>
    constexpr auto append(const StaticString<M>& other) -> StaticString& {
        return append(std::string_view(other.data(), other.size()));
    }

    /**
     * @brief Resizes the string to the specified size.
     *
     * @param count The new size
     * @param ch Character to fill with if expanding
     * @throws std::runtime_error If count exceeds capacity
     */
    constexpr void resize(size_type count, char ch = '\0') {
        if (count > N) {
            throw std::runtime_error("StaticString resize exceeds capacity");
        }

        if (count > size_) {
            std::fill_n(data_.data() + size_, count - size_, ch);
        }

        size_ = count;
        data_[size_] = '\0';
    }

    /**
     * @brief Returns a substring of the StaticString.
     *
     * @param pos The starting position of the substring.
     * @param count The length of the substring.
     * @return A new StaticString containing the substring.
     * @throws std::out_of_range If position is out of bounds.
     */
    [[nodiscard]] constexpr auto substr(size_type pos = 0,
                                        size_type count = npos) const
        -> StaticString {
        if (pos > size_) {
            throw std::out_of_range("Substring position out of range");
        }

        const size_type len = std::min(count, size_ - pos);
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
    [[nodiscard]] constexpr auto find(char ch, size_type pos = 0) const noexcept
        -> size_type {
        if (pos >= size_)
            return npos;

        const size_t result =
            detail::SimdHelper::find_char(data_.data() + pos, size_ - pos, ch);
        return (result == static_cast<size_t>(-1)) ? npos : pos + result;
    }

    /**
     * @brief Finds a substring in the StaticString.
     *
     * @param str The substring to find.
     * @param pos The position to start the search.
     * @return The position of the substring, or npos if not found.
     */
    [[nodiscard]] constexpr auto find(std::string_view str,
                                      size_type pos = 0) const noexcept
        -> size_type {
        if (pos >= size_ || str.empty() || str.size() > size_ - pos) {
            return npos;
        }

        const auto it =
            std::search(begin() + pos, end(), str.begin(), str.end());
        return it == end() ? npos : std::distance(begin(), it);
    }

    /**
     * @brief Replaces a portion of the StaticString with another string.
     *
     * @param pos The starting position of the portion to replace.
     * @param count The length of the portion to replace.
     * @param str The string to replace with.
     * @return A reference to the modified StaticString.
     * @throws std::out_of_range If position is out of bounds.
     * @throws std::runtime_error If the operation would exceed capacity.
     */
    constexpr auto replace(size_type pos, size_type count, std::string_view str)
        -> StaticString& {
        if (pos > size_) {
            throw std::out_of_range("Replace position out of range");
        }

        const size_type end_pos = std::min(pos + count, size_);
        const size_type new_size = size_ - (end_pos - pos) + str.size();

        if (new_size > N) {
            throw std::runtime_error("StaticString overflow on replace");
        }

        std::array<char, N + 1> temp_buffer{};
        std::copy_n(data_.data(), pos, temp_buffer.data());
        std::copy_n(str.data(), str.size(), temp_buffer.data() + pos);
        std::copy_n(data_.data() + end_pos, size_ - end_pos,
                    temp_buffer.data() + pos + str.size());

        std::copy_n(temp_buffer.data(), new_size, data_.data());
        size_ = new_size;
        data_[size_] = '\0';

        return *this;
    }

    /**
     * @brief Inserts a string at the specified position.
     *
     * @param pos Position at which to insert
     * @param str String to insert
     * @return Reference to this object
     * @throws std::out_of_range If position is out of bounds
     * @throws std::runtime_error If the operation would exceed capacity
     */
    constexpr auto insert(size_type pos, std::string_view str)
        -> StaticString& {
        return replace(pos, 0, str);
    }

    /**
     * @brief Erases a portion of the string.
     *
     * @param pos Position at which to start erasing
     * @param count Number of characters to erase
     * @return Reference to this object
     * @throws std::out_of_range If position is out of bounds
     */
    constexpr auto erase(size_type pos, size_type count = npos)
        -> StaticString& {
        if (pos > size_) {
            throw std::out_of_range("Erase position out of range");
        }

        const size_type actual_count = std::min(count, size_ - pos);
        const size_type end_pos = pos + actual_count;

        std::move(data_.data() + end_pos, data_.data() + size_,
                  data_.data() + pos);
        size_ -= actual_count;
        data_[size_] = '\0';

        return *this;
    }

    constexpr bool operator==(const StaticString& other) const noexcept {
        return size_ == other.size_ &&
               detail::SimdHelper::equal(data_.data(), other.data_.data(),
                                         size_);
    }

    constexpr bool operator!=(const StaticString& other) const noexcept {
        return !(*this == other);
    }

    constexpr bool operator==(std::string_view sv) const noexcept {
        return size_ == sv.size() &&
               detail::SimdHelper::equal(data_.data(), sv.data(), size_);
    }

    constexpr auto operator+=(char ch) -> StaticString& {
        return push_back(ch);
    }

    constexpr auto operator+=(std::string_view str) -> StaticString& {
        return append(str);
    }

    constexpr auto operator+=(const StaticString& other) -> StaticString& {
        return append(std::string_view(other.data(), other.size()));
    }

    /**
     * @brief Concatenation operator +
     *
     * @tparam M Size of the other StaticString
     * @param other The StaticString to concatenate.
     * @return A new StaticString with concatenated content.
     * @throws std::runtime_error If the operation would exceed capacity.
     */
    template <std::size_t M>
    [[nodiscard]] constexpr auto operator+(const StaticString<M>& other) const
        -> StaticString<N + M> {
        StaticString<N + M> result;
        const size_type total_size = this->size() + other.size();

        if (total_size > N + M) {
            throw std::runtime_error("StaticString overflow on concatenation");
        }

        std::copy_n(this->data(), this->size(), result.data());
        std::copy_n(other.data(), other.size(), result.data() + this->size());
        result.resize(total_size);
        return result;
    }

    /**
     * @brief Convert to std::string_view
     *
     * @return string_view representing this string
     */
    [[nodiscard]] constexpr operator std::string_view() const noexcept {
        return std::string_view(data_.data(), size_);
    }

    /**
     * @brief Safely creates a StaticString from a potentially unsafe source.
     *
     * @param str Source string
     * @return std::optional containing the StaticString if successful,
     * std::nullopt otherwise
     */
    [[nodiscard]] static constexpr auto make_safe(std::string_view str) noexcept
        -> std::optional<StaticString> {
        try {
            if (str.size() <= N) {
                return StaticString(str);
            }
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    size_type size_;
    std::array<char, N + 1> data_{};
};

/**
 * @brief Stream insertion operator for StaticString.
 *
 * @tparam N Size of the StaticString
 * @param os Output stream
 * @param str The StaticString to output
 * @return Reference to the output stream
 */
template <std::size_t N>
std::ostream& operator<<(std::ostream& os, const StaticString<N>& str) {
    return os.write(str.data(), str.size());
}

/**
 * @brief Deduction guide for array initialization.
 */
template <typename T, std::size_t N>
StaticString(const T (&)[N]) -> StaticString<N - 1>;

#endif  // ATOM_EXPERIMENT_SSTRING_HPP
