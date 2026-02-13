/*
 * boyer_moore.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: Boyer-Moore string searching algorithm

**************************************************/

#ifndef ATOM_ALGORITHM_CORE_BOYER_MOORE_HPP
#define ATOM_ALGORITHM_CORE_BOYER_MOORE_HPP

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::algorithm {

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

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CORE_BOYER_MOORE_HPP
