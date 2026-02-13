#ifndef ATOM_ALGORITHM_COMMON_CONCEPTS_HPP
#define ATOM_ALGORITHM_COMMON_CONCEPTS_HPP

#include <concepts>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Concept for string-like types that can be used in text algorithms.
 *
 * Satisfied by std::string, std::string_view, and similar types.
 */
template <typename T>
concept StringLike = requires(T t) {
    { t.data() } -> std::convertible_to<const char*>;
    { t.size() } -> std::convertible_to<usize>;
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
};

/**
 * @brief Concept for byte-like types.
 *
 * Satisfied by std::byte, char, unsigned char, uint8_t.
 */
template <typename T>
concept ByteLike =
    std::same_as<std::remove_cv_t<T>, std::byte> ||
    std::same_as<std::remove_cv_t<T>, char> ||
    std::same_as<std::remove_cv_t<T>, unsigned char> ||
    std::same_as<std::remove_cv_t<T>, u8>;

/**
 * @brief Concept for containers of bytes.
 *
 * Satisfied by std::vector<u8>, std::array<u8, N>, std::span<u8>, etc.
 */
template <typename T>
concept ByteContainer = std::ranges::contiguous_range<T> &&
                        ByteLike<std::ranges::range_value_t<T>>;

/**
 * @brief Concept for containers of 32-bit unsigned integers.
 *
 * Used by TEA/XXTEA encryption algorithms.
 */
template <typename T>
concept UInt32Container = std::ranges::contiguous_range<T> &&
                          std::same_as<std::ranges::range_value_t<T>, u32>;

/**
 * @brief Concept for matrix-like 2D containers.
 *
 * Satisfied by std::vector<std::vector<T>> and similar nested containers.
 */
template <typename T>
concept MatrixLike = std::ranges::range<T> &&
                     std::ranges::range<std::ranges::range_value_t<T>>;

/**
 * @brief Concept for numeric types that support arithmetic operations.
 */
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

/**
 * @brief Concept for integral types.
 */
template <typename T>
concept Integral = std::is_integral_v<T>;

/**
 * @brief Concept for floating-point types.
 */
template <typename T>
concept FloatingPoint = std::is_floating_point_v<T>;

/**
 * @brief Concept for hashable types.
 */
template <typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<usize>;
};

/**
 * @brief Concept for unsigned integral types.
 */
template <typename T>
concept UnsignedIntegral = std::unsigned_integral<T>;

/**
 * @brief Concept for arithmetic types (integral or floating-point).
 */
template <typename T>
concept Arithmetic = std::integral<T> || std::floating_point<T>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_COMMON_CONCEPTS_HPP
