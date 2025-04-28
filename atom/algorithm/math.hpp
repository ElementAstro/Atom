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
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <vector>

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
     * @return std::shared_ptr<const std::vector<uint64_t>> Thread-safe shared pointer to prime vector
     */
    [[nodiscard]] std::shared_ptr<const std::vector<uint64_t>> getCachedPrimes(uint64_t limit);

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
    std::unordered_map<uint64_t, std::shared_ptr<std::vector<uint64_t>>> primeCache_;
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
[[nodiscard]] auto mulDiv64(uint64_t operant, uint64_t multiplier,
                            uint64_t divider) -> uint64_t;

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
[[nodiscard]] constexpr auto safeAdd(uint64_t a, uint64_t b) -> uint64_t;

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
[[nodiscard]] constexpr auto safeMul(uint64_t a, uint64_t b) -> uint64_t;

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
[[nodiscard]] constexpr auto rotl64(uint64_t n, unsigned int c) noexcept -> uint64_t;

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
[[nodiscard]] constexpr auto rotr64(uint64_t n, unsigned int c) noexcept -> uint64_t;

/**
 * @brief Counts the leading zeros in a 64-bit integer.
 *
 * This function counts the number of leading zeros in a 64-bit integer.
 * Uses std::countl_zero from C++20.
 *
 * @param x The 64-bit integer to count leading zeros in.
 * @return The number of leading zeros in the 64-bit integer.
 */
[[nodiscard]] constexpr auto clz64(uint64_t x) noexcept -> int;

/**
 * @brief Normalizes a 64-bit integer.
 *
 * This function normalizes a 64-bit integer by shifting it to the left until
 * the most significant bit is set.
 *
 * @param x The 64-bit integer to normalize.
 * @return The normalized 64-bit integer.
 */
[[nodiscard]] constexpr auto normalize(uint64_t x) noexcept -> uint64_t;

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
[[nodiscard]] constexpr auto safeSub(uint64_t a, uint64_t b) -> uint64_t;

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
[[nodiscard]] constexpr auto safeDiv(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Calculates the bitwise reverse of a 64-bit integer.
 *
 * This function calculates the bitwise reverse of a 64-bit integer.
 * Uses optimized SIMD implementation when available.
 *
 * @param n The 64-bit integer to reverse.
 * @return The bitwise reverse of the 64-bit integer.
 */
[[nodiscard]] auto bitReverse64(uint64_t n) noexcept -> uint64_t;

/**
 * @brief Approximates the square root of a 64-bit integer.
 *
 * This function approximates the square root of a 64-bit integer using a fast
 * algorithm. Uses SIMD optimization when available.
 *
 * @param n The 64-bit integer for which to approximate the square root.
 * @return The approximate square root of the 64-bit integer.
 */
[[nodiscard]] auto approximateSqrt(uint64_t n) noexcept -> uint64_t;

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
[[nodiscard]] constexpr auto gcd64(uint64_t a, uint64_t b) noexcept -> uint64_t;

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
[[nodiscard]] auto lcm64(uint64_t a, uint64_t b) -> uint64_t;

/**
 * @brief Checks if a 64-bit integer is a power of two.
 *
 * This function checks if a 64-bit integer is a power of two.
 * Uses std::has_single_bit from C++20.
 *
 * @param n The 64-bit integer to check.
 * @return True if the 64-bit integer is a power of two, false otherwise.
 */
[[nodiscard]] constexpr auto isPowerOfTwo(uint64_t n) noexcept -> bool;

/**
 * @brief Calculates the next power of two for a 64-bit integer.
 *
 * This function calculates the next power of two for a 64-bit integer.
 * Uses std::bit_ceil from C++20 when available.
 *
 * @param n The 64-bit integer for which to calculate the next power of two.
 * @return The next power of two for the 64-bit integer.
 */
[[nodiscard]] constexpr auto nextPowerOfTwo(uint64_t n) noexcept -> uint64_t;

/**
 * @brief Parallel addition of two vectors using SIMD
 *
 * Uses lock-free algorithms and SIMD instructions when available.
 * Automatically chooses between parallel and sequential implementation
 * based on input size.
 *
 * @tparam T Arithmetic type
 * @param a First vector
 * @param b Second vector
 * @return std::vector<T> Result of addition
 */
template <Arithmetic T>
[[nodiscard]] auto parallelVectorAdd(std::span<const T> a,
                                     std::span<const T> b) -> std::vector<T>;

/**
 * @brief Parallel multiplication of two vectors using SIMD
 *
 * Uses lock-free algorithms and SIMD instructions when available.
 * Automatically chooses between parallel and sequential implementation
 * based on input size.
 *
 * @tparam T Arithmetic type
 * @param a First vector
 * @param b Second vector
 * @return std::vector<T> Result of multiplication
 */
template <Arithmetic T>
[[nodiscard]] auto parallelVectorMul(std::span<const T> a,
                                     std::span<const T> b) -> std::vector<T>;

/**
 * @brief Fast exponentiation for integral types
 *
 * @tparam T Integral type
 * @param base The base value
 * @param exponent The exponent value
 * @return T The result of base^exponent
 */
template <std::integral T>
[[nodiscard]] constexpr auto fastPow(T base, T exponent) noexcept -> T;

/**
 * @brief Prime number checker using optimized trial division
 *
 * Uses cache for repeated checks of the same value.
 *
 * @param n Number to check
 * @return true If n is prime
 * @return false If n is not prime
 */
[[nodiscard]] auto isPrime(uint64_t n) noexcept -> bool;

/**
 * @brief Generates prime numbers up to a limit using the Sieve of Eratosthenes
 *
 * Uses thread-safe caching for repeated calls with the same limit.
 *
 * @param limit Upper limit for prime generation
 * @return std::vector<uint64_t> Vector of primes up to limit
 */
[[nodiscard]] auto generatePrimes(uint64_t limit) -> std::vector<uint64_t>;

/**
 * @brief Montgomery modular multiplication
 *
 * Uses optimized implementation for different platforms.
 *
 * @param a First operand
 * @param b Second operand
 * @param n Modulus
 * @return uint64_t (a * b) mod n
 */
[[nodiscard]] auto montgomeryMultiply(uint64_t a, uint64_t b,
                                      uint64_t n) -> uint64_t;

/**
 * @brief Modular exponentiation using Montgomery reduction
 *
 * Uses optimized implementation with compile-time selection
 * between regular and Montgomery algorithms.
 *
 * @param base Base value
 * @param exponent Exponent value
 * @param modulus Modulus
 * @return uint64_t (base^exponent) mod modulus
 */
[[nodiscard]] auto modPow(uint64_t base, uint64_t exponent,
                          uint64_t modulus) -> uint64_t;

/**
 * @brief Generate a cryptographically secure random number
 * 
 * @return std::optional<uint64_t> Random value, or nullopt if generation failed
 */
[[nodiscard]] auto secureRandom() noexcept -> std::optional<uint64_t>;

/**
 * @brief Generate a random number in the specified range
 * 
 * @param min Minimum value (inclusive)
 * @param max Maximum value (inclusive)
 * @return std::optional<uint64_t> Random value in range, or nullopt if generation failed
 */
[[nodiscard]] auto randomInRange(uint64_t min, uint64_t max) noexcept -> std::optional<uint64_t>;

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
    [[nodiscard]] void* allocate(size_t size);
    
    /**
     * @brief Return memory to the pool
     * 
     * @param ptr Pointer to memory
     * @param size Size of the allocation
     */
    void deallocate(void* ptr, size_t size) noexcept;

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
    
    [[nodiscard]] T* allocate(std::size_t n);
    void deallocate(T* p, std::size_t n) noexcept;
    
    template <typename U>
    bool operator==(const MathAllocator<U>&) const noexcept { return true; }
    
    template <typename U>
    bool operator!=(const MathAllocator<U>&) const noexcept { return false; }
};

}  // namespace atom::algorithm

#endif
