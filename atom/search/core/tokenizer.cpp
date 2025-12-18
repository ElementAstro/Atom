#include "tokenizer.hpp"

namespace atom::search {

namespace {
const HashSet<String> DEFAULT_STOP_WORDS = {
    "a",     "an",   "and",  "are",  "as",    "at",   "be",     "by",   "for",
    "from",  "has",  "he",   "in",   "is",    "it",   "its",    "of",   "on",
    "that",  "the",  "to",   "was",  "were",  "will", "with",   "the",  "this",
    "but",   "they", "have", "had",  "what",  "when", "where",  "who",  "which",
    "why",   "how",  "all",  "each", "every", "both", "few",    "more", "most",
    "other", "some", "such", "no",   "nor",   "not",  "only",   "own",  "same",
    "so",    "than", "too",  "very", "can",   "just", "should", "now"};
}  // namespace

Tokenizer::Tokenizer(TokenizerConfig config)
    : config_(std::move(config)), stopWords_(DEFAULT_STOP_WORDS) {}

std::vector<String> Tokenizer::tokenize(std::string_view content) const {
    std::vector<String> tokens;
    tokens.reserve(content.size() / 5);  // Estimate average word length

    size_t start = 0;
    size_t end = 0;

    while (end <= content.size()) {
        if (end == content.size() ||
            !std::isalnum(static_cast<unsigned char>(content[end]))) {
            if (start < end) {
                auto token = normalizeToken(content.substr(start, end - start));
                if (!token.empty() && token.size() >= config_.minTokenLength &&
                    token.size() <= config_.maxTokenLength &&
                    (!config_.enableStopWords || !isStopWord(token))) {
                    tokens.push_back(std::move(token));
                }
            }
            start = end + 1;
        }
        ++end;
    }

    return tokens;
}

std::vector<String> Tokenizer::tokenizeWithNgrams(std::string_view content,
                                                  size_t n) const {
    auto tokens = tokenize(content);

    if (n <= 1 || tokens.size() < n) {
        return tokens;
    }

    std::vector<String> ngrams;
    ngrams.reserve(tokens.size() + tokens.size() - n + 1);

    // Add unigrams
    for (const auto& token : tokens) {
        ngrams.push_back(token);
    }

    // Add n-grams
    for (size_t i = 0; i + n <= tokens.size(); ++i) {
        std::string ngram;
        for (size_t j = 0; j < n; ++j) {
            if (j > 0)
                ngram += '_';
            ngram += tokens[i + j];
        }
        ngrams.push_back(String(ngram));
    }

    return ngrams;
}

String Tokenizer::normalizeToken(std::string_view token) const {
    std::string result;
    result.reserve(token.size());

    for (char c : token) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            if (config_.caseSensitive) {
                result += c;
            } else {
                result += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
        }
    }

    return String(result);
}

bool Tokenizer::isStopWord(std::string_view word) const noexcept {
    return stopWords_.contains(String(word));
}

void Tokenizer::addStopWord(const std::string& word) {
    stopWords_.insert(String(word));
}

void Tokenizer::removeStopWord(const std::string& word) {
    stopWords_.erase(String(word));
}

void Tokenizer::clearStopWords() noexcept { stopWords_.clear(); }

const HashSet<String>& Tokenizer::getDefaultStopWords() noexcept {
    return DEFAULT_STOP_WORDS;
}

}  // namespace atom::search
