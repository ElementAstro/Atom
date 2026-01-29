#ifndef ATOM_SEARCH_CORE_TOKENIZER_HPP
#define ATOM_SEARCH_CORE_TOKENIZER_HPP

#include <cctype>
#include "types.hpp"

namespace atom::search {

/**
 * @brief Configuration for tokenizer behavior.
 */
struct TokenizerConfig {
    bool enableStopWords = true;
    bool enableStemming = false;
    bool caseSensitive = false;
    size_t minTokenLength = 1;
    size_t maxTokenLength = 100;
    size_t maxNgramSize = 1;
};

/**
 * @brief Text tokenizer for search indexing and query processing.
 * @details Provides tokenization, normalization, stop word filtering,
 *          and n-gram generation for text processing.
 */
class Tokenizer {
public:
    explicit Tokenizer(TokenizerConfig config = {});

    /**
     * @brief Tokenizes content into individual terms.
     * @param content The text to tokenize
     * @return Vector of normalized tokens
     */
    [[nodiscard]] std::vector<String> tokenize(std::string_view content) const;

    /**
     * @brief Tokenizes content with n-gram support.
     * @param content The text to tokenize
     * @param n N-gram size (1 = unigrams, 2 = bigrams, etc.)
     * @return Vector of tokens including n-grams
     */
    [[nodiscard]] std::vector<String> tokenizeWithNgrams(
        std::string_view content, size_t n) const;

    /**
     * @brief Normalizes a single token.
     * @param token The token to normalize
     * @return Normalized token
     */
    [[nodiscard]] String normalizeToken(std::string_view token) const;

    /**
     * @brief Checks if a word is a stop word.
     * @param word The word to check
     * @return true if the word is a stop word
     */
    [[nodiscard]] bool isStopWord(std::string_view word) const noexcept;

    /**
     * @brief Adds a custom stop word.
     * @param word The word to add
     */
    void addStopWord(const std::string& word);

    /**
     * @brief Removes a stop word.
     * @param word The word to remove
     */
    void removeStopWord(const std::string& word);

    /**
     * @brief Clears all stop words.
     */
    void clearStopWords() noexcept;

    /**
     * @brief Gets the current configuration.
     */
    [[nodiscard]] const TokenizerConfig& getConfig() const noexcept {
        return config_;
    }

    /**
     * @brief Updates the configuration.
     */
    void setConfig(TokenizerConfig config) noexcept {
        config_ = std::move(config);
    }

    /**
     * @brief Gets the default English stop words.
     */
    [[nodiscard]] static const HashSet<String>& getDefaultStopWords() noexcept;

private:
    TokenizerConfig config_;
    HashSet<String> stopWords_;
};

}  // namespace atom::search

#endif  // ATOM_SEARCH_CORE_TOKENIZER_HPP
