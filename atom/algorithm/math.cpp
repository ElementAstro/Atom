/*
 * math.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Extra Math Library with SIMD support

**************************************************/

#include "math.hpp"

#include <algorithm>   // For std::all_of, std::transform
#include <bit>         // For std::bit_width, std::countl_zero
#include <cmath>       // For std::sqrt
#include <execution>   // For parallel execution policies
#include <functional>  // For std::plus, std::multiplies
#include <limits>      // For std::numeric_limits
#include <numeric>     // For std::gcd, std::lcm

#ifdef _MSC_VER
#include <intrin.h>   // For _umul128 and _BitScanReverse
#include <stdexcept>  // For std::runtime_error
#endif

#include "atom/error/exception.hpp"

// SIMD headers
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#endif

#ifdef ATOM_USE_BOOST
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/simd/pack.hpp>
using boost::simd::pack;
#endif

namespace atom::algorithm {

// Helper function for checking division by zero
inline void checkDivisionByZero(uint64_t divisor) {
    if (divisor == 0) {
        THROW_INVALID_ARGUMENT("Division by zero");
    }
}

// Helper function for input validation
template <typename T>
constexpr void validateInput(T value, T min, T max, const char* errorMsg) {
    if (value < min || value > max) {
        THROW_INVALID_ARGUMENT(errorMsg);
    }
}

#ifdef ATOM_USE_BOOST
auto mulDiv64(uint64_t operand, uint64_t multiplier,
              uint64_t divider) -> uint64_t {
    try {
        checkDivisionByZero(divider);
        boost::multiprecision::uint128_t a = operand;
        boost::multiprecision::uint128_t b = multiplier;
        boost::multiprecision::uint128_t c = divider;
        return static_cast<uint64_t>((a * b) / c);
    } catch (const boost::multiprecision::overflow_error&) {
        THROW_OVERFLOW("Overflow in multiplication before division");
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in mulDiv64: ") + e.what());
    }
}
#endif

#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
auto mulDiv64(uint64_t operand, uint64_t multiplier,
              uint64_t divider) -> uint64_t {
    try {
        checkDivisionByZero(divider);

        __uint128_t a = operand;
        __uint128_t b = multiplier;
        __uint128_t c = divider;
        __uint128_t result = (a * b) / c;

        // Check if result fits in uint64_t
        if (result > std::numeric_limits<uint64_t>::max()) {
            THROW_OVERFLOW("Result exceeds uint64_t range");
        }

        return static_cast<uint64_t>(result);
    } catch (const atom::error::Exception& e) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in mulDiv64: ") + e.what());
    }
}
#elif defined(_MSC_VER)
auto mulDiv64(uint64_t operand, uint64_t multiplier,
              uint64_t divider) -> uint64_t {
    try {
        checkDivisionByZero(divider);

        uint64_t highProd;
        uint64_t lowProd = _umul128(operand, multiplier, &highProd);

        // Check for overflow in multiplication
        if (operand > 0 && multiplier > 0 &&
            highProd > (std::numeric_limits<uint64_t>::max() / operand)) {
            THROW_OVERFLOW("Overflow in multiplication");
        }

        // Fast path for small values that won't overflow
        if (highProd == 0) {
            return lowProd / divider;
        }

        // Normalize divisor
        unsigned long shift = 63 - std::countl_zero(divider);
        uint64_t normDiv = divider << shift;

        // Prepare for division
        highProd = (highProd << shift) | (lowProd >> (64 - shift));
        lowProd <<= shift;

        // Perform division
        uint64_t quotient;
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

auto safeAdd(uint64_t a, uint64_t b) -> uint64_t {
    try {
        uint64_t result;
#ifdef ATOM_USE_BOOST
        boost::multiprecision::uint128_t temp =
            boost::multiprecision::uint128_t(a) + b;
        if (temp > std::numeric_limits<uint64_t>::max()) {
            THROW_OVERFLOW("Overflow in addition");
        }
        result = static_cast<uint64_t>(temp);
#else
        // Check for overflow before addition
        if (b > std::numeric_limits<uint64_t>::max() - a) {
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

auto safeMul(uint64_t a, uint64_t b) -> uint64_t {
    try {
        uint64_t result;
#ifdef ATOM_USE_BOOST
        boost::multiprecision::uint128_t temp =
            boost::multiprecision::uint128_t(a) * b;
        if (temp > std::numeric_limits<uint64_t>::max()) {
            THROW_OVERFLOW("Overflow in multiplication");
        }
        result = static_cast<uint64_t>(temp);
#else
        // Check for overflow before multiplication
        if (a > 0 && b > std::numeric_limits<uint64_t>::max() / a) {
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

auto rotl64(uint64_t n, unsigned int c) noexcept -> uint64_t {
    // Using std::rotl from C++20
    return std::rotl(n, static_cast<int>(c));
}

auto rotr64(uint64_t n, unsigned int c) noexcept -> uint64_t {
    // Using std::rotr from C++20
    return std::rotr(n, static_cast<int>(c));
}

auto clz64(uint64_t x) noexcept -> int {
    // Using std::countl_zero from C++20
    return std::countl_zero(x);
}

auto normalize(uint64_t x) noexcept -> uint64_t {
    if (x == 0) {
        return 0;
    }
    int n = clz64(x);
    return x << n;
}

auto safeSub(uint64_t a, uint64_t b) -> uint64_t {
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

auto safeDiv(uint64_t a, uint64_t b) -> uint64_t {
    try {
        checkDivisionByZero(b);
        return a / b;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in safeDiv: ") + e.what());
    }
}

auto bitReverse64(uint64_t n) noexcept -> uint64_t {
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
    return _byteswap_uint64(n);
#elif defined(__ARM_NEON)
    return vrev64_u8(vcreate_u8(n));
#else
    // Fallback to optimized non-SIMD implementation
#endif
#endif
    // Optimized bit reversal using lookup table approach for smaller chunks
    static const uint8_t lookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                                       0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

    uint64_t result = 0;
    for (int i = 0; i < 16; ++i) {
        result = (result << 4) | lookup[(n >> (i * 4)) & 0xF];
    }
    return result;
}

auto approximateSqrt(uint64_t n) noexcept -> uint64_t {
    if (n == 0 || n == 1) {
        return n;
    }

#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
    return _mm_cvtsd_si64(
        _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(static_cast<double>(n))));
#elif defined(__ARM_NEON)
    float32x2_t x = vdup_n_f32(static_cast<float>(n));
    float32x2_t sqrt_reciprocal = vrsqrte_f32(x);
    // One Newton-Raphson refinement step for better precision
    sqrt_reciprocal =
        vmul_f32(vrsqrts_f32(vmul_f32(x, sqrt_reciprocal), sqrt_reciprocal),
                 sqrt_reciprocal);
    float32x2_t result = vmul_f32(x, sqrt_reciprocal);
    return static_cast<uint64_t>(vget_lane_f32(result, 0));
#else
    // Fallback to Newton-Raphson method
#endif
#endif

    // Optimized Newton-Raphson method
    double x = static_cast<double>(n);
    double y = 1.0;

    // Initial guess based on bit manipulation for faster convergence
    uint64_t i = n;
    i = i >> 1;  // Divide by 2

    // Fast integer square root approximation
    for (uint64_t k = 3; k < 64; k <<= 1) {
        i = (i >> 1) + (n >> k) / i;
    }

    x = static_cast<double>(i);

    // Fine-tune with a few Newton-Raphson iterations
    for (int iter = 0; iter < 3; ++iter) {
        y = n / x;
        x = (x + y) / 2.0;
    }

    return static_cast<uint64_t>(x);
}

auto gcd64(uint64_t a, uint64_t b) noexcept -> uint64_t {
    // Using std::gcd from C++17
    return std::gcd(a, b);
}

auto lcm64(uint64_t a, uint64_t b) -> uint64_t {
    try {
        // Using std::lcm from C++17
        if (a == 0 || b == 0) {
            return 0;  // lcm(0, x) = 0 by convention
        }

        // Check for potential overflow
        uint64_t gcd_val = gcd64(a, b);
        uint64_t first_part = a / gcd_val;  // This division is always exact

        // Check for overflow in multiplication
        if (first_part > std::numeric_limits<uint64_t>::max() / b) {
            THROW_OVERFLOW("Overflow in LCM calculation");
        }

        return first_part * b;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in lcm64: ") + e.what());
    }
}

auto isPowerOfTwo(uint64_t n) noexcept -> bool {
    // Check if n is non-zero and has exactly one bit set
    return n != 0 && (n & (n - 1)) == 0;
}

auto nextPowerOfTwo(uint64_t n) noexcept -> uint64_t {
    if (n == 0) {
        return 1;
    }

    // Fast path for powers of two
    if (isPowerOfTwo(n)) {
        return n;
    }

#if defined(__GNUC__) || defined(__clang__)
    // Use built-in functions for bit manipulation
    return 1ULL << (64 - __builtin_clzll(n));
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse64(&index, n);
    return 1ULL << (index + 1);
#else
    // Fallback to portable implementation
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
#endif
}

// Optimized SIMD vector operations
template <Arithmetic T>
auto parallelVectorAdd(std::span<const T> a,
                       std::span<const T> b) -> std::vector<T> {
    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Vectors must be of equal size");
    }

    std::vector<T> result(a.size());

    // Choose approach based on vector size
    if (a.size() < 1000) {
// For small vectors, use SIMD without threading
#ifdef USE_SIMD
// SIMD implementation based on architecture
#else
        // Fallback to standard algorithm with potential auto-vectorization
        std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                       std::plus<>{});
#endif
    } else {
        // For larger vectors, use parallel execution policy
        std::transform(std::execution::par_unseq, a.begin(), a.end(), b.begin(),
                       result.begin(), std::plus<>{});
    }

    return result;
}

template <Arithmetic T>
auto parallelVectorMul(std::span<const T> a,
                       std::span<const T> b) -> std::vector<T> {
    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Vectors must be of equal size");
    }

    std::vector<T> result(a.size());

    // Choose approach based on vector size
    if (a.size() < 1000) {
// For small vectors, use SIMD without threading
#ifdef USE_SIMD
// SIMD implementation based on architecture
#else
        // Fallback to standard algorithm with potential auto-vectorization
        std::transform(a.begin(), a.end(), b.begin(), result.begin(),
                       std::multiplies<>{});
#endif
    } else {
        // For larger vectors, use parallel execution policy
        std::transform(std::execution::par_unseq, a.begin(), a.end(), b.begin(),
                       result.begin(), std::multiplies<>{});
    }

    return result;
}

// Fast exponentiation using binary exponentiation
template <std::integral T>
auto fastPow(T base, T exponent) noexcept -> T {
    T result = 1;

    // Handle edge cases
    if (exponent < 0) {
        return (base == 1) ? 1 : 0;
    }

    // Binary exponentiation algorithm
    while (exponent > 0) {
        if (exponent & 1) {
            result *= base;
        }
        exponent >>= 1;
        base *= base;
    }

    return result;
}

auto isPrime(uint64_t n) noexcept -> bool {
    if (n <= 1)
        return false;
    if (n <= 3)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;

    // Optimized trial division
    uint64_t limit = approximateSqrt(n);
    for (uint64_t i = 5; i <= limit; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }

    return true;
}

auto generatePrimes(uint64_t limit) -> std::vector<uint64_t> {
    try {
        if (limit > std::numeric_limits<uint32_t>::max()) {
            THROW_INVALID_ARGUMENT("Limit too large for efficient sieve");
        }

        // Use Sieve of Eratosthenes for efficient prime generation
        std::vector<bool> isPrime(limit + 1, true);
        isPrime[0] = isPrime[1] = false;

        uint64_t sqrtLimit = approximateSqrt(limit);

        // Mark non-primes using sieve
        for (uint64_t i = 2; i <= sqrtLimit; ++i) {
            if (isPrime[i]) {
                for (uint64_t j = i * i; j <= limit; j += i) {
                    isPrime[j] = false;
                }
            }
        }

        // Collect primes
        std::vector<uint64_t> primes;
        primes.reserve(limit /
                       10);  // Rough estimate based on prime number theorem

        for (uint64_t i = 2; i <= limit; ++i) {
            if (isPrime[i]) {
                primes.push_back(i);
            }
        }

        return primes;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in generatePrimes: ") +
                            e.what());
    }
}

auto montgomeryMultiply(uint64_t a, uint64_t b, uint64_t n) -> uint64_t {
    try {
        checkDivisionByZero(n);

        // Cannot use Montgomery multiplication if n is even
        if ((n & 1) == 0) {
            // Fallback to standard modular multiplication
            return (a * b) % n;
        }

        // Compute R^2 mod n
        uint64_t r_sq = 0;
        for (int i = 0; i < 128; ++i) {
            r_sq = (r_sq << 1) % n;
        }

        // Convert a and b to Montgomery form
        uint64_t a_mont = (a * r_sq) % n;
        uint64_t b_mont = (b * r_sq) % n;

        // Compute Montgomery multiplication
        uint64_t t = a_mont * b_mont;

        // Convert back from Montgomery form
        uint64_t result = 0;
        for (int i = 0; i < 64; ++i) {
            result = (result + ((t & 1) * n)) >> 1;
            t >>= 1;
        }
        if (result >= n) {
            result -= n;
        }

        return result;
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in montgomeryMultiply: ") +
                            e.what());
    }
}

auto modPow(uint64_t base, uint64_t exponent, uint64_t modulus) -> uint64_t {
    try {
        checkDivisionByZero(modulus);

        if (modulus == 1)
            return 0;
        if (exponent == 0)
            return 1;

        // Use Montgomery multiplication for large moduli
        if (modulus > 1000000ULL && (modulus & 1)) {
            // Compute R = 2^64 mod n
            uint64_t r = 0;

            // Compute R^2 mod n
            uint64_t r_sq = 0;
            for (int i = 0; i < 128; ++i) {
                r_sq = (r_sq << 1) % modulus;
                if (i == 63) {
                    r = r_sq;
                }
            }

            // Convert base to Montgomery form
            uint64_t base_mont = (base * r_sq) % modulus;
            uint64_t result_mont = (1 * r_sq) % modulus;

            while (exponent > 0) {
                if (exponent & 1) {
                    // Multiply result by base using Montgomery multiplication
                    result_mont =
                        montgomeryMultiply(result_mont, base_mont, modulus);
                }
                base_mont = montgomeryMultiply(base_mont, base_mont, modulus);
                exponent >>= 1;
            }

            // Convert back from Montgomery form
            uint64_t inv_r = 1;
            for (int i = 0; i < 64; ++i) {
                if ((inv_r * r) % modulus == 1)
                    break;
                inv_r = (inv_r + modulus) % modulus;
            }

            return (result_mont * inv_r) % modulus;
        } else {
            // Standard binary exponentiation for smaller moduli
            uint64_t result = 1;
            base %= modulus;

            while (exponent > 0) {
                if (exponent & 1) {
                    result = (result * base) % modulus;
                }
                base = (base * base) % modulus;
                exponent >>= 1;
            }

            return result;
        }
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in modPow: ") + e.what());
    }
}

// Explicit template instantiations
template auto parallelVectorAdd(std::span<const int>,
                                std::span<const int>) -> std::vector<int>;
template auto parallelVectorAdd(std::span<const float>,
                                std::span<const float>) -> std::vector<float>;
template auto parallelVectorAdd(std::span<const double>,
                                std::span<const double>) -> std::vector<double>;

template auto parallelVectorMul(std::span<const int>,
                                std::span<const int>) -> std::vector<int>;
template auto parallelVectorMul(std::span<const float>,
                                std::span<const float>) -> std::vector<float>;
template auto parallelVectorMul(std::span<const double>,
                                std::span<const double>) -> std::vector<double>;

template auto fastPow(int, int) noexcept -> int;
template auto fastPow(long, long) noexcept -> long;
template auto fastPow(long long, long long) noexcept -> long long;

}  // namespace atom::algorithm
