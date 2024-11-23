#ifndef ATOM_UTILS_DIFFLIB_HPP
#define ATOM_UTILS_DIFFLIB_HPP

#include <memory>
#include <string>
#include <vector>

namespace atom::utils {

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
     */
    SequenceMatcher(const std::string& str1, const std::string& str2);

    /**
     * @brief Destructor for SequenceMatcher.
     */
    ~SequenceMatcher();

    /**
     * @brief Set the sequences to be compared.
     * @param str1 The first sequence.
     * @param str2 The second sequence.
     */
    void setSeqs(const std::string& str1, const std::string& str2);

    /**
     * @brief Calculate the similarity ratio between the sequences.
     * @return The similarity ratio as a double.
     */
    [[nodiscard]] auto ratio() const -> double;

    /**
     * @brief Get the matching blocks between the sequences.
     * @return A vector of tuples, each containing the start positions and
     * length of matching blocks.
     */
    [[nodiscard]] auto getMatchingBlocks() const
        -> std::vector<std::tuple<int, int, int>>;

    /**
     * @brief Get a list of opcodes describing how to turn the first sequence
     * into the second.
     * @return A vector of tuples, each containing an opcode and the start and
     * end positions in both sequences.
     */
    [[nodiscard]] auto getOpcodes() const
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
     */
    static auto compare(const std::vector<std::string>& vec1,
                        const std::vector<std::string>& vec2)
        -> std::vector<std::string>;

    /**
     * @brief Generate a unified diff between two sequences.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @param label1 The label for the first sequence.
     * @param label2 The label for the second sequence.
     * @param context The number of context lines to include.
     * @return A vector of strings representing the unified diff.
     */
    static auto unifiedDiff(const std::vector<std::string>& vec1,
                            const std::vector<std::string>& vec2,
                            const std::string& label1 = "a",
                            const std::string& label2 = "b",
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
     * @brief Generate an HTML file showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @return A string containing the HTML representation of the differences.
     */
    static auto makeFile(const std::vector<std::string>& fromlines,
                         const std::vector<std::string>& tolines,
                         const std::string& fromdesc = "",
                         const std::string& todesc = "") -> std::string;

    /**
     * @brief Generate an HTML table showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @return A string containing the HTML table representation of the
     * differences.
     */
    static auto makeTable(const std::vector<std::string>& fromlines,
                          const std::vector<std::string>& tolines,
                          const std::string& fromdesc = "",
                          const std::string& todesc = "") -> std::string;
};

/**
 * @brief Get a list of close matches to a word from a list of possibilities.
 * @param word The word to match.
 * @param possibilities The list of possible matches.
 * @param n The maximum number of close matches to return.
 * @param cutoff The similarity ratio threshold for considering a match.
 * @return A vector of strings containing the close matches.
 */
auto getCloseMatches(const std::string& word,
                     const std::vector<std::string>& possibilities, int n = 3,
                     double cutoff = 0.6) -> std::vector<std::string>;

}  // namespace atom::utils
#endif
