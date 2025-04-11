#ifndef ATOM_EXTRA_INICPP_COMMON_HPP
#define ATOM_EXTRA_INICPP_COMMON_HPP

#include <algorithm>
#include <charconv>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>

#include "atom/macro.hpp"

// Configuration macro definitions
#ifndef INICPP_CONFIG_USE_BOOST
#define INICPP_CONFIG_USE_BOOST 0  // Do not use Boost by default
#endif

#ifndef INICPP_CONFIG_USE_BOOST_CONTAINERS
#define INICPP_CONFIG_USE_BOOST_CONTAINERS \
    0  // Do not use Boost containers by default
#endif

#ifndef INICPP_CONFIG_USE_MEMORY_POOL
#define INICPP_CONFIG_USE_MEMORY_POOL 0  // Do not use memory pool by default
#endif

#ifndef INICPP_CONFIG_NESTED_SECTIONS
#define INICPP_CONFIG_NESTED_SECTIONS 1  // Enable nested sections by default
#endif

#ifndef INICPP_CONFIG_EVENT_LISTENERS
#define INICPP_CONFIG_EVENT_LISTENERS 1  // Enable event listeners by default
#endif

#ifndef INICPP_CONFIG_PATH_QUERY
#define INICPP_CONFIG_PATH_QUERY 1  // Enable path query by default
#endif

#ifndef INICPP_CONFIG_FORMAT_CONVERSION
#define INICPP_CONFIG_FORMAT_CONVERSION \
    1  // Enable format conversion by default
#endif

// Check if Boost is available
#if INICPP_CONFIG_USE_BOOST
#ifdef __has_include
#if __has_include(<boost/version.hpp>)
#include <boost/version.hpp>
#define INICPP_HAS_BOOST 1
#else
#define INICPP_HAS_BOOST 0
#if INICPP_CONFIG_USE_BOOST_CONTAINERS
#undef INICPP_CONFIG_USE_BOOST_CONTAINERS
#define INICPP_CONFIG_USE_BOOST_CONTAINERS 0
#endif
#endif
#else
#define INICPP_HAS_BOOST 0
#endif
#else
#define INICPP_HAS_BOOST 0
#undef INICPP_CONFIG_USE_BOOST_CONTAINERS
#define INICPP_CONFIG_USE_BOOST_CONTAINERS 0
#endif

// Include necessary Boost headers
#if INICPP_HAS_BOOST && INICPP_CONFIG_USE_BOOST_CONTAINERS
#include <boost/container/flat_map.hpp>
#include <boost/container/string.hpp>
#include <boost/pool/object_pool.hpp>
#include <boost/unordered_map.hpp>
#else
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#endif

namespace inicpp {

// Container type definitions, select different implementations based on
// configuration
#if INICPP_HAS_BOOST && INICPP_CONFIG_USE_BOOST_CONTAINERS
// Use Boost containers
template <typename Key, typename Value, typename Compare = std::less<Key>>
using map_type = boost::container::flat_map<Key, Value, Compare>;

template <typename Key, typename Value, typename Hash = boost::hash<Key>>
using hash_map_type = boost::unordered_map<Key, Value, Hash>;

// For small strings, use Boost's string type
using small_string = boost::container::string;

#if INICPP_CONFIG_USE_MEMORY_POOL
template <typename T>
using allocator_type = boost::pool_allocator<T>;
#else
template <typename T>
using allocator_type = std::allocator<T>;
#endif
#else
// Use standard library containers
template <typename Key, typename Value, typename Compare = std::less<Key>>
using map_type = std::map<Key, Value, Compare>;

template <typename Key, typename Value, typename Hash = std::hash<Key>>
using hash_map_type = std::unordered_map<Key, Value, Hash>;

using small_string = std::string;

template <typename T>
using allocator_type = std::allocator<T>;
#endif

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
    auto operator()(std::string_view lhs, std::string_view rhs) const noexcept
        -> bool {
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
