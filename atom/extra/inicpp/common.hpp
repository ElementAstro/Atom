#ifndef ATOM_EXTRA_INICPP_COMMON_HPP
#define ATOM_EXTRA_INICPP_COMMON_HPP

#include <algorithm>
#include <charconv>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include "atom/macro.hpp"

namespace inicpp {

/**
 * @brief Returns a string view of whitespace characters.
 * @return A string view containing whitespace characters.
 */
ATOM_CONSTEXPR auto whitespaces() noexcept -> std::string_view {
    return " \t\n\r\f\v";
}

/**
 * @brief Returns a string view of indent characters.
 * @return A string view containing indent characters.
 */
ATOM_CONSTEXPR auto indents() noexcept -> std::string_view { return " \t"; }

/**
 * @brief Trims leading and trailing whitespace from a string.
 * @param str The string to trim.
 */
ATOM_INLINE void trim(std::string& str) noexcept {
    const auto first = str.find_first_not_of(whitespaces());
    const auto last = str.find_last_not_of(whitespaces());

    if (first == std::string::npos || last == std::string::npos) {
        str.clear();
    } else {
        str = str.substr(first, last - first + 1);
    }
}

/**
 * @brief Converts a string view to a long integer.
 * @param value The string view to convert.
 * @return An optional containing the converted long integer, or std::nullopt if
 * conversion fails.
 */
ATOM_INLINE auto strToLong(std::string_view value) noexcept
    -> std::optional<long> {
    if (value.empty()) {
        return std::nullopt;
    }

    long result;
    auto [ptr, ec] =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec == std::errc()) {
        return result;
    }
    return std::nullopt;
}

/**
 * @brief Converts a string view to an unsigned long integer.
 * @param value The string view to convert.
 * @return An optional containing the converted unsigned long integer, or
 * std::nullopt if conversion fails.
 */
ATOM_INLINE auto strToULong(std::string_view value) noexcept
    -> std::optional<unsigned long> {
    if (value.empty()) {
        return std::nullopt;
    }

    // Check for negative values which would be invalid for unsigned
    if (value.front() == '-') {
        return std::nullopt;
    }

    unsigned long result;
    auto [ptr, ec] =
        std::from_chars(value.data(), value.data() + value.size(), result);
    if (ec == std::errc()) {
        return result;
    }
    return std::nullopt;
}

/**
 * @struct StringInsensitiveLess
 * @brief A comparator for case-insensitive string comparison.
 */
struct StringInsensitiveLess {
    /**
     * @brief Compares two strings in a case-insensitive manner.
     * @param lhs The left-hand side string view.
     * @param rhs The right-hand side string view.
     * @return True if lhs is less than rhs, false otherwise.
     */
    auto operator()(std::string_view lhs,
                    std::string_view rhs) const noexcept -> bool {
        return std::ranges::lexicographical_compare(
            lhs, rhs, [](unsigned char a, unsigned char b) noexcept {
                return std::tolower(a) < std::tolower(b);
            });
    }
};

// Concept defining what types can be used as string-like values
template <typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

// Concept defining numeric types for conversion functions
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_COMMON_HPP
