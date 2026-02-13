/*
 * bloom_filter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: Bloom filter data structure

**************************************************/

#ifndef ATOM_ALGORITHM_CORE_BLOOM_FILTER_HPP
#define ATOM_ALGORITHM_CORE_BLOOM_FILTER_HPP

#include <bitset>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <string_view>

#include "atom/error/exception.hpp"

namespace atom::algorithm {

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

// Implementation of BloomFilter template methods
template <std::size_t N, typename ElementType, typename HashFunction>
    requires(N > 0) && requires(HashFunction h, ElementType e) {
        { h(e) } -> std::convertible_to<std::size_t>;
    }
BloomFilter<N, ElementType, HashFunction>::BloomFilter(
    std::size_t num_hash_functions) {
    if (num_hash_functions == 0) {
        THROW_INVALID_ARGUMENT(
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

#endif  // ATOM_ALGORITHM_CORE_BLOOM_FILTER_HPP
