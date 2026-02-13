/*
 * safe_math.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Safe arithmetic operations - platform-specific implementations

**************************************************/

#include "safe_math.hpp"

#include <limits>

#ifdef _MSC_VER
#include <intrin.h>
#include <stdexcept>
#endif

#include "atom/error/exception.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/multiprecision/cpp_int.hpp>
#endif

namespace atom::algorithm {

#ifdef ATOM_USE_BOOST
auto mulDiv64(u64 operand, u64 multiplier, u64 divider) -> u64 {
    try {
        if (isDivisionByZero(divider)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        boost::multiprecision::uint128_t a = operand;
        boost::multiprecision::uint128_t b = multiplier;
        boost::multiprecision::uint128_t c = divider;
        return static_cast<u64>((a * b) / c);
    } catch (const boost::multiprecision::overflow_error&) {
        THROW_OVERFLOW("Overflow in multiplication before division");
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in mulDiv64: ") + e.what());
    }
}
#endif

#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
auto mulDiv64(u64 operand, u64 multiplier, u64 divider) -> u64 {
    try {
        if (isDivisionByZero(divider)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        __uint128_t a = operand;
        __uint128_t b = multiplier;
        __uint128_t c = divider;
        __uint128_t result = (a * b) / c;

        // Check if result fits in u64
        if (result > std::numeric_limits<u64>::max()) {
            THROW_OVERFLOW("Result exceeds u64 range");
        }

        return static_cast<u64>(result);
    } catch (const atom::error::Exception& e) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in mulDiv64: ") + e.what());
    }
}
#elif defined(_MSC_VER)
auto mulDiv64(u64 operand, u64 multiplier, u64 divider) -> u64 {
    try {
        if (isDivisionByZero(divider)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        u64 highProd;
        u64 lowProd = _umul128(operand, multiplier, &highProd);

        // Check for overflow in multiplication
        if (operand > 0 && multiplier > 0 &&
            highProd > (std::numeric_limits<u64>::max() / operand)) {
            THROW_OVERFLOW("Overflow in multiplication");
        }

        // Fast path for small values that won't overflow
        if (highProd == 0) {
            return lowProd / divider;
        }

        // Normalize divisor
        unsigned long shift = 63 - std::countl_zero(divider);
        u64 normDiv = divider << shift;

        // Prepare for division
        highProd = (highProd << shift) | (lowProd >> (64 - shift));
        lowProd <<= shift;

        // Perform division
        u64 quotient;
        _udiv128(highProd, lowProd, normDiv, &quotient);

        return quotient;
    } catch (const atom::error::Exception& e) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in mulDiv64: ") + e.what());
    }
}
#else
#error "Platform not supported for mulDiv64 function!"
#endif

}  // namespace atom::algorithm
