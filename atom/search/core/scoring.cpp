#include "scoring.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace atom::search {

Scorer::Scorer(const IndexStats& stats, BM25Config bm25Config)
    : stats_(stats), bm25Config_(bm25Config) {}

void Scorer::updateStats(const IndexStats& stats) noexcept { stats_ = stats; }

double Scorer::tfIdf(const Document& doc, std::string_view term,
                     size_t termFrequency) const noexcept {
    std::string contentStd(doc.getContent());
    std::string termStd(term);

    std::transform(contentStd.begin(), contentStd.end(), contentStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(termStd.begin(), termStd.end(), termStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    size_t count = countTermOccurrences(contentStd, termStd);
    if (count == 0)
        return 0.0;

    // Log-normalized term frequency
    double tf = 1.0 + std::log(static_cast<double>(count));

    // Inverse document frequency
    double df = static_cast<double>(termFrequency);
    if (df < 1.0)
        df = 1.0;

    double idf = (stats_.documentCount > 0 && df > 0)
                     ? std::log(static_cast<double>(stats_.documentCount) / df)
                     : 0.0;

    // Apply click boost
    double boost = clickBoost(doc.getClickCount());

    return tf * idf * boost;
}

double Scorer::bm25(const Document& doc, std::string_view term,
                    size_t termFrequency) const noexcept {
    std::string contentStd(doc.getContent());
    std::string termStd(term);

    std::transform(contentStd.begin(), contentStd.end(), contentStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::transform(termStd.begin(), termStd.end(), termStd.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    size_t termCount = countTermOccurrences(contentStd, termStd);
    if (termCount == 0)
        return 0.0;

    size_t docLen = estimateDocumentLength(contentStd);

    // IDF component (BM25 variant)
    double df = static_cast<double>(termFrequency);
    if (df < 1.0)
        df = 1.0;

    double N = static_cast<double>(stats_.documentCount);
    double idf =
        (N > 0 && df > 0) ? std::log((N - df + 0.5) / (df + 0.5) + 1.0) : 0.0;

    // BM25 term frequency component
    double avgDl =
        stats_.avgDocumentLength > 0 ? stats_.avgDocumentLength : 1.0;
    double k1 = bm25Config_.k1;
    double b = bm25Config_.b;

    double tfNorm = (static_cast<double>(termCount) * (k1 + 1.0)) /
                    (static_cast<double>(termCount) +
                     k1 * (1.0 - b + b * static_cast<double>(docLen) / avgDl));

    // BM25+ delta
    tfNorm += bm25Config_.delta;

    // Apply click boost
    double boost = clickBoost(doc.getClickCount());

    return idf * tfNorm * boost;
}

double Scorer::clickBoost(int clickCount) noexcept {
    return 1.0 + std::log1p(static_cast<double>(clickCount) * 0.1);
}

int Scorer::levenshteinDistance(std::string_view s1,
                                std::string_view s2) noexcept {
    const size_t m = s1.length();
    const size_t n = s2.length();

    if (m == 0)
        return static_cast<int>(n);
    if (n == 0)
        return static_cast<int>(m);

    std::vector<int> prevRow(n + 1);
    std::vector<int> currRow(n + 1);

    for (size_t j = 0; j <= n; ++j) {
        prevRow[j] = static_cast<int>(j);
    }

    for (size_t i = 0; i < m; ++i) {
        currRow[0] = static_cast<int>(i + 1);
        for (size_t j = 0; j < n; ++j) {
            int cost = (s1[i] == s2[j]) ? 0 : 1;
            currRow[j + 1] = std::min(
                {prevRow[j + 1] + 1, currRow[j] + 1, prevRow[j] + cost});
        }
        prevRow.swap(currRow);
    }

    return prevRow[n];
}

size_t Scorer::countTermOccurrences(std::string_view content,
                                    std::string_view term) noexcept {
    if (term.empty())
        return 0;

    size_t count = 0;
    size_t pos = 0;

    while ((pos = content.find(term, pos)) != std::string_view::npos) {
        ++count;
        pos += term.length();
    }

    return count;
}

size_t Scorer::estimateDocumentLength(std::string_view content) noexcept {
    size_t wordCount = 0;
    bool inWord = false;

    for (char c : content) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            if (!inWord) {
                ++wordCount;
                inWord = true;
            }
        } else {
            inWord = false;
        }
    }

    return wordCount;
}

}  // namespace atom::search
