/*
 * hash_compute.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-28

Description: Hash computation utilities for various standard library types
             including vectors, tuples, arrays, pairs, optionals, variants,
             and std::any. Includes thread-safe caching and parallel
             processing support.

**************************************************/

#ifndef ATOM_ALGORITHM_HASH_HASH_COMPUTE_HPP
#define ATOM_ALGORITHM_HASH_HASH_COMPUTE_HPP

#include <any>
#include <array>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

#include "hash_base.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/functional/hash.hpp>
#endif

namespace atom::algorithm {

// Thread-safe hash cache
template <typename T>
class HashCache {
private:
    std::shared_mutex mutex_;
    std::unordered_map<T, usize> cache_;

public:
    std::optional<usize> get(const T& key) {
        std::shared_lock lock(mutex_);
        if (auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void set(const T& key, usize hash) {
        std::unique_lock lock(mutex_);
        cache_[key] = hash;
    }

    void clear() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }
};

/**
 * @brief Computes hash using selected algorithm
 *
 * @tparam T Type of value to hash
 * @param value The value to hash
 * @param algorithm Hash algorithm to use
 * @return usize Computed hash value
 */
template <Hashable T>
inline auto computeHash(const T& value,
                        HashAlgorithm algorithm = HashAlgorithm::STD) noexcept
    -> usize {
    // Only use cache for default STD algorithm to avoid returning wrong cached
    // results
    static thread_local HashCache<T> cache;

    if (algorithm == HashAlgorithm::STD) {
        if (auto cached = cache.get(value); cached) {
            return *cached;
        }
    }

    usize result = 0;
    switch (algorithm) {
        case HashAlgorithm::STD:
            result = std::hash<T>{}(value);
            break;
        case HashAlgorithm::FNV1A:
            // For string types, hash the actual content
            if constexpr (std::is_same_v<T, std::string> ||
                          std::is_same_v<T, std::string_view>) {
                result = hash(value.data(), value.size(), 2166136261u);
            } else {
                result = hash(reinterpret_cast<const char*>(&value), sizeof(T),
                              2166136261u);
            }
            break;
        // Other algorithms would be implemented here
        default:
            result = std::hash<T>{}(value);
            break;
    }

    // Only cache STD algorithm results
    if (algorithm == HashAlgorithm::STD) {
        cache.set(value, result);
    }
    return result;
}

/**
 * @brief Computes the hash value for a vector of Hashable values.
 *
 * @tparam T Type of the elements in the vector, must satisfy Hashable concept.
 * @param values The vector of values to hash.
 * @param parallel Use parallel processing for large vectors
 * @return usize Hash value of the vector of values.
 */
template <Hashable T>
inline auto computeHash(const std::vector<T>& values,
                        bool parallel = false) noexcept -> usize {
    if (values.empty()) {
        return 0;
    }

    if (!parallel || values.size() < 1000) {
        usize result = 0;
        for (const auto& value : values) {
            hashCombine(result, computeHash(value));
        }
        return result;
    }

    // Parallel implementation for large vectors
    usize hw_threads = std::thread::hardware_concurrency();
    if (hw_threads == 0) {
        hw_threads = 4;  // Default fallback
    }
    // Limit to reasonable number of threads (max 8 or hardware threads)
    const usize num_threads = std::min(hw_threads, static_cast<usize>(8));

    std::vector<usize> partial_results(num_threads, 0);
    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    const usize chunk_size = values.size() / num_threads;
    for (usize i = 0; i < num_threads; ++i) {
        const usize start_idx = i * chunk_size;
        const usize end_idx =
            (i == num_threads - 1) ? values.size() : (i + 1) * chunk_size;
        threads.emplace_back(
            [&values, &partial_results, i, start_idx, end_idx] {
                usize local_result = 0;
                for (usize idx = start_idx; idx < end_idx; ++idx) {
                    hashCombine(local_result, computeHash(values[idx]));
                }
                partial_results[i] = local_result;
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    usize final_result = 0;
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
 * @return usize Hash value of the tuple of values.
 */
template <Hashable... Ts>
inline auto computeHash(const std::tuple<Ts...>& tuple) noexcept -> usize {
    usize result = 0;
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
 * @return usize Hash value of the array of values.
 */
template <Hashable T, usize N>
inline auto computeHash(const std::array<T, N>& array) noexcept -> usize {
    usize result = 0;
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
 * @return usize Hash value of the pair of values.
 */
template <Hashable T1, Hashable T2>
inline auto computeHash(const std::pair<T1, T2>& pair) noexcept -> usize {
    usize seed = computeHash(pair.first);
    hashCombine(seed, computeHash(pair.second));
    return seed;
}

/**
 * @brief Computes the hash value for a std::optional of a Hashable value.
 *
 * @tparam T Type of the value inside the optional, must satisfy Hashable
 * concept.
 * @param opt The optional value to hash.
 * @return usize Hash value of the optional value.
 */
template <Hashable T>
inline auto computeHash(const std::optional<T>& opt) noexcept -> usize {
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
 * @return usize Hash value of the variant value.
 */
template <Hashable... Ts>
inline auto computeHash(const std::variant<Ts...>& var) noexcept -> usize {
#ifdef ATOM_USE_BOOST
    usize result = 0;
    boost::apply_visitor(
        [&result](const auto& value) {
            hashCombine(result, computeHash(value));
        },
        var);
    return result;
#else
    usize result = 0;
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
 * @return usize Hash value of the std::any value.
 */
inline auto computeHash(const std::any& value) noexcept -> usize {
    static HashCache<std::type_index> type_cache;

    if (!value.has_value()) {
        return 0;
    }

    const std::type_info& type = value.type();
    if (auto cached = type_cache.get(std::type_index(type)); cached) {
        return *cached;
    }

    usize result = type.hash_code();
    type_cache.set(std::type_index(type), result);
    return result;
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_HASH_HASH_COMPUTE_HPP
