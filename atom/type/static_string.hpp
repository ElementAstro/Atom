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
#include <cstring>
#include <future>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace detail {
// SIMD optimized string operations helper functions
class SimdHelper {
public:
    // SIMD accelerated character search
    static inline size_t find_char(const char* data, size_t size,
                                   char ch) noexcept {
        // For small strings, use standard approach
        if (size < 16) {
            for (size_t i = 0; i < size; ++i) {
                if (data[i] == ch)
                    return i;
            }
            return static_cast<size_t>(-1);
        }

        // Use SIMD for larger strings
        size_t i = 0;
#ifdef __AVX2__
        __m256i target = _mm256_set1_epi8(ch);

        // Process 32 bytes at a time
        for (; i + 32 <= size; i += 32) {
            __m256i chunk =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + i));
            __m256i eq = _mm256_cmpeq_epi8(chunk, target);
            uint32_t mask = _mm256_movemask_epi8(eq);

            if (mask != 0) {
                return i + __builtin_ctz(mask);
            }
        }
#endif

        // Process remaining bytes
        for (; i < size; ++i) {
            if (data[i] == ch)
                return i;
        }

        return static_cast<size_t>(-1);
    }

    // SIMD accelerated string comparison
    static inline bool equal(const char* a, const char* b,
                             size_t size) noexcept {
        if (size < 16) {
            return std::memcmp(a, b, size) == 0;
        }

        size_t i = 0;
#ifdef __AVX2__

        for (; i + 32 <= size; i += 32) {
            __m256i chunk_a =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a + i));
            __m256i chunk_b =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b + i));
            __m256i eq = _mm256_cmpeq_epi8(chunk_a, chunk_b);
            uint32_t mask = _mm256_movemask_epi8(eq);

            if (mask != 0xFFFFFFFF) {
                return false;
            }
        }
#endif

        return std::memcmp(a + i, b + i, size - i) == 0;
    }
};

// Used for parallel operations threshold decision
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

    /**
     * @brief Default constructor. Constructs an empty StaticString.
     */
    constexpr StaticString() noexcept : size_(0) { data_.fill('\0'); }

    /**
     * @brief Constructor accepting a C-style string literal.
     *
     * @param str The C-style string literal to initialize the StaticString
     * with.
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
     * @param str The string literal to initialize the StaticString with.
     */
    template <size_type M>
    constexpr StaticString(const char (&str)[M]) noexcept {
        static_assert(M <= N + 1,
                      "String literal exceeds StaticString capacity");
        size_ = M - 1;  // Exclude null terminator
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
        // Find actual string length (up to null terminator)
        size_ = 0;
        while (size_ < N && arr[size_] != '\0') {
            ++size_;
        }
    }

    /**
     * @brief Copy constructor.
     */
    constexpr StaticString(const StaticString& other) noexcept = default;

    /**
     * @brief Move constructor.
     */
    constexpr StaticString(StaticString&& other) noexcept
        : size_(other.size_), data_(std::move(other.data_)) {
        other.size_ = 0;
        other.data_.fill('\0');
    }

    /**
     * @brief Copy assignment operator.
     */
    constexpr StaticString& operator=(const StaticString& other) noexcept =
        default;

    /**
     * @brief Move assignment operator.
     */
    constexpr StaticString& operator=(StaticString&& other) noexcept {
        if (this != &other) {
            size_ = other.size_;
            data_ = std::move(other.data_);
            other.size_ = 0;
            other.data_.fill('\0');
        }
        return *this;
    }

    /**
     * @brief Destructor.
     */
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
     * @return A reference to the underlying data array.
     */
    [[nodiscard]] constexpr auto data() noexcept -> char* {
        return data_.data();
    }

    /**
     * @brief Returns the underlying data array (const version).
     *
     * @return A const reference to the underlying data array.
     */
    [[nodiscard]] constexpr auto data() const noexcept -> const char* {
        return data_.data();
    }

    /**
     * @brief Returns an iterator to the beginning of the string.
     *
     * @return An iterator to the beginning of the string.
     */
    [[nodiscard]] constexpr auto begin() noexcept -> iterator {
        return data_.data();
    }

    /**
     * @brief Returns a const iterator to the beginning of the string.
     *
     * @return A const iterator to the beginning of the string.
     */
    [[nodiscard]] constexpr auto begin() const noexcept -> const_iterator {
        return data_.data();
    }

    /**
     * @brief Returns a const iterator to the beginning of the string.
     *
     * @return A const iterator to the beginning of the string.
     */
    [[nodiscard]] constexpr auto cbegin() const noexcept -> const_iterator {
        return data_.data();
    }

    /**
     * @brief Returns an iterator to the end of the string.
     *
     * @return An iterator to the end of the string.
     */
    [[nodiscard]] constexpr auto end() noexcept -> iterator {
        return data_.data() + size_;
    }

    /**
     * @brief Returns a const iterator to the end of the string.
     *
     * @return A const iterator to the end of the string.
     */
    [[nodiscard]] constexpr auto end() const noexcept -> const_iterator {
        return data_.data() + size_;
    }

    /**
     * @brief Returns a const iterator to the end of the string.
     *
     * @return A const iterator to the end of the string.
     */
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
     * checking.
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

    /**
     * @brief Accesses the character at the specified position.
     *
     * @param index The position of the character to access.
     * @return A reference to the character at the specified position.
     */
    constexpr auto operator[](size_type index) noexcept -> reference {
        return data_[index];
    }

    /**
     * @brief Accesses the character at the specified position.
     *
     * @param index The position of the character to access.
     * @return A const reference to the character at the specified position.
     */
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
     * @brief Access the first character of the string.
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
     * @brief Access the last character of the string.
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
        try {
            if (size_ + str.size() > N) {
                throw std::runtime_error("StaticString overflow on append");
            }

            if (str.size() > detail::parallel_threshold &&
                size_ > detail::parallel_threshold) {
                // Parallel copy for large strings
                const size_t chunk_size =
                    str.size() / std::thread::hardware_concurrency();
                std::vector<std::future<void>> futures;

                for (size_t offset = 0; offset < str.size();
                     offset += chunk_size) {
                    size_t len = std::min(chunk_size, str.size() - offset);
                    futures.push_back(std::async(
                        std::launch::async,
                        [this, &str, offset, len, pos = size_ + offset]() {
                            std::copy_n(str.data() + offset, len,
                                        data_.data() + pos);
                        }));
                }

                for (auto& f : futures) {
                    f.wait();
                }
            } else {
                // Sequential copy for smaller strings
                std::copy_n(str.data(), str.size(), data_.data() + size_);
            }

            size_ += str.size();
            data_[size_] = '\0';
            return *this;
        } catch (const std::exception& e) {
            // Ensure string remains in valid state
            data_[size_] = '\0';
            throw;
        }
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
            // Fill with specified character
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
    [[nodiscard]] constexpr auto substr(
        size_type pos = 0, size_type count = npos) const -> StaticString {
        if (pos > size_) {
            throw std::out_of_range("Substring position out of range");
        }
        size_type len = std::min(count, size_ - pos);
        StaticString result;
        result.size_ = len;

        try {
            std::copy_n(data_.data() + pos, len, result.data_.begin());
            result.data_[len] = '\0';
            return result;
        } catch (const std::exception& e) {
            // Ensure the result string is in a valid state
            result.size_ = 0;
            result.data_[0] = '\0';
            throw;
        }
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
        if (pos >= size_) {
            return npos;
        }

        size_t result =
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
    [[nodiscard]] constexpr auto find(
        std::string_view str, size_type pos = 0) const noexcept -> size_type {
        if (pos >= size_ || str.empty() || str.size() > size_ - pos) {
            return npos;
        }

        // Use optimized algorithm (Boyer-Moore or KMP) for larger strings
        if (str.size() > 16 && size_ > detail::parallel_threshold) {
            // Simplified Boyer-Moore-like algorithm
            // In a full implementation, we would use a proper Boyer-Moore
            const size_type str_len = str.size();
            const size_type limit = size_ - str_len + 1;

            for (size_type i = pos; i < limit;) {
                // Start comparing from the end
                size_type j = str_len - 1;
                while (j != static_cast<size_type>(-1) &&
                       data_[i + j] == str[j]) {
                    j--;
                }

                if (j == static_cast<size_type>(-1)) {
                    return i;  // Match found
                }

                // Skip by at least 1, could be optimized further with bad
                // character rule
                i++;
            }
            return npos;
        } else {
            // Standard search for smaller strings
            auto it = std::search(begin() + pos, end(), str.begin(), str.end());
            return it == end() ? npos : std::distance(begin(), it);
        }
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
    constexpr auto replace(size_type pos, size_type count,
                           std::string_view str) -> StaticString& {
        if (pos > size_) {
            throw std::out_of_range("Replace position out of range");
        }

        size_type end_pos = std::min(pos + count, size_);
        size_type new_size = size_ - (end_pos - pos) + str.size();

        if (new_size > N) {
            throw std::runtime_error("StaticString overflow on replace");
        }

        try {
            // Create a temporary buffer for safety
            std::array<char, N + 1> temp_buffer{};

            // Copy the beginning part
            std::copy_n(data_.data(), pos, temp_buffer.data());

            // Copy the replacement string
            std::copy_n(str.data(), str.size(), temp_buffer.data() + pos);

            // Copy the ending part
            std::copy_n(data_.data() + end_pos, size_ - end_pos,
                        temp_buffer.data() + pos + str.size());

            // Update data
            std::copy_n(temp_buffer.data(), new_size, data_.data());
            size_ = new_size;
            data_[size_] = '\0';

            return *this;
        } catch (const std::exception& e) {
            // Ensure string remains in valid state
            data_[size_] = '\0';
            throw;
        }
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
    constexpr auto insert(size_type pos,
                          std::string_view str) -> StaticString& {
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
    constexpr auto erase(size_type pos,
                         size_type count = npos) -> StaticString& {
        if (pos > size_) {
            throw std::out_of_range("Erase position out of range");
        }

        size_type actual_count = std::min(count, size_ - pos);
        size_type end_pos = pos + actual_count;

        try {
            std::move(data_.data() + end_pos, data_.data() + size_,
                      data_.data() + pos);
            size_ -= actual_count;
            data_[size_] = '\0';

            return *this;
        } catch (const std::exception& e) {
            // Ensure string remains in valid state
            data_[size_] = '\0';
            throw;
        }
    }

    /**
     * @brief Comparison operator ==
     *
     * @param other The StaticString to compare with.
     * @return True if the strings are equal, false otherwise.
     */
    constexpr bool operator==(const StaticString& other) const noexcept {
        if (size_ != other.size_) {
            return false;
        }

        return detail::SimdHelper::equal(data_.data(), other.data_.data(),
                                         size_);
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

    /**
     * @brief Comparison operator with string_view.
     *
     * @param sv The string_view to compare with
     * @return True if the strings are equal, false otherwise
     */
    constexpr bool operator==(std::string_view sv) const noexcept {
        if (size_ != sv.size()) {
            return false;
        }

        return detail::SimdHelper::equal(data_.data(), sv.data(), size_);
    }

    /**
     * @brief Append character operator.
     *
     * @param ch Character to append
     * @return Reference to this object
     */
    constexpr auto operator+=(char ch) -> StaticString& {
        return push_back(ch);
    }

    /**
     * @brief Append string operator.
     *
     * @param str String to append
     * @return Reference to this object
     */
    constexpr auto operator+=(std::string_view str) -> StaticString& {
        return append(str);
    }

    /**
     * @brief Append StaticString operator.
     *
     * @param other StaticString to append
     * @return Reference to this object
     */
    constexpr auto operator+=(const StaticString& other) -> StaticString& {
        return append(std::string_view(other.data(), other.size()));
    }

    /**
     * @brief Concatenation operator +
     *
     * @param other The StaticString to concatenate.
     * @return A new StaticString with concatenated content.
     * @throws std::runtime_error If the operation would exceed capacity.
     */
    template <std::size_t M>
    [[nodiscard]] constexpr auto operator+(const StaticString<M>& other) const
        -> StaticString<N + M> {
        StaticString<N + M> result;
        result.size_ = size_ + other.size();

        if (result.size_ > result.capacity()) {
            throw std::runtime_error("StaticString overflow on concatenation");
        }

        try {
            // Copy this string
            std::copy_n(data_.data(), size_, result.data_.begin());

            // Copy other string
            std::copy_n(other.data_.data(), other.size(),
                        result.data_.begin() + size_);

            result.data_[result.size_] = '\0';
            return result;
        } catch (const std::exception& e) {
            // Ensure result is in valid state
            result.clear();
            throw;
        }
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
     * @brief Returns the maximum capacity of the StaticString.
     *
     * @return The capacity of the StaticString.
     */
    [[nodiscard]] constexpr auto capacity() const noexcept -> size_type {
        return N;
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

    /**
     * @brief The value used to represent an invalid position.
     */
    static constexpr size_type npos = static_cast<size_type>(-1);

private:
    size_type size_;  ///< The current size of the string.
    std::array<char, N + 1>
        data_{};  ///< The underlying data storage for the string.
};

/**
 * @brief Stream insertion operator for StaticString.
 *
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
