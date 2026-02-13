/*
 * random.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Secure random number generation utilities

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_RANDOM_HPP
#define ATOM_ALGORITHM_MATH_RANDOM_HPP

#include <optional>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Generate a cryptographically secure random number
 *
 * @return std::optional<u64> Random value, or nullopt if generation failed
 */
[[nodiscard]] auto secureRandom() noexcept -> std::optional<u64>;

/**
 * @brief Generate a random number in the specified range
 *
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return std::optional<u64> Random value in range, or nullopt if
 * generation failed
 */
[[nodiscard]] auto randomInRange(u64 min,
                                 u64 max) noexcept -> std::optional<u64>;

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_RANDOM_HPP
