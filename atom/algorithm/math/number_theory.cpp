/*
 * number_theory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Number theory functions - implementations

**************************************************/

#include "number_theory.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "safe_math.hpp"

// SIMD headers
#ifdef USE_SIMD
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#endif

#include "atom/error/exception.hpp"

namespace atom::algorithm {

namespace {
// Thread-local cache for frequently used values
thread_local std::vector<bool> isPrimeCache;
thread_local bool isPrimeCacheInitialized = false;
constexpr usize PRIME_CACHE_SIZE = 1024;

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

        // Use 128-bit multiplication to avoid overflow
        // (a * b) mod n
        __uint128_t prod = static_cast<__uint128_t>(a % n) * (b % n);
        return static_cast<u64>(prod % n);
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
                // No need to check if inv_r < 0 since it's unsigned
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

}  // namespace atom::algorithm
