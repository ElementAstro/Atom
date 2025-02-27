/*
 * algorithm.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: A collection of algorithms for C++

**************************************************/

#ifndef ATOM_ALGORITHM_ALGORITHM_HPP
#define ATOM_ALGORITHM_ALGORITHM_HPP

#include <bitset>
#include <cmath>
#include <concepts>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atom::algorithm {

// Concepts for string-like types
template <typename T>
concept StringLike = requires(T t) {
    { t.data() } -> std::convertible_to<const char*>;
    { t.size() } -> std::convertible_to<std::size_t>;
    { t[0] } -> std::convertible_to<char>;
};

/**
 * @brief Implements the Knuth-Morris-Pratt (KMP) string searching algorithm.
 *
 * This class provides methods to search for occurrences of a pattern within a
 * text using the KMP algorithm, which preprocesses the pattern to achieve
 * efficient string searching.
 */
class KMP {
public:
    /**
     * @brief Constructs a KMP object with the given pattern.
     *
     * @param pattern The pattern to search for in text.
     * @throws std::invalid_argument If the pattern is invalid
     */
    explicit KMP(std::string_view pattern);

    /**
     * @brief Searches for occurrences of the pattern in the given text.
     *
     * @param text The text to search within.
     * @return std::vector<int> Vector containing positions where the pattern
     * starts in the text.
     * @throws std::runtime_error If search operation fails
     */
    [[nodiscard]] auto search(std::string_view text) const -> std::vector<int>;

    /**
     * @brief Sets a new pattern for searching.
     *
     * @param pattern The new pattern to search for.
     * @throws std::invalid_argument If the pattern is invalid
     */
    void setPattern(std::string_view pattern);

    /**
     * @brief Asynchronously searches for pattern occurrences in chunks of text.
     *
     * @param text The text to search within
     * @param chunk_size Size of each text chunk to process separately
     * @return std::vector<int> Vector containing positions where the pattern
     * starts
     * @throws std::runtime_error If search operation fails
     */
    [[nodiscard]] auto searchParallel(std::string_view text,
                                      size_t chunk_size = 1024) const
        -> std::vector<int>;

private:
    /**
     * @brief Computes the failure function (partial match table) for the given
     * pattern.
     *
     * @param pattern The pattern for which to compute the failure function.
     * @return std::vector<int> The computed failure function.
     */
    [[nodiscard]] static auto computeFailureFunction(
        std::string_view pattern) noexcept -> std::vector<int>;

    std::string pattern_;       ///< The pattern to search for.
    std::vector<int> failure_;  ///< Failure function for the pattern.

    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe operations
};

/**
 * @brief The BloomFilter class implements a Bloom filter data structure.
 * @tparam N The size of the Bloom filter (number of bits).
 * @tparam ElementType The type of elements stored (must be hashable)
 * @tparam HashFunction Custom hash function type (optional)
 */
template <std::size_t N, typename ElementType = std::string_view,
          typename HashFunction = std::hash<ElementType>>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
class BloomFilter {
public:
    /**
     * @brief Constructs a new BloomFilter object with the specified number of
     * hash functions.
     * @param num_hash_functions The number of hash functions to use.
     * @throws std::invalid_argument If num_hash_functions is zero
     */
    explicit BloomFilter(std::size_t num_hash_functions);

    /**
     * @brief Inserts an element into the Bloom filter.
     * @param element The element to insert.
     */
    void insert(const ElementType& element) noexcept;

    /**
     * @brief Checks if an element might be present in the Bloom filter.
     * @param element The element to check.
     * @return True if the element might be present, false otherwise.
     */
    [[nodiscard]] auto contains(const ElementType& element) const noexcept
        -> bool;

    /**
     * @brief Clears the Bloom filter, removing all elements.
     */
    void clear() noexcept;

    /**
     * @brief Estimates the current false positive probability.
     * @return The estimated false positive rate
     */
    [[nodiscard]] auto falsePositiveProbability() const noexcept -> double;

    /**
     * @brief Returns the number of elements added to the filter.
     */
    [[nodiscard]] auto elementCount() const noexcept -> size_t;

private:
    std::bitset<N> m_bits_{}; /**< The bitset representing the Bloom filter. */
    std::size_t m_num_hash_functions_; /**< Number of hash functions used. */
    std::size_t m_count_{0};  /**< Number of elements added to the filter */
    HashFunction m_hasher_{}; /**< Hash function instance */

    /**
     * @brief Computes the hash value of an element using a specific seed.
     * @param element The element to hash.
     * @param seed The seed value for the hash function.
     * @return The hash value of the element.
     */
    [[nodiscard]] auto hash(const ElementType& element,
                            std::size_t seed) const noexcept -> std::size_t;
};

/**
 * @brief Implements the Boyer-Moore string searching algorithm.
 *
 * This class provides methods to search for occurrences of a pattern within a
 * text using the Boyer-Moore algorithm, which preprocesses the pattern to
 * achieve efficient string searching.
 */
class BoyerMoore {
public:
    /**
     * @brief Constructs a BoyerMoore object with the given pattern.
     *
     * @param pattern The pattern to search for in text.
     * @throws std::invalid_argument If the pattern is invalid
     */
    explicit BoyerMoore(std::string_view pattern);

    /**
     * @brief Searches for occurrences of the pattern in the given text.
     *
     * @param text The text to search within.
     * @return std::vector<int> Vector containing positions where the pattern
     * starts in the text.
     * @throws std::runtime_error If search operation fails
     */
    [[nodiscard]] auto search(std::string_view text) const -> std::vector<int>;

    /**
     * @brief Sets a new pattern for searching.
     *
     * @param pattern The new pattern to search for.
     * @throws std::invalid_argument If the pattern is invalid
     */
    void setPattern(std::string_view pattern);

    /**
     * @brief Performs a Boyer-Moore search using SIMD instructions if
     * available.
     *
     * @param text The text to search within
     * @return std::vector<int> Vector of pattern positions
     * @throws std::runtime_error If search operation fails
     */
    [[nodiscard]] auto searchOptimized(std::string_view text) const
        -> std::vector<int>;

private:
    /**
     * @brief Computes the bad character shift table for the current pattern.
     *
     * This table determines how far to shift the pattern relative to the text
     * based on the last occurrence of a mismatched character.
     */
    void computeBadCharacterShift() noexcept;

    /**
     * @brief Computes the good suffix shift table for the current pattern.
     *
     * This table helps determine how far to shift the pattern when a mismatch
     * occurs based on the occurrence of a partial match (suffix).
     */
    void computeGoodSuffixShift() noexcept;

    std::string pattern_;  ///< The pattern to search for.
    std::unordered_map<char, int>
        bad_char_shift_;                  ///< Bad character shift table.
    std::vector<int> good_suffix_shift_;  ///< Good suffix shift table.

    mutable std::mutex mutex_;  ///< Mutex for thread-safe operations
};

// Implementation of BloomFilter template methods
template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
BloomFilter<N, ElementType, HashFunction>::BloomFilter(
    std::size_t num_hash_functions) {
    if (num_hash_functions == 0) {
        throw std::invalid_argument(
            "Number of hash functions must be greater than zero");
    }
    m_num_hash_functions_ = num_hash_functions;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
void BloomFilter<N, ElementType, HashFunction>::insert(
    const ElementType& element) noexcept {
    for (std::size_t i = 0; i < m_num_hash_functions_; ++i) {
        std::size_t hashValue = hash(element, i);
        m_bits_.set(hashValue % N);
    }
    ++m_count_;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
auto BloomFilter<N, ElementType, HashFunction>::contains(
    const ElementType& element) const noexcept -> bool {
    for (std::size_t i = 0; i < m_num_hash_functions_; ++i) {
        std::size_t hashValue = hash(element, i);
        if (!m_bits_.test(hashValue % N)) {
            return false;
        }
    }
    return true;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
void BloomFilter<N, ElementType, HashFunction>::clear() noexcept {
    m_bits_.reset();
    m_count_ = 0;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
auto BloomFilter<N, ElementType, HashFunction>::hash(
    const ElementType& element,
    std::size_t seed) const noexcept -> std::size_t {
    // Combine the element hash with the seed using FNV-1a variation
    std::size_t hashValue = 0x811C9DC5 + seed;  // FNV offset basis + seed
    std::size_t elementHash = m_hasher_(element);

    // FNV-1a hash combine
    hashValue ^= elementHash;
    hashValue *= 0x01000193;  // FNV prime

    return hashValue;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
auto BloomFilter<N, ElementType, HashFunction>::falsePositiveProbability()
    const noexcept -> double {
    if (m_count_ == 0)
        return 0.0;

    // Calculate (1 - e^(-k*n/m))^k
    // where k = num_hash_functions, n = element count, m = bit array size
    double exponent =
        -static_cast<double>(m_num_hash_functions_ * m_count_) / N;
    double probability =
        std::pow(1.0 - std::exp(exponent), m_num_hash_functions_);
    return probability;
}

template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
auto BloomFilter<N, ElementType, HashFunction>::elementCount() const noexcept
    -> size_t {
    return m_count_;
}

}  // namespace atom::algorithm

#endif