/*
 * hash_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-28

Description: Core hash functions, concepts, and utilities including FNV-1a
             hash algorithm with SIMD optimizations, hash combining, and
             hash verification.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_HASH_BASE_HPP
#define ATOM_ALGORITHM_HASH_HASH_BASE_HPP

#include <functional>

#include "atom/algorithm/common/concepts.hpp"    // Hashable
#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/algorithm/core/simd_utils.hpp"    // ATOM_SIMD_* macros + SIMD headers

#ifdef ATOM_USE_BOOST
#include <boost/functional/hash.hpp>
#endif

namespace atom::algorithm {

// Hashable concept is now defined in atom/algorithm/common/concepts.hpp

/**
 * @brief Enumeration of available hash algorithms
 *
 * Currently implemented:
 * - STD: Standard library std::hash
 * - FNV1A: FNV-1a hash algorithm
 *
 * Reserved for future implementation:
 * - XXHASH: xxHash (fast non-cryptographic hash)
 * - CITYHASH: CityHash (Google's string hashing)
 * - MURMUR3: MurmurHash3 (fast non-cryptographic hash)
 */
enum class HashAlgorithm {
    STD,       // Standard library hash (implemented)
    FNV1A,     // FNV-1a (implemented)
    XXHASH,    // xxHash (reserved)
    CITYHASH,  // CityHash (reserved)
    MURMUR3    // MurmurHash3 (reserved)
};

/**
 * @brief Computes a hash value for a null-terminated string using FNV-1a
 * algorithm. Optimized with SIMD instructions when available.
 *
 * @param str Pointer to the null-terminated string to hash.
 * @param basis Initial basis value for hashing.
 * @return constexpr usize Hash value of the string.
 */
constexpr auto hash(const char* str, usize basis = 2166136261u) noexcept
    -> usize {
#if defined(__AVX2__)
    __m256i hash_vec = _mm256_set1_epi64x(basis);
    const __m256i prime = _mm256_set1_epi64x(16777619u);

    while (*str != '\0') {
        __m256i char_vec = _mm256_set1_epi64x(*str);
        hash_vec = _mm256_xor_si256(hash_vec, char_vec);
        hash_vec = _mm256_mullo_epi64(hash_vec, prime);
        ++str;
    }

    return _mm256_extract_epi64(hash_vec, 0);
#else
    usize hash_val = basis;
    while (*str != '\0') {
        hash_val ^= static_cast<usize>(*str);
        hash_val *= 16777619u;
        ++str;
    }
    return hash_val;
#endif
}

/**
 * @brief Computes a hash value for data with specified length using FNV-1a.
 *
 * @param data Pointer to the data to hash.
 * @param length Length of data in bytes.
 * @param basis Initial basis value for hashing.
 * @return usize Hash value of the data.
 */
inline auto hash(const char* data, usize length,
                 usize basis = 2166136261u) noexcept -> usize {
    usize hash_val = basis;
    for (usize i = 0; i < length; ++i) {
        hash_val ^= static_cast<usize>(static_cast<unsigned char>(data[i]));
        hash_val *= 16777619u;
    }
    return hash_val;
}

#ifdef ATOM_USE_BOOST
/**
 * @brief Combines two hash values into one using Boost's hash_combine.
 *
 * @param seed The initial hash value.
 * @param hash The hash value to combine with the seed.
 */
inline void hashCombine(usize& seed, usize hash) noexcept {
    boost::hash_combine(seed, hash);
}
#else
/**
 * @brief Combines two hash values into one.
 *
 * This function implements the hash combining technique proposed by Boost.
 * Optimized with SIMD instructions when available.
 *
 * @param seed The initial hash value (modified in place).
 * @param hash The hash value to combine with the seed.
 */
inline void hashCombine(usize& seed, usize hash) noexcept {
#if defined(__AVX2__)
    __m256i seed_vec = _mm256_set1_epi64x(seed);
    __m256i hash_vec = _mm256_set1_epi64x(hash);
    __m256i magic = _mm256_set1_epi64x(0x9e3779b9);
    __m256i result = _mm256_xor_si256(
        seed_vec,
        _mm256_add_epi64(
            hash_vec,
            _mm256_add_epi64(
                magic, _mm256_add_epi64(_mm256_slli_epi64(seed_vec, 6),
                                        _mm256_srli_epi64(seed_vec, 2)))));
    seed = _mm256_extract_epi64(result, 0);
#else
    // Fallback to original implementation
    seed ^= (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
#endif
}
#endif

/**
 * @brief Verifies if two hash values match
 *
 * @param hash1 First hash value
 * @param hash2 Second hash value
 * @param tolerance Allowed difference (for fuzzy matching)
 * @return bool True if hashes match within tolerance
 */
inline auto verifyHash(usize hash1, usize hash2, usize tolerance = 0) noexcept
    -> bool {
    return (hash1 == hash2) ||
           (tolerance > 0 &&
            (hash1 >= hash2 ? hash1 - hash2 : hash2 - hash1) <= tolerance);
}

}  // namespace atom::algorithm

/**
 * @brief User-defined literal for computing hash values of string literals.
 *
 * Example usage: "example"_hash
 *
 * @param str Pointer to the string literal to hash.
 * @param size Size of the string literal (unused).
 * @return constexpr usize Hash value of the string literal.
 */
constexpr auto operator""_hash(const char* str,
                               atom::algorithm::usize size) noexcept
    -> atom::algorithm::usize {
    // The size parameter is not used in this implementation
    static_cast<void>(size);
    return atom::algorithm::hash(str);
}

#endif  // ATOM_ALGORITHM_HASH_HASH_BASE_HPP
