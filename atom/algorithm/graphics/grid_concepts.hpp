#ifndef ATOM_ALGORITHM_GRAPHICS_GRID_CONCEPTS_HPP
#define ATOM_ALGORITHM_GRAPHICS_GRID_CONCEPTS_HPP

#include <concepts>
#include <ranges>
#include <span>
#include <type_traits>

#include "../core/rust_numeric.hpp"

/**
 * @enum Connectivity
 * @brief Enum to specify the type of connectivity for flood fill.
 */
enum class Connectivity {
    Four,  ///< 4-way connectivity (up, down, left, right)
    Eight  ///< 8-way connectivity (up, down, left, right, and diagonals)
};

// Static assertion to ensure enum values are as expected
static_assert(static_cast<std::int32_t>(Connectivity::Four) == 0 &&
                  static_cast<std::int32_t>(Connectivity::Eight) == 1,
              "Connectivity enum values must be 0 and 1");

/**
 * @concept Grid
 * @brief Concept that defines requirements for a type to be used as a grid.
 */
template <typename T>
concept Grid = requires(T t, std::size_t i, std::size_t j) {
    { t[i] } -> std::ranges::random_access_range;
    { t[i][j] } -> std::convertible_to<typename T::value_type::value_type>;
    requires std::is_default_constructible_v<T>;
    // { t.size() } -> std::convertible_to<usize>;
    { t.empty() } -> std::same_as<bool>;
    // requires(!t.empty() ? t[0].size() > 0 : true);
};

/**
 * @concept SIMDCompatibleGrid
 * @brief Concept that defines requirements for a type to be used with SIMD
 * operations.
 */
template <typename T>
concept SIMDCompatibleGrid =
    Grid<T> &&
    (std::same_as<typename T::value_type::value_type, atom::algorithm::i32> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::f32> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::f64> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::u8> ||
     std::same_as<typename T::value_type::value_type, atom::algorithm::u32>);

/**
 * @concept ContiguousGrid
 * @brief Concept that defines requirements for a grid with contiguous memory
 * layout.
 */
template <typename T>
concept ContiguousGrid = Grid<T> && requires(T t) {
    { t.data() } -> std::convertible_to<typename T::value_type*>;
    requires std::contiguous_iterator<typename T::iterator>;
};

/**
 * @concept SpanCompatibleGrid
 * @brief Concept for grids that can work with std::span for efficient views.
 */
template <typename T>
concept SpanCompatibleGrid = Grid<T> && requires(T t) {
    { std::span<typename T::value_type>(t) };
};

#endif  // ATOM_ALGORITHM_GRAPHICS_GRID_CONCEPTS_HPP
