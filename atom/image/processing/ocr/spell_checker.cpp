#include "spell_checker.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <vector>

namespace atom::image::ocr {

SpellChecker::SpellChecker(const std::string& dictionaryPath) {
    if (!dictionaryPath.empty()) {
        loadDictionary(dictionaryPath);
    }
}

bool SpellChecker::loadDictionary(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string word;
    while (std::getline(file, word)) {
        // Remove trailing newline/carriage return
        while (!word.empty() && (word.back() == '\n' || word.back() == '\r')) {
            word.pop_back();
        }
        if (!word.empty()) {
            m_dictionary[normalizeWord(word)] = 1;
        }
    }
    return true;
}

void SpellChecker::addWord(const std::string& word) {
    m_dictionary[normalizeWord(word)]++;
}

bool SpellChecker::isCorrect(const std::string& word) const {
    return m_dictionary.count(normalizeWord(word)) > 0;
}

int SpellChecker::levenshteinDistance(const std::string& s1,
                                      const std::string& s2) {
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i)
        d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j)
        d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            d[i][j] =
                std::min({d[i - 1][j] + 1, d[i][j - 1] + 1,
                          d[i - 1][j - 1] + (s1[i - 1] == s2[j - 1] ? 0 : 1)});
        }
    }

    return d[len1][len2];
}

std::string SpellChecker::normalizeWord(const std::string& word) {
    std::string result;
    result.reserve(word.size());
    for (char c : word) {
        result +=
            static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

std::string SpellChecker::suggest(const std::string& word) const {
    std::string normalizedWord = normalizeWord(word);

    if (isCorrect(normalizedWord)) {
        return word;
    }

    std::string bestMatch = word;
    int minDistance = std::numeric_limits<int>::max();

    for (const auto& [dictWord, _] : m_dictionary) {
        // Only consider words with similar length
        int lenDiff = static_cast<int>(dictWord.size()) -
                      static_cast<int>(normalizedWord.size());
        if (std::abs(lenDiff) > 2) {
            continue;
        }

        int distance = levenshteinDistance(normalizedWord, dictWord);
        if (distance < minDistance) {
            minDistance = distance;
            bestMatch = dictWord;

            if (distance == 1) {
                break;
            }
        }
    }

    return (minDistance <= 2) ? bestMatch : word;
}

std::vector<std::string> SpellChecker::getSuggestions(
    const std::string& word, size_t maxSuggestions) const {
    std::string normalizedWord = normalizeWord(word);
    std::vector<std::pair<int, std::string>> candidates;

    for (const auto& [dictWord, _] : m_dictionary) {
        int lenDiff = static_cast<int>(dictWord.size()) -
                      static_cast<int>(normalizedWord.size());
        if (std::abs(lenDiff) > 3) {
            continue;
        }

        int distance = levenshteinDistance(normalizedWord, dictWord);
        if (distance <= 3) {
            candidates.emplace_back(distance, dictWord);
        }
    }

    // Sort by distance
    std::sort(candidates.begin(), candidates.end());

    std::vector<std::string> result;
    for (size_t i = 0; i < std::min(maxSuggestions, candidates.size()); ++i) {
        result.push_back(candidates[i].second);
    }

    return result;
}

std::string SpellChecker::correctText(const std::string& text) const {
    std::istringstream iss(text);
    std::ostringstream oss;
    std::string word;
    bool first = true;

    while (iss >> word) {
        if (!first) {
            oss << " ";
        }
        first = false;

        // Extract punctuation
        std::string prefix, suffix;
        while (!word.empty() &&
               std::ispunct(static_cast<unsigned char>(word.front()))) {
            prefix += word.front();
            word.erase(0, 1);
        }
        while (!word.empty() &&
               std::ispunct(static_cast<unsigned char>(word.back()))) {
            suffix = word.back() + suffix;
            word.pop_back();
        }

        if (!word.empty()) {
            std::string corrected = suggest(word);
            oss << prefix << corrected << suffix;
        } else {
            oss << prefix << suffix;
        }
    }

    return oss.str();
}

size_t SpellChecker::getDictionarySize() const { return m_dictionary.size(); }

void SpellChecker::clear() { m_dictionary.clear(); }

}  // namespace atom::image::ocr
