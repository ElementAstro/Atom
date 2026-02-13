/*
 * safe_math.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Safe arithmetic operations with overflow/underflow detection

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_SAFE_MATH_HPP
#define ATOM_ALGORITHM_MATH_SAFE_MATH_HPP

#include <concepts>
#include <numeric>

#ifdef __has_include
#if __has_include(<bit>)
#include <bit>
#define HAS_STD_BIT 1
#endif
#endif

#ifndef HAS_STD_BIT
#define HAS_STD_BIT 0
#endif

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

/**
 * @brief Performs a 64-bit multiplication followed by division.
 *
 * This function calculates the result of (operant * multiplier) / divider.
 * Uses compile-time optimizations when possible.
 *
 * @param operant The first operand for multiplication.
 * @param multiplier The second operand for multiplication.
 * @param divider The divisor for the division operation.
 * @return The result of (operant * multiplier) / divider.
 * @throws atom::error::InvalidArgumentException if divider is zero.
 */
[[nodiscard]] auto mulDiv64(u64 operant, u64 multiplier, u64 divider) -> u64;

/**
 * @brief Performs a safe addition operation.
 *
 * This function adds two unsigned 64-bit integers, handling potential overflow.
 * Uses compile-time checks when possible.
 *
 * @param a The first operand for addition.
 * @param b The second operand for addition.
 * @return The result of a + b.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] constexpr auto safeAdd(u64 a, u64 b) -> u64 {
    try {
        u64 result;
#ifdef ATOM_USE_BOOST
        boost::multiprecision::uint128_t temp =
            boost::multiprecision::uint128_t(a) + b;
        if (temp > std::numeric_limits<u64>::max()) {
            THROW_OVERFLOW("Overflow in addition");
        }
        result = static_cast<u64>(temp);
#else
        // Check for overflow before addition using C++20 feature
        if (std::numeric_limits<u64>::max() - a < b) {
            THROW_OVERFLOW("Overflow in addition");
        }
        result = a + b;
#endif
        return result;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in safeAdd: ") + e.what());
    }
}

/**
 * @brief Performs a safe multiplication operation.
 *
 * This function multiplies two unsigned 64-bit integers, handling potential
 * overflow.
 *
 * @param a The first operand for multiplication.
 * @param b The second operand for multiplication.
 * @return The result of a * b.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] constexpr auto safeMul(u64 a, u64 b) -> u64 {
    try {
        u64 result;
#ifdef ATOM_USE_BOOST
        boost::multiprecision::uint128_t temp =
            boost::multiprecision::uint128_t(a) * b;
        if (temp > std::numeric_limits<u64>::max()) {
            THROW_OVERFLOW("Overflow in multiplication");
        }
        result = static_cast<u64>(temp);
#else
        // Check for overflow before multiplication
        if (a > 0 && b > std::numeric_limits<u64>::max() / a) {
            THROW_OVERFLOW("Overflow in multiplication");
        }
        result = a * b;
#endif
        return result;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in safeMul: ") + e.what());
    }
}

/**
 * @brief Performs a safe subtraction operation.
 *
 * This function subtracts two unsigned 64-bit integers, handling potential
 * underflow.
 *
 * @param a The first operand for subtraction.
 * @param b The second operand for subtraction.
 * @return The result of a - b.
 * @throws atom::error::UnderflowException if the operation would underflow.
 */
[[nodiscard]] constexpr auto safeSub(u64 a, u64 b) -> u64 {
    try {
        if (b > a) {
            THROW_UNDERFLOW("Underflow in subtraction");
        }
        return a - b;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in safeSub: ") + e.what());
    }
}

[[nodiscard]] constexpr bool isDivisionByZero(u64 divisor) noexcept {
    return divisor == 0;
}

/**
 * @brief Performs a safe division operation.
 *
 * This function divides two unsigned 64-bit integers, handling potential
 * division by zero.
 *
 * @param a The numerator for division.
 * @param b The denominator for division.
 * @return The result of a / b.
 * @throws atom::error::InvalidArgumentException if there is a division by zero.
 */
[[nodiscard]] constexpr auto safeDiv(u64 a, u64 b) -> u64 {
    try {
        if (isDivisionByZero(b)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }
        return a / b;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in safeDiv: ") + e.what());
    }
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_SAFE_MATH_HPP
