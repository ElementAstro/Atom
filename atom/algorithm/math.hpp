/*
 * math.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Extra Math Library

**************************************************/

#ifndef ATOM_ALGORITHM_MATH_HPP
#define ATOM_ALGORITHM_MATH_HPP

#include <concepts>
#include <memory>
#include <numeric>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

template <typename T>
concept UnsignedIntegral = std::unsigned_integral<T>;

template <typename T>
concept Arithmetic = std::integral<T> || std::floating_point<T>;

/**
 * @brief Thread-safe cache for math computations
 *
 * A singleton class that provides thread-safe caching for expensive
 * mathematical operations.
 */
class MathCache {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the singleton instance
     */
    static MathCache& getInstance() noexcept;

    /**
     * @brief Get a cached prime number vector up to the specified limit
     *
     * @param limit Upper bound for prime generation
     * @return std::shared_ptr<const std::vector<u64>> Thread-safe shared
     * pointer to prime vector
     */
    [[nodiscard]] std::shared_ptr<const std::vector<u64>> getCachedPrimes(
        u64 limit);

    /**
     * @brief Clear all cached values
     */
    void clear() noexcept;

private:
    MathCache() = default;
    ~MathCache() = default;
    MathCache(const MathCache&) = delete;
    MathCache& operator=(const MathCache&) = delete;
    MathCache(MathCache&&) = delete;
    MathCache& operator=(MathCache&&) = delete;

    std::shared_mutex mutex_;
    std::unordered_map<u64, std::shared_ptr<std::vector<u64>>> primeCache_;
};

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
 * @brief Rotates a 64-bit integer to the left.
 *
 * This function rotates a 64-bit integer to the left by a specified number of
 * bits. Uses std::rotl from C++20.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] constexpr auto rotl64(u64 n, u32 c) noexcept -> u64 {
    // Using std::rotl from C++20
    return std::rotl(n, static_cast<int>(c));
}

/**
 * @brief Rotates a 64-bit integer to the right.
 *
 * This function rotates a 64-bit integer to the right by a specified number of
 * bits. Uses std::rotr from C++20.
 *
 * @param n The 64-bit integer to rotate.
 * @param c The number of bits to rotate.
 * @return The rotated 64-bit integer.
 */
[[nodiscard]] constexpr auto rotr64(u64 n, u32 c) noexcept -> u64 {
    // Using std::rotr from C++20
    return std::rotr(n, static_cast<int>(c));
}

/**
 * @brief Counts the leading zeros in a 64-bit integer.
 *
 * This function counts the number of leading zeros in a 64-bit integer.
 * Uses std::countl_zero from C++20.
 *
 * @param x The 64-bit integer to count leading zeros in.
 * @return The number of leading zeros in the 64-bit integer.
 */
[[nodiscard]] constexpr auto clz64(u64 x) noexcept -> i32 {
    // Using std::countl_zero from C++20
    return std::countl_zero(x);
}

/**
 * @brief Normalizes a 64-bit integer.
 *
 * This function normalizes a 64-bit integer by shifting it to the left until
 * the most significant bit is set.
 *
 * @param x The 64-bit integer to normalize.
 * @return The normalized 64-bit integer.
 */
[[nodiscard]] constexpr auto normalize(u64 x) noexcept -> u64 {
    if (x == 0) {
        return 0;
    }
    i32 n = clz64(x);
    return x << n;
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

/**
 * @brief Calculates the bitwise reverse of a 64-bit integer.
 *
 * This function calculates the bitwise reverse of a 64-bit integer.
 * Uses optimized SIMD implementation when available.
 *
 * @param n The 64-bit integer to reverse.
 * @return The bitwise reverse of the 64-bit integer.
 */
[[nodiscard]] auto bitReverse64(u64 n) noexcept -> u64;

/**
 * @brief Approximates the square root of a 64-bit integer.
 *
 * This function approximates the square root of a 64-bit integer using a fast
 * algorithm. Uses SIMD optimization when available.
 *
 * @param n The 64-bit integer for which to approximate the square root.
 * @return The approximate square root of the 64-bit integer.
 */
[[nodiscard]] auto approximateSqrt(u64 n) noexcept -> u64;

/**
 * @brief Calculates the greatest common divisor (GCD) of two 64-bit integers.
 *
 * This function calculates the greatest common divisor (GCD) of two 64-bit
 * integers using std::gcd.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The greatest common divisor of the two 64-bit integers.
 */
[[nodiscard]] constexpr auto gcd64(u64 a, u64 b) noexcept -> u64 {
    // Using std::gcd from C++17, which is constexpr in C++20
    return std::gcd(a, b);
}

/**
 * @brief Calculates the least common multiple (LCM) of two 64-bit integers.
 *
 * This function calculates the least common multiple (LCM) of two 64-bit
 * integers using std::lcm with overflow checking.
 *
 * @param a The first 64-bit integer.
 * @param b The second 64-bit integer.
 * @return The least common multiple of the two 64-bit integers.
 * @throws atom::error::OverflowException if the operation would overflow.
 */
[[nodiscard]] auto lcm64(u64 a, u64 b) -> u64;

/**
 * @brief Checks if a 64-bit integer is a power of two.
 *
 * This function checks if a 64-bit integer is a power of two.
 * Uses std::has_single_bit from C++20.
 *
 * @param n The 64-bit integer to check.
 * @return True if the 64-bit integer is a power of two, false otherwise.
 */
[[nodiscard]] constexpr auto isPowerOfTwo(u64 n) noexcept -> bool {
    // Using C++20 std::has_single_bit
    return n != 0 && std::has_single_bit(n);
}

/**
 * @brief Calculates the next power of two for a 64-bit integer.
 *
 * This function calculates the next power of two for a 64-bit integer.
 * Uses std::bit_ceil from C++20 when available.
 *
 * @param n The 64-bit integer for which to calculate the next power of two.
 * @return The next power of two for the 64-bit integer.
 */
[[nodiscard]] constexpr auto nextPowerOfTwo(u64 n) noexcept -> u64 {
    if (n == 0) {
        return 1;
    }

    // Fast path for powers of two
    if (isPowerOfTwo(n)) {
        return n;
    }

    // Use C++20 std::bit_ceil
    return std::bit_ceil(n);
}

/**
 * @brief Fast exponentiation for integral types
 *
 * @tparam T Integral type
 * @param base The base value
 * @param exponent The exponent value
 * @return T The result of base^exponent
 */
template <std::integral T>
[[nodiscard]] constexpr auto fastPow(T base, T exponent) noexcept -> T {
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

/**
 * @brief Prime number checker using optimized trial division
 *
 * Uses cache for repeated checks of the same value.
 *
 * @param n Number to check
 * @return true If n is prime
 * @return false If n is not prime
 */
[[nodiscard]] auto isPrime(u64 n) noexcept -> bool;

/**
 * @brief Generates prime numbers up to a limit using the Sieve of Eratosthenes
 *
 * Uses thread-safe caching for repeated calls with the same limit.
 *
 * @param limit Upper limit for prime generation
 * @return std::vector<u64> Vector of primes up to limit
 */
[[nodiscard]] auto generatePrimes(u64 limit) -> std::vector<u64>;

/**
 * @brief Montgomery modular multiplication
 *
 * Uses optimized implementation for different platforms.
 *
 * @param a First operand
 * @param b Second operand
 * @param n Modulus
 * @return u64 (a * b) mod n
 */
[[nodiscard]] auto montgomeryMultiply(u64 a, u64 b, u64 n) -> u64;

/**
 * @brief Modular exponentiation using Montgomery reduction
 *
 * Uses optimized implementation with compile-time selection
 * between regular and Montgomery algorithms.
 *
 * @param base Base value
 * @param exponent Exponent value
 * @param modulus Modulus
 * @return u64 (base^exponent) mod modulus
 */
[[nodiscard]] auto modPow(u64 base, u64 exponent, u64 modulus) -> u64;

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
[[nodiscard]] auto randomInRange(u64 min, u64 max) noexcept
    -> std::optional<u64>;

/**
 * @brief Custom memory pool for efficient allocation in math operations
 */
class MathMemoryPool {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the singleton instance
     */
    static MathMemoryPool& getInstance() noexcept;

    /**
     * @brief Allocate memory from the pool
     *
     * @param size Size in bytes to allocate
     * @return void* Pointer to allocated memory
     */
    [[nodiscard]] void* allocate(usize size);

    /**
     * @brief Return memory to the pool
     *
     * @param ptr Pointer to memory
     * @param size Size of the allocation
     */
    void deallocate(void* ptr, usize size) noexcept;

private:
    MathMemoryPool() = default;
    ~MathMemoryPool();
    MathMemoryPool(const MathMemoryPool&) = delete;
    MathMemoryPool& operator=(const MathMemoryPool&) = delete;
    MathMemoryPool(MathMemoryPool&&) = delete;
    MathMemoryPool& operator=(MathMemoryPool&&) = delete;

    std::shared_mutex mutex_;
    // Implementation details hidden
};

/**
 * @brief Custom allocator that uses MathMemoryPool
 *
 * @tparam T Type to allocate
 */
template <typename T>
class MathAllocator {
public:
    using value_type = T;

    MathAllocator() noexcept = default;

    template <typename U>
    MathAllocator(const MathAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(usize n);
    void deallocate(T* p, usize n) noexcept;

    template <typename U>
    bool operator==(const MathAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const MathAllocator<U>&) const noexcept {
        return false;
    }
};

/**
 * @brief 并行向量加法
 * @param a 输入向量a
 * @param b 输入向量b
 * @return 每个元素为a[i]+b[i]的新向量
 * @throws atom::error::InvalidArgumentException 如果长度不一致
 */
[[nodiscard]] std::vector<uint64_t> parallelVectorAdd(
    const std::vector<uint64_t>& a,
    const std::vector<uint64_t>& b);

}  // namespace atom::algorithm

#endif
