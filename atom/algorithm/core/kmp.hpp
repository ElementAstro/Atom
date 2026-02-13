/*
 * kmp.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-5

Description: KMP string searching algorithm

**************************************************/

#ifndef ATOM_ALGORITHM_CORE_KMP_HPP
#define ATOM_ALGORITHM_CORE_KMP_HPP

#include <concepts>
#include <cstddef>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "atom/error/exception.hpp"

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

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_CORE_KMP_HPP
