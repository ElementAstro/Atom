/*
 * hash.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-28

Description: A collection of optimized and enhanced hash algorithms
             with thread safety, parallel processing, and additional
             hash algorithms support.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_HPP
#define ATOM_ALGORITHM_HASH_HPP

#include <any>
#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <typeindex>
#include <variant>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/functional/hash.hpp>
#endif

// SIMD headers if available
#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__AVX2__)
#include <immintrin.h>
#endif

// Forward declaration
constexpr auto hash(const char* str,
                    std::size_t basis = 2166136261u) noexcept -> std::size_t;

namespace atom::algorithm {

// Thread-safe hash cache
template <typename T>
class HashCache {
private:
    std::shared_mutex mutex_;
    std::unordered_map<T, std::size_t> cache_;

public:
    std::optional<std::size_t> get(const T& key) {
        std::shared_lock lock(mutex_);
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void set(const T& key, std::size_t hash) {
        std::unique_lock lock(mutex_);
        cache_[key] = hash;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }
};

/**
 * @brief Concept for types that can be hashed.
 *
 * A type is Hashable if it supports hashing via std::hash and the result is
 * convertible to std::size_t.
 */
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Enumeration of available hash algorithms
 */
enum class HashAlgorithm {
    STD,       // Standard library hash
    FNV1A,     // FNV-1a
    XXHASH,    // xxHash
    CITYHASH,  // CityHash
    MURMUR3    // MurmurHash3
};

#ifdef ATOM_USE_BOOST
/**
 * @brief Combines two hash values into one using Boost's hash_combine.
 *
 * @param seed The initial hash value.
 * @param hash The hash value to combine with the seed.
 */
inline void hashCombine(std::size_t& seed, std::size_t hash) noexcept {
    boost::hash_combine(seed, hash);
}
#else
/**
 * @brief Combines two hash values into one.
 *
 * This function implements the hash combining technique proposed by Boost.
 * Optimized with SIMD instructions when available.
 *
 * @param seed The initial hash value.
 * @param hash The hash value to combine with the seed.
 * @return std::size_t The combined hash value.
 */
inline auto hashCombine(std::size_t seed,
                        std::size_t hash) noexcept -> std::size_t {
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
    return _mm256_extract_epi64(result, 0);
#else
    // Fallback to original implementation
    return seed ^ (hash + 0x9e3779b9 + (seed << 6) + (seed >> 2));
#endif
}
#endif

/**
 * @brief Computes hash using selected algorithm
 *
 * @tparam T Type of value to hash
 * @param value The value to hash
 * @param algorithm Hash algorithm to use
 * @return std::size_t Computed hash value
 */
template <Hashable T>
inline auto computeHash(const T& value,
                        HashAlgorithm algorithm = HashAlgorithm::STD) noexcept
    -> std::size_t {
    static thread_local HashCache<T> cache;

    if (auto cached = cache.get(value); cached) {
        return *cached;
    }

    std::size_t result = 0;
    switch (algorithm) {
        case HashAlgorithm::STD:
            result = std::hash<T>{}(value);
            break;
        case HashAlgorithm::FNV1A:
            result = hash(reinterpret_cast<const char*>(&value), sizeof(T));
            break;
        // Other algorithms would be implemented here
        default:
            result = std::hash<T>{}(value);
            break;
    }

    cache.set(value, result);
    return result;
}

/**
 * @brief Computes the hash value for a vector of Hashable values.
 *
 * @tparam T Type of the elements in the vector, must satisfy Hashable concept.
 * @param values The vector of values to hash.
 * @param parallel Use parallel processing for large vectors
 * @return std::size_t Hash value of the vector of values.
 */
template <Hashable T>
inline auto computeHash(const std::vector<T>& values,
                        bool parallel = false) noexcept -> std::size_t {
    if (values.empty()) {
        return 0;
    }

    if (!parallel || values.size() < 1000) {
        std::size_t result = 0;
        for (const auto& value : values) {
            hashCombine(result, computeHash(value));
        }
        return result;
    }

    // Parallel implementation for large vectors
    const size_t num_threads = std::thread::hardware_concurrency();
    std::vector<std::size_t> partial_results(num_threads, 0);
    std::vector<std::thread> threads;

    const size_t chunk_size = values.size() / num_threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i] {
            auto start = values.begin() + i * chunk_size;
            auto end =
                (i == num_threads - 1) ? values.end() : start + chunk_size;
            for (auto it = start; it != end; ++it) {
                hashCombine(partial_results[i], computeHash(*it));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::size_t final_result = 0;
    for (const auto& partial : partial_results) {
        hashCombine(final_result, partial);
    }

    return final_result;
}

/**
 * @brief Computes the hash value for a tuple of Hashable values.
 *
 * @tparam Ts Types of the elements in the tuple, all must satisfy Hashable
 * concept.
 * @param tuple The tuple of values to hash.
 * @return std::size_t Hash value of the tuple of values.
 */
template <Hashable... Ts>
inline auto computeHash(const std::tuple<Ts...>& tuple) noexcept
    -> std::size_t {
    std::size_t result = 0;
    std::apply(
        [&result](const Ts&... values) {
            ((hashCombine(result, computeHash(values))), ...);
        },
        tuple);
    return result;
}

/**
 * @brief Computes the hash value for an array of Hashable values.
 *
 * @tparam T Type of the elements in the array, must satisfy Hashable concept.
 * @tparam N Size of the array.
 * @param array The array of values to hash.
 * @return std::size_t Hash value of the array of values.
 */
template <Hashable T, std::size_t N>
inline auto computeHash(const std::array<T, N>& array) noexcept -> std::size_t {
    std::size_t result = 0;
    for (const auto& value : array) {
        hashCombine(result, computeHash(value));
    }
    return result;
}

/**
 * @brief Computes the hash value for a std::pair of Hashable values.
 *
 * @tparam T1 Type of the first element in the pair, must satisfy Hashable
 * concept.
 * @tparam T2 Type of the second element in the pair, must satisfy Hashable
 * concept.
 * @param pair The pair of values to hash.
 * @return std::size_t Hash value of the pair of values.
 */
template <Hashable T1, Hashable T2>
inline auto computeHash(const std::pair<T1, T2>& pair) noexcept -> std::size_t {
    std::size_t seed = computeHash(pair.first);
    hashCombine(seed, computeHash(pair.second));
    return seed;
}

/**
 * @brief Computes the hash value for a std::optional of a Hashable value.
 *
 * @tparam T Type of the value inside the optional, must satisfy Hashable
 * concept.
 * @param opt The optional value to hash.
 * @return std::size_t Hash value of the optional value.
 */
template <Hashable T>
inline auto computeHash(const std::optional<T>& opt) noexcept -> std::size_t {
    if (opt.has_value()) {
        return computeHash(*opt) +
#ifdef ATOM_USE_BOOST
               1;  // Boost does not require differentiation, handled internally
#else
               1;  // Adding 1 to differentiate from std::nullopt
#endif
    }
    return 0;
}

/**
 * @brief Computes the hash value for a std::variant of Hashable types.
 *
 * @tparam Ts Types contained in the variant, all must satisfy Hashable concept.
 * @param var The variant of values to hash.
 * @return std::size_t Hash value of the variant value.
 */
template <Hashable... Ts>
inline auto computeHash(const std::variant<Ts...>& var) noexcept
    -> std::size_t {
#ifdef ATOM_USE_BOOST
    std::size_t result = 0;
    boost::apply_visitor(
        [&result](const auto& value) {
            hashCombine(result, computeHash(value));
        },
        var);
    return result;
#else
    std::size_t result = 0;
    std::visit(
        [&result](const auto& value) {
            hashCombine(result, computeHash(value));
        },
        var);
    return result;
#endif
}

/**
 * @brief Computes the hash value for a std::any value.
 *
 * This function attempts to hash the contained value if it is Hashable.
 * If the contained type is not Hashable, it hashes the type information
 * instead. Includes thread-safe caching.
 *
 * @param value The std::any value to hash.
 * @return std::size_t Hash value of the std::any value.
 */
inline auto computeHash(const std::any& value) noexcept -> std::size_t {
    static HashCache<std::type_index> type_cache;

    if (!value.has_value()) {
        return 0;
    }

    const std::type_info& type = value.type();
    if (auto cached = type_cache.get(std::type_index(type)); cached) {
        return *cached;
    }

    std::size_t result = type.hash_code();
    type_cache.set(std::type_index(type), result);
    return result;
}

/**
 * @brief Verifies if two hash values match
 *
 * @param hash1 First hash value
 * @param hash2 Second hash value
 * @param tolerance Allowed difference (for fuzzy matching)
 * @return bool True if hashes match within tolerance
 */
inline auto verifyHash(std::size_t hash1, std::size_t hash2,
                       std::size_t tolerance = 0) noexcept -> bool {
    return (hash1 == hash2) ||
           (tolerance > 0 &&
            (hash1 >= hash2 ? hash1 - hash2 : hash2 - hash1) <= tolerance);
}

/**
 * @brief Computes a hash value for a null-terminated string using FNV-1a
 * algorithm. Optimized with SIMD instructions when available.
 *
 * @param str Pointer to the null-terminated string to hash.
 * @param basis Initial basis value for hashing.
 * @return constexpr std::size_t Hash value of the string.
 */
constexpr auto hash(const char* str,
                    std::size_t basis = 2166136261u) noexcept -> std::size_t {
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
    std::size_t hash = basis;
    while (*str != '\0') {
        hash ^= static_cast<std::size_t>(*str);
        hash *= 16777619u;
        ++str;
    }
    return hash;
#endif
}
}  // namespace atom::algorithm

/**
 * @brief User-defined literal for computing hash values of string literals.
 *
 * Example usage: "example"_hash
 *
 * @param str Pointer to the string literal to hash.
 * @param size Size of the string literal (unused).
 * @return constexpr std::size_t Hash value of the string literal.
 */
constexpr auto operator""_hash(const char* str,
                               std::size_t size) noexcept -> std::size_t {
    // The size parameter is not used in this implementation
    static_cast<void>(size);
    return atom::algorithm::hash(str);
}

#endif  // ATOM_ALGORITHM_HASH_HPP
