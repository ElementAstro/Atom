#ifndef ATOM_UTILS_DIFFLIB_HPP
#define ATOM_UTILS_DIFFLIB_HPP

#include <concepts>
#include <expected>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/expected.hpp"

namespace atom::utils {

/**
 * @brief Concept for types that can be compared as sequences
 */
template <typename T>
concept Sequence = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
    { std::ranges::size(a) } -> std::convertible_to<std::size_t>;
};

/**
 * @class SequenceMatcher
 * @brief A class for comparing pairs of sequences of any type.
 *
 * This class provides methods to compare sequences and calculate the similarity
 * ratio.
 */
class SequenceMatcher {
public:
    /**
     * @brief Constructs a SequenceMatcher with two sequences.
     * @param str1 The first sequence.
     * @param str2 The second sequence.
     * @throws std::invalid_argument If the sequences are invalid.
     */
    SequenceMatcher(std::string_view str1, std::string_view str2);

    /**
     * @brief Destructor for SequenceMatcher.
     */
    ~SequenceMatcher() noexcept;

    /**
     * @brief Set the sequences to be compared.
     * @param str1 The first sequence.
     * @param str2 The second sequence.
     * @throws std::invalid_argument If the sequences are invalid.
     */
    void setSeqs(std::string_view str1, std::string_view str2);

    /**
     * @brief Calculate the similarity ratio between the sequences.
     * @return The similarity ratio as a double.
     */
    [[nodiscard]] auto ratio() const noexcept -> double;

    /**
     * @brief Get the matching blocks between the sequences.
     * @return A vector of tuples, each containing the start positions and
     * length of matching blocks.
     */
    [[nodiscard]] auto getMatchingBlocks() const noexcept
        -> std::vector<std::tuple<int, int, int>>;

    /**
     * @brief Get a list of opcodes describing how to turn the first sequence
     * into the second.
     * @return A vector of tuples, each containing an opcode and the start and
     * end positions in both sequences.
     */
    [[nodiscard]] auto getOpcodes() const noexcept
        -> std::vector<std::tuple<std::string, int, int, int, int>>;

private:
    class Impl;                    ///< Implementation detail class.
    std::unique_ptr<Impl> pimpl_;  ///< Pointer to the implementation.
};

/**
 * @class Differ
 * @brief A class for comparing sequences and generating differences.
 */
class Differ {
public:
    /**
     * @brief Compare two sequences and return the differences.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @return A vector of strings representing the differences.
     * @throws std::invalid_argument If the input sequences are invalid.
     */
    static auto compare(std::span<const std::string> vec1,
                        std::span<const std::string> vec2)
        -> std::vector<std::string>;

    /**
     * @brief Generate a unified diff between two sequences.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @param label1 The label for the first sequence.
     * @param label2 The label for the second sequence.
     * @param context The number of context lines to include.
     * @return A vector of strings representing the unified diff.
     * @throws std::invalid_argument If the input parameters are invalid.
     */
    static auto unifiedDiff(std::span<const std::string> vec1,
                            std::span<const std::string> vec2,
                            std::string_view label1 = "a",
                            std::string_view label2 = "b",
                            int context = 3) -> std::vector<std::string>;
};

/**
 * @class HtmlDiff
 * @brief A class for generating HTML representations of differences between
 * sequences.
 */
class HtmlDiff {
public:
    /**
     * @brief Result type for HTML diff operations.
     */
    using DiffResult = type::expected<std::string, std::string>;

    /**
     * @brief Generate an HTML file showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @return A string containing the HTML representation of the differences or
     * an error message.
     */
    static auto makeFile(std::span<const std::string> fromlines,
                         std::span<const std::string> tolines,
                         std::string_view fromdesc = "",
                         std::string_view todesc = "") -> DiffResult;

    /**
     * @brief Generate an HTML table showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @return A string containing the HTML table representation of the
     * differences or an error message.
     */
    static auto makeTable(std::span<const std::string> fromlines,
                          std::span<const std::string> tolines,
                          std::string_view fromdesc = "",
                          std::string_view todesc = "") -> DiffResult;
};

/**
 * @brief Get a list of close matches to a word from a list of possibilities.
 * @param word The word to match.
 * @param possibilities The list of possible matches.
 * @param n The maximum number of close matches to return.
 * @param cutoff The similarity ratio threshold for considering a match.
 * @return A vector of strings containing the close matches.
 * @throws std::invalid_argument If n <= 0 or cutoff is outside valid range.
 */
auto getCloseMatches(std::string_view word,
                     std::span<const std::string> possibilities, int n = 3,
                     double cutoff = 0.6) -> std::vector<std::string>;

}  // namespace atom::utils
#endif
