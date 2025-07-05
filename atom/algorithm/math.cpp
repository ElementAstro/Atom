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

#include <algorithm>  // For std::all_of, std::transform
#include <bit>        // For std::bit_width, std::countl_zero, std::bit_ceil
#include <limits>     // For std::numeric_limits
#include <memory_resource>  // For pmr utilities
#include <mutex>
#include <random>         // For secure random number generation
#include <shared_mutex>   // For std::shared_mutex
#include <unordered_map>  // For cache implementation

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
#include <boost/pool/object_pool.hpp>
#include <boost/simd/pack.hpp>
using boost::simd::pack;
#endif

namespace atom::algorithm {

namespace {
// Thread-local cache for frequently used values
thread_local std::vector<bool> isPrimeCache;
thread_local bool isPrimeCacheInitialized = false;
constexpr usize PRIME_CACHE_SIZE = 1024;

// Helper function for input validation with compile-time evaluation if possible
template <typename T>
constexpr void validateInput(T value, T min, T max, const char* errorMsg) {
    if (value < min || value > max) {
        THROW_INVALID_ARGUMENT(errorMsg);
    }
}

// RAII wrapper for memory allocation from MathMemoryPool
template <typename T>
class PooledMemory {
public:
    explicit PooledMemory(usize count)
        : size_(count * sizeof(T)),
          ptr_(static_cast<T*>(MathMemoryPool::getInstance().allocate(size_))) {
    }

    ~PooledMemory() {
        if (ptr_) {
            MathMemoryPool::getInstance().deallocate(ptr_, size_);
        }
    }

    // Disable copy operations
    PooledMemory(const PooledMemory&) = delete;
    PooledMemory& operator=(const PooledMemory&) = delete;

    // Enable move operations
    PooledMemory(PooledMemory&& other) noexcept
        : size_(other.size_), ptr_(other.ptr_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
    }

    PooledMemory& operator=(PooledMemory&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                MathMemoryPool::getInstance().deallocate(ptr_, size_);
            }
            size_ = other.size_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    [[nodiscard]] operator T*() const noexcept { return ptr_; }

private:
    usize size_;
    T* ptr_;
};

// Initialize thread-local prime cache
void initPrimeCache() {
    if (!isPrimeCacheInitialized) {
        isPrimeCache.resize(PRIME_CACHE_SIZE, true);
        isPrimeCache[0] = isPrimeCache[1] = false;

        for (usize i = 2; i * i < PRIME_CACHE_SIZE; ++i) {
            if (isPrimeCache[i]) {
                for (usize j = i * i; j < PRIME_CACHE_SIZE; j += i) {
                    isPrimeCache[j] = false;
                }
            }
        }

        isPrimeCacheInitialized = true;
    }
}
}  // anonymous namespace

// Implementation of MathCache
MathCache& MathCache::getInstance() noexcept {
    static MathCache instance;
    return instance;
}

std::shared_ptr<const std::vector<u64>> MathCache::getCachedPrimes(u64 limit) {
    // Use shared lock for reading
    {
        std::shared_lock lock(mutex_);
        auto it = primeCache_.find(limit);
        if (it != primeCache_.end()) {
            return it->second;
        }
    }

    // Generate primes (outside the lock to avoid contention)
    auto primes = std::make_shared<std::vector<u64>>();

    // Generate prime numbers using Sieve of Eratosthenes
    std::vector<bool> isPrime(limit + 1, true);
    isPrime[0] = isPrime[1] = false;

    u64 sqrtLimit = approximateSqrt(limit);

    for (u64 i = 2; i <= sqrtLimit; ++i) {
        if (isPrime[i]) {
            for (u64 j = i * i; j <= limit; j += i) {
                isPrime[j] = false;
            }
        }
    }

    primes->reserve(limit / 10);  // Reserve estimated capacity
    for (u64 i = 2; i <= limit; ++i) {
        if (isPrime[i]) {
            primes->push_back(i);
        }
    }

    // Use exclusive lock for writing
    {
        std::unique_lock lock(mutex_);
        // Check again to handle race condition
        auto it = primeCache_.find(limit);
        if (it != primeCache_.end()) {
            return it->second;
        }

        primeCache_[limit] = primes;
        return primes;
    }
}

void MathCache::clear() noexcept {
    std::unique_lock lock(mutex_);
    primeCache_.clear();
}

// MathMemoryPool implementation
namespace {

// Memory pools for different block sizes
#ifdef ATOM_USE_BOOST
boost::object_pool<char[SMALL_BLOCK_SIZE]> smallPool;
boost::object_pool<char[MEDIUM_BLOCK_SIZE]> mediumPool;
boost::object_pool<char[LARGE_BLOCK_SIZE]> largePool;
#else
std::pmr::synchronized_pool_resource memoryPool;
#endif
}  // namespace

MathMemoryPool& MathMemoryPool::getInstance() noexcept {
    static MathMemoryPool instance;
    return instance;
}

void* MathMemoryPool::allocate(usize size) {
#ifdef ATOM_USE_BOOST
    std::unique_lock lock(mutex_);
    if (size <= SMALL_BLOCK_SIZE) {
        return smallPool.malloc();
    } else if (size <= MEDIUM_BLOCK_SIZE) {
        return mediumPool.malloc();
    } else if (size <= LARGE_BLOCK_SIZE) {
        return largePool.malloc();
    } else {
        return ::operator new(size);
    }
#else
    return memoryPool.allocate(size);
#endif
}

void MathMemoryPool::deallocate(void* ptr, usize size) noexcept {
#ifdef ATOM_USE_BOOST
    std::unique_lock lock(mutex_);
    if (size <= SMALL_BLOCK_SIZE) {
        smallPool.free(static_cast<char(*)[SMALL_BLOCK_SIZE]>(ptr));
    } else if (size <= MEDIUM_BLOCK_SIZE) {
        mediumPool.free(static_cast<char(*)[MEDIUM_BLOCK_SIZE]>(ptr));
    } else if (size <= LARGE_BLOCK_SIZE) {
        largePool.free(static_cast<char(*)[LARGE_BLOCK_SIZE]>(ptr));
    } else {
        ::operator delete(ptr);
    }
#else
    memoryPool.deallocate(ptr, size);
#endif
}

MathMemoryPool::~MathMemoryPool() {
    // Cleanup is automatically handled by member destructors
}

// MathAllocator implementation
template <typename T>
T* MathAllocator<T>::allocate(usize n) {
    if (n > std::numeric_limits<usize>::max() / sizeof(T)) {
        throw std::bad_alloc();
    }

    void* ptr = MathMemoryPool::getInstance().allocate(n * sizeof(T));
    if (!ptr) {
        throw std::bad_alloc();
    }

    return static_cast<T*>(ptr);
}

template <typename T>
void MathAllocator<T>::deallocate(T* p, usize n) noexcept {
    MathMemoryPool::getInstance().deallocate(p, n * sizeof(T));
}

// Generate random numbers
auto secureRandom() noexcept -> std::optional<u64> {
    try {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist;
        return dist(gen);
    } catch (...) {
        return std::nullopt;
    }
}

auto randomInRange(u64 min, u64 max) noexcept -> std::optional<u64> {
    if (min > max) {
        return std::nullopt;
    }

    try {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<u64> dist(min, max);
        return dist(gen);
    } catch (...) {
        return std::nullopt;
    }
}

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

auto bitReverse64(u64 n) noexcept -> u64 {
    // Use efficient platform-specific intrinsics for bit reversal
    if constexpr (std::endian::native == std::endian::little) {
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
        return _byteswap_uint64(n);
#elif defined(__ARM_NEON)
        return vrev64_u8(vcreate_u8(n));
#endif
#endif
    }

    // Optimized implementation using lookup table and constexpr evaluation
    static constexpr u8 lookup[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                                      0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};

    u64 result = 0;
    for (i32 i = 0; i < 16; ++i) {
        result = (result << 4) | lookup[(n >> (i * 4)) & 0xF];
    }
    return result;
}

auto approximateSqrt(u64 n) noexcept -> u64 {
    if (n <= 1) {
        return n;
    }

// Use optimal implementation based on available hardware instructions
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
    return _mm_cvtsd_si64(
        _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(static_cast<double>(n))));
#elif defined(__ARM_NEON)
    float32x2_t x = vdup_n_f32(static_cast<float>(n));
    float32x2_t sqrt_reciprocal = vrsqrte_f32(x);
    // Newton-Raphson refinement for better precision
    sqrt_reciprocal =
        vmul_f32(vrsqrts_f32(vmul_f32(x, sqrt_reciprocal), sqrt_reciprocal),
                 sqrt_reciprocal);
    float32x2_t result = vmul_f32(x, sqrt_reciprocal);
    return static_cast<u64>(vget_lane_f32(result, 0));
#else
    // Fall back to optimized integer implementation
#endif
#endif

    // Fast integer Newton-Raphson method
    u64 x = n;
    u64 y = (x + 1) / 2;

    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }

    return x;
}

auto lcm64(u64 a, u64 b) -> u64 {
    try {
        // Handle edge cases explicitly
        if (a == 0 || b == 0) {
            return 0;  // lcm(0, x) = 0 by convention
        }

        // Use std::lcm from C++17 for the actual computation with overflow
        // check
        u64 gcd_val = gcd64(a, b);
        u64 first_part = a / gcd_val;  // This division is always exact

        // Check for overflow in multiplication
        if (first_part > std::numeric_limits<u64>::max() / b) {
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

auto isPrime(u64 n) noexcept -> bool {
    // Initialize thread-local cache if needed
    initPrimeCache();

    // Use cache for small numbers
    if (n < PRIME_CACHE_SIZE) {
        return isPrimeCache[n];
    }

    if (n <= 1)
        return false;
    if (n <= 3)
        return true;
    if (n % 2 == 0 || n % 3 == 0)
        return false;

    // Optimized trial division
    u64 limit = approximateSqrt(n);
    for (u64 i = 5; i <= limit; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }

    return true;
}

auto generatePrimes(u64 limit) -> std::vector<u64> {
    try {
        // Input validation
        if (limit > std::numeric_limits<u32>::max()) {
            THROW_INVALID_ARGUMENT("Limit too large for efficient sieve");
        }

        // Use thread-safe cache to avoid redundant calculations
        return *MathCache::getInstance().getCachedPrimes(limit);
    } catch (const atom::error::Exception&) {
        // Re-throw atom exceptions
        throw;
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Error in generatePrimes: ") +
                            e.what());
    }
}

auto montgomeryMultiply(u64 a, u64 b, u64 n) -> u64 {
    try {
        if (isDivisionByZero(n)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        // Cannot use Montgomery multiplication if n is even
        if ((n & 1) == 0) {
            // Fallback to standard modular multiplication
            return (a * b) % n;
        }

        // Compute R^2 mod n
        u64 r_sq = 0;
        for (i32 i = 0; i < 128; ++i) {
            r_sq = (r_sq << 1) % n;
        }

        // Convert a and b to Montgomery form
        u64 a_mont = (a * r_sq) % n;
        u64 b_mont = (b * r_sq) % n;

        // Compute Montgomery multiplication
        u64 t = a_mont * b_mont;

        // Convert back from Montgomery form
        u64 result = 0;
        for (i32 i = 0; i < 64; ++i) {
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

auto modPow(u64 base, u64 exponent, u64 modulus) -> u64 {
    try {
        if (isDivisionByZero(modulus)) {
            THROW_INVALID_ARGUMENT("Division by zero");
        }

        if (modulus == 1)
            return 0;
        if (exponent == 0)
            return 1;

        // Use Montgomery multiplication for large moduli
        if (modulus > 1000000ULL && (modulus & 1)) {
            // Compute R = 2^64 mod n
            u64 r = 0;

            // Compute R^2 mod n
            u64 r_sq = 0;
            for (i32 i = 0; i < 128; ++i) {
                r_sq = (r_sq << 1) % modulus;
                if (i == 63) {
                    r = r_sq;
                }
            }

            // Convert base to Montgomery form
            u64 base_mont = (base * r_sq) % modulus;
            u64 result_mont = (1 * r_sq) % modulus;

            while (exponent > 0) {
                if (exponent & 1) {
                    // Multiply result by base using Montgomery multiplication
                    result_mont =
                        montgomeryMultiply(result_mont, base_mont, modulus);
                }
                base_mont = montgomeryMultiply(base_mont, base_mont, modulus);
                exponent >>= 1;
            }

            // Convert back from Montgomery form (improved implementation)
            u64 inv_r = 1;
            // Use extended Euclidean algorithm to compute inverse more
            // efficiently
            u64 u = modulus, v = 1;
            u64 s = r, t = 0;

            while (s != 0) {
                u64 q = u / s;
                std::swap(u -= q * s, s);
                std::swap(v -= q * t, t);
            }

            // If u is 1, then v is the inverse of r mod n
            if (u == 1) {
                inv_r = v % modulus;
                if (inv_r < 0)
                    inv_r += modulus;
            }

            return (result_mont * inv_r) % modulus;
        } else {
            // Standard binary exponentiation for smaller moduli
            u64 result = 1;
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

// Explicit template instantiations for MathAllocator
template class MathAllocator<i32>;
template class MathAllocator<f32>;
template class MathAllocator<f64>;
template class MathAllocator<u64>;

std::vector<uint64_t> parallelVectorAdd(const std::vector<uint64_t>& a,
                                        const std::vector<uint64_t>& b) {
    if (a.size() != b.size()) {
        THROW_INVALID_ARGUMENT("Input vectors must have the same length");
    }
    std::vector<uint64_t> result(a.size());
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

}  // namespace atom::algorithm
