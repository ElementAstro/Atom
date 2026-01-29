#ifndef ATOM_SEARCH_CORE_SCORING_HPP
#define ATOM_SEARCH_CORE_SCORING_HPP

#include "document.hpp"
#include "types.hpp"

namespace atom::search {

/**
 * @brief Configuration for BM25 scoring algorithm.
 */
struct BM25Config {
    double k1 = 1.2;     // Term frequency saturation parameter
    double b = 0.75;     // Length normalization parameter
    double delta = 0.0;  // BM25+ delta parameter
};

/**
 * @brief Index statistics needed for scoring calculations.
 */
struct IndexStats {
    size_t documentCount = 0;
    size_t uniqueTerms = 0;
    size_t uniqueTags = 0;
    size_t totalTokens = 0;
    double avgDocumentLength = 0.0;
};

/**
 * @brief Scoring algorithms for document relevance ranking.
 * @details Provides TF-IDF and BM25 scoring implementations.
 */
class Scorer {
public:
    /**
     * @brief Constructs a Scorer with index statistics.
     * @param stats Index statistics for IDF calculations
     * @param bm25Config BM25 configuration
     */
    explicit Scorer(const IndexStats& stats, BM25Config bm25Config = {});

    /**
     * @brief Updates index statistics.
     */
    void updateStats(const IndexStats& stats) noexcept;

    /**
     * @brief Calculates TF-IDF score for a term in a document.
     * @param doc The document
     * @param term The search term
     * @param termFrequency Document frequency of the term
     * @return TF-IDF score
     */
    [[nodiscard]] double tfIdf(const Document& doc, std::string_view term,
                               size_t termFrequency) const noexcept;

    /**
     * @brief Calculates BM25 score for a term in a document.
     * @param doc The document
     * @param term The search term
     * @param termFrequency Document frequency of the term
     * @return BM25 score
     */
    [[nodiscard]] double bm25(const Document& doc, std::string_view term,
                              size_t termFrequency) const noexcept;

    /**
     * @brief Calculates click boost factor.
     * @param clickCount Number of clicks
     * @return Boost multiplier
     */
    [[nodiscard]] static double clickBoost(int clickCount) noexcept;

    /**
     * @brief Calculates Levenshtein distance between two strings.
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance
     */
    [[nodiscard]] static int levenshteinDistance(std::string_view s1,
                                                 std::string_view s2) noexcept;

    /**
     * @brief Gets the current BM25 configuration.
     */
    [[nodiscard]] const BM25Config& getBM25Config() const noexcept {
        return bm25Config_;
    }

    /**
     * @brief Sets the BM25 configuration.
     */
    void setBM25Config(BM25Config config) noexcept { bm25Config_ = config; }

    /**
     * @brief Gets the current index statistics.
     */
    [[nodiscard]] const IndexStats& getStats() const noexcept { return stats_; }

private:
    /**
     * @brief Counts term occurrences in content.
     */
    [[nodiscard]] static size_t countTermOccurrences(
        std::string_view content, std::string_view term) noexcept;

    /**
     * @brief Estimates document length (word count).
     */
    [[nodiscard]] static size_t estimateDocumentLength(
        std::string_view content) noexcept;

    IndexStats stats_;
    BM25Config bm25Config_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_SCORING_HPP
