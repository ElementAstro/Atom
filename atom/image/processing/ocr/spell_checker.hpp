/**
 * @file spell_checker.hpp
 * @brief Spell checking and correction for OCR results
 */

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace atom::image::ocr {

/**
 * @class SpellChecker
 * @brief Spell checking and correction for OCR results
 *
 * Implements dictionary-based spell checking with Levenshtein distance
 * for suggestion generation.
 */
class SpellChecker {
public:
    /**
     * @brief Construct a new SpellChecker
     * @param dictionaryPath Path to dictionary file (one word per line)
     */
    explicit SpellChecker(const std::string& dictionaryPath = "");

    ~SpellChecker() = default;

    /**
     * @brief Load dictionary from file
     * @param filePath Path to dictionary file
     * @return True if loading succeeded
     */
    bool loadDictionary(const std::string& filePath);

    /**
     * @brief Add word to dictionary
     * @param word Word to add
     */
    void addWord(const std::string& word);

    /**
     * @brief Check if word is in dictionary
     * @param word Word to check
     * @return True if word is in dictionary
     */
    bool isCorrect(const std::string& word) const;

    /**
     * @brief Get spelling suggestion for word
     * @param word Word to get suggestion for
     * @return Suggested correction
     */
    std::string suggest(const std::string& word) const;

    /**
     * @brief Get multiple suggestions for word
     * @param word Word to get suggestions for
     * @param maxSuggestions Maximum number of suggestions
     * @return Vector of suggestions sorted by likelihood
     */
    std::vector<std::string> getSuggestions(const std::string& word,
                                            size_t maxSuggestions = 5) const;

    /**
     * @brief Correct spelling in text
     * @param text Input text
     * @return Corrected text
     */
    std::string correctText(const std::string& text) const;

    /**
     * @brief Get dictionary size
     * @return Number of words in dictionary
     */
    size_t getDictionarySize() const;

    /**
     * @brief Clear dictionary
     */
    void clear();

private:
    std::unordered_map<std::string, int> m_dictionary;  ///< Word dictionary

    /**
     * @brief Calculate Levenshtein edit distance
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance between strings
     */
    static int levenshteinDistance(const std::string& s1,
                                   const std::string& s2);

    /**
     * @brief Normalize word for comparison
     * @param word Input word
     * @return Lowercase version of word
     */
    static std::string normalizeWord(const std::string& word);
};

}  // namespace atom::image::ocr
