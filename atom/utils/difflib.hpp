#ifndef ATOM_UTILS_DIFFLIB_HPP
#define ATOM_UTILS_DIFFLIB_HPP

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/expected.hpp"

namespace atom::utils {

// Forward declaration for logging callback
using LogCallback = std::function<void(const std::string&, int)>;

// Enum for diff algorithm selection
enum class DiffAlgorithm {
    Default,    ///< Original algorithm
    Myers,      ///< Myers diff algorithm
    Patience,   ///< Patience diff algorithm
    Histogram,  ///< Histogram diff algorithm
};

// Diff statistics structure
struct DiffStats {
    int insertions{0};       ///< Number of insertions
    int deletions{0};        ///< Number of deletions
    int modifications{0};    ///< Number of modifications
    double similarity{0.0};  ///< Overall similarity ratio

    // Duration of the diff operation
    std::chrono::microseconds duration{0};

    // Returns a formatted string representation of the statistics
    [[nodiscard]] auto toString() const -> std::string;
};

// Performance options for diff operations
struct DiffOptions {
    bool enableCaching{true};  ///< Enable result caching
    bool useParallelProcessing{
        true};                ///< Use parallel algorithms when possible
    bool lazyLoading{false};  ///< Enable lazy loading for large diffs
    int cacheSizeLimit{100};  ///< Maximum number of cached results
    size_t largeFileThreshold{
        1024 * 1024};  ///< Threshold for large file optimization (bytes)
    DiffAlgorithm algorithm{DiffAlgorithm::Default};  ///< Diff algorithm to use
    LogCallback logger{nullptr};  ///< Logging callback function
};

// Custom exceptions for better error handling
class DiffException : public std::runtime_error {
public:
    explicit DiffException(const std::string& message)
        : std::runtime_error(message) {}
};

class InvalidInputException : public DiffException {
public:
    explicit InvalidInputException(const std::string& message)
        : DiffException(message) {}
};

class AlgorithmFailureException : public DiffException {
public:
    explicit AlgorithmFailureException(const std::string& message)
        : DiffException(message) {}
};

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
     * @param options Optional performance and algorithm options
     * @throws InvalidInputException If the sequences are invalid.
     */
    SequenceMatcher(std::string_view str1, std::string_view str2,
                    const DiffOptions& options = DiffOptions{});

    /**
     * @brief Destructor for SequenceMatcher.
     */
    ~SequenceMatcher() noexcept;

    /**
     * @brief Set the sequences to be compared.
     * @param str1 The first sequence.
     * @param str2 The second sequence.
     * @throws InvalidInputException If the sequences are invalid.
     */
    void setSeqs(std::string_view str1, std::string_view str2);

    /**
     * @brief Set the algorithm and performance options.
     * @param options The options to use for diff operations.
     */
    void setOptions(const DiffOptions& options);

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

    /**
     * @brief Get performance statistics for the last diff operation.
     * @return Structure containing statistics about the last diff operation.
     */
    [[nodiscard]] auto getStats() const noexcept -> const DiffStats&;

    /**
     * @brief Clear the internal cache to free memory.
     */
    void clearCache() noexcept;

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
     * @brief Default constructor.
     * @param options Optional performance and algorithm options
     */
    explicit Differ(const DiffOptions& options = DiffOptions{});

    /**
     * @brief Destructor.
     */
    ~Differ() noexcept;

    /**
     * @brief Compare two sequences and return the differences.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @return A vector of strings representing the differences.
     * @throws InvalidInputException If the input sequences are invalid.
     */
    auto compare(std::span<const std::string> vec1,
                 std::span<const std::string> vec2) -> std::vector<std::string>;

    /**
     * @brief Generate a unified diff between two sequences.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @param label1 The label for the first sequence.
     * @param label2 The label for the second sequence.
     * @param context The number of context lines to include.
     * @return A vector of strings representing the unified diff.
     * @throws InvalidInputException If the input parameters are invalid.
     */
    auto unifiedDiff(std::span<const std::string> vec1,
                     std::span<const std::string> vec2,
                     std::string_view label1 = "a",
                     std::string_view label2 = "b", int context = 3)
        -> std::vector<std::string>;

    /**
     * @brief Set the algorithm and performance options.
     * @param options The options to use for diff operations.
     */
    void setOptions(const DiffOptions& options);

    /**
     * @brief Get performance statistics for the last diff operation.
     * @return Structure containing statistics about the last diff operation.
     */
    [[nodiscard]] auto getStats() const noexcept -> const DiffStats&;

    /**
     * @brief Static version of compare method.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @param options Optional performance and algorithm options
     * @return A vector of strings representing the differences.
     * @throws InvalidInputException If the input sequences are invalid.
     */
    static auto compare(std::span<const std::string> vec1,
                        std::span<const std::string> vec2,
                        const DiffOptions& options) -> std::vector<std::string>;

    /**
     * @brief Static version of unifiedDiff method.
     * @param vec1 The first sequence.
     * @param vec2 The second sequence.
     * @param label1 The label for the first sequence.
     * @param label2 The label for the second sequence.
     * @param context The number of context lines to include.
     * @param options Optional performance and algorithm options
     * @return A vector of strings representing the unified diff.
     * @throws InvalidInputException If the input parameters are invalid.
     */
    static auto unifiedDiff(std::span<const std::string> vec1,
                            std::span<const std::string> vec2,
                            std::string_view label1, std::string_view label2,
                            int context, const DiffOptions& options)
        -> std::vector<std::string>;

private:
    class Impl;                    ///< Implementation detail class.
    std::unique_ptr<Impl> pimpl_;  ///< Pointer to the implementation.
};

/**
 * @class HtmlDiff
 * @brief A class for generating HTML representations of differences between
 * sequences.
 */
class HtmlDiff {
public:
    /**
     * @brief Default constructor with options.
     * @param options Optional performance and algorithm options
     */
    explicit HtmlDiff(const DiffOptions& options = DiffOptions{});

    /**
     * @brief Destructor.
     */
    ~HtmlDiff() noexcept;

    /**
     * @brief Result type for HTML diff operations.
     */
    using DiffResult = type::expected<std::string, std::string>;

    /**
     * @brief HTML diff styling options
     */
    struct HtmlDiffOptions {
        std::string addedClass;    ///< CSS class for added content
        std::string removedClass;  ///< CSS class for removed content
        std::string changedClass;  ///< CSS class for changed content
        bool inlineDiff;           ///< Show character-level inline diffs
        bool showStatistics;       ///< Show diff statistics
        bool showLineNumbers;      ///< Show line numbers

        HtmlDiffOptions()
            : addedClass("diff-add"),
              removedClass("diff-remove"),
              changedClass("diff-change"),
              inlineDiff(true),
              showStatistics(true),
              showLineNumbers(true) {}
    };

    /**
     * @brief Generate an HTML file showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @param htmlOptions HTML styling options
     * @return A string containing the HTML representation of the differences or
     * an error message.
     */
    auto makeFile(std::span<const std::string> fromlines,
                  std::span<const std::string> tolines,
                  std::string_view fromdesc = "", std::string_view todesc = "",
                  const HtmlDiffOptions& htmlOptions = HtmlDiffOptions{})
        -> DiffResult;

    /**
     * @brief Generate an HTML table showing the differences between two
     * sequences.
     * @param fromlines The first sequence.
     * @param tolines The second sequence.
     * @param fromdesc Description for the first sequence.
     * @param todesc Description for the second sequence.
     * @param htmlOptions HTML styling options
     * @return A string containing the HTML table representation of the
     * differences or an error message.
     */
    auto makeTable(std::span<const std::string> fromlines,
                   std::span<const std::string> tolines,
                   std::string_view fromdesc = "", std::string_view todesc = "",
                   const HtmlDiffOptions& htmlOptions = HtmlDiffOptions{})
        -> DiffResult;

    /**
     * @brief Set the algorithm and performance options.
     * @param options The options to use for diff operations.
     */
    void setOptions(const DiffOptions& options);

    /**
     * @brief Get performance statistics for the last diff operation.
     * @return Structure containing statistics about the last diff operation.
     */
    [[nodiscard]] auto getStats() const noexcept -> const DiffStats&;

    /**
     * @brief Static version of makeFile method.
     */
    static auto makeFile(std::span<const std::string> fromlines,
                         std::span<const std::string> tolines,
                         std::string_view fromdesc, std::string_view todesc,
                         const DiffOptions& options,
                         const HtmlDiffOptions& htmlOptions = HtmlDiffOptions{})
        -> DiffResult;

    /**
     * @brief Static version of makeTable method.
     */
    static auto makeTable(
        std::span<const std::string> fromlines,
        std::span<const std::string> tolines, std::string_view fromdesc,
        std::string_view todesc, const DiffOptions& options,
        const HtmlDiffOptions& htmlOptions = HtmlDiffOptions{}) -> DiffResult;

private:
    class Impl;                    ///< Implementation detail class.
    std::unique_ptr<Impl> pimpl_;  ///< Pointer to the implementation.
};

/**
 * @brief Get a list of close matches to a word from a list of possibilities.
 * @param word The word to match.
 * @param possibilities The list of possible matches.
 * @param n The maximum number of close matches to return.
 * @param cutoff The similarity ratio threshold for considering a match.
 * @param options Optional performance and algorithm options
 * @return A vector of strings containing the close matches.
 * @throws InvalidInputException If n <= 0 or cutoff is outside valid range.
 */
auto getCloseMatches(std::string_view word,
                     std::span<const std::string> possibilities, int n = 3,
                     double cutoff = 0.6,
                     const DiffOptions& options = DiffOptions{})
    -> std::vector<std::string>;

/**
 * @brief A utility class to detect changes between lines of text with
 * character-level precision
 */
class InlineDiff {
public:
    /**
     * @brief Constructs an InlineDiff with options.
     * @param options Optional performance and algorithm options
     */
    explicit InlineDiff(const DiffOptions& options = DiffOptions{});

    /**
     * @brief Destructor.
     */
    ~InlineDiff() noexcept;

    /**
     * @brief Compare two strings at character level
     * @param str1 First string
     * @param str2 Second string
     * @return A list of tuples representing operations (equal, insert, delete)
     * with the content
     */
    auto compareChars(std::string_view str1, std::string_view str2)
        -> std::vector<std::tuple<std::string, std::string>>;

    /**
     * @brief Generate HTML representation of inline diff
     * @param str1 First string
     * @param str2 Second string
     * @param options HTML styling options
     * @return A pair of HTML-formatted strings showing character-level
     * differences
     */
    auto toHtml(
        std::string_view str1, std::string_view str2,
        const HtmlDiff::HtmlDiffOptions& options = HtmlDiff::HtmlDiffOptions{})
        -> std::pair<std::string, std::string>;

    /**
     * @brief Set the algorithm and performance options.
     * @param options The options to use for diff operations.
     */
    void setOptions(const DiffOptions& options);

private:
    class Impl;                    ///< Implementation detail class.
    std::unique_ptr<Impl> pimpl_;  ///< Pointer to the implementation.
};

/**
 * @brief Configuration class for difflib logging and telemetry
 */
class DiffLibConfig {
public:
    /**
     * @brief Set global logging callback
     * @param callback Function to call for logging
     */
    static void setLogCallback(const LogCallback& callback);

    /**
     * @brief Enable or disable telemetry data collection
     * @param enabled Whether telemetry is enabled
     */
    static void setTelemetryEnabled(bool enabled);

    /**
     * @brief Set global default options
     * @param options The default options to use
     */
    static void setDefaultOptions(const DiffOptions& options);

    /**
     * @brief Get global default options
     * @return The current default options
     */
    static const DiffOptions& getDefaultOptions();

    /**
     * @brief Clear all internal caches
     */
    static void clearCaches();

private:
    static DiffOptions default_options_;
    static LogCallback log_callback_;
    static bool telemetry_enabled_;
};

}  // namespace atom::utils
#endif
