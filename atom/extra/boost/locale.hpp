#ifndef ATOM_EXTRA_BOOST_LOCALE_HPP
#define ATOM_EXTRA_BOOST_LOCALE_HPP

#include <algorithm>
#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/functional/hash.hpp>
#include <boost/locale.hpp>
#include <boost/locale/encoding.hpp>
#include <boost/locale/generator.hpp>
#include <boost/regex.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atom::extra::boost {

// Forward declarations
class LocaleCache;
class PhoneticMatcher;
class UnicodeAnalyzer;

/**
 * @brief Enhanced locale configuration options
 */
struct LocaleConfig {
    std::string name;
    std::string encoding = "UTF-8";
    bool enableCaching = true;
    bool enablePhonetics = false;
    size_t cacheSize = 1024;
    std::chrono::minutes cacheTimeout{30};
    bool threadSafe = true;
};

/**
 * @brief Text analysis result structure
 */
struct TextAnalysis {
    size_t characterCount = 0;
    size_t wordCount = 0;
    size_t sentenceCount = 0;
    size_t paragraphCount = 0;
    std::vector<std::string> languages;
    std::unordered_map<std::string, size_t> wordFrequency;
    double readabilityScore = 0.0;
    std::string dominantLanguage;
};

/**
 * @brief Phonetic matching result
 */
struct PhoneticMatch {
    std::string original;
    std::string phonetic;
    double similarity = 0.0;
    std::string algorithm;
};

/**
 * @brief High-performance wrapper class for Boost.Locale functionalities with
 * advanced features
 *
 * This enhanced class provides utilities for string conversion, Unicode
 * normalization, tokenization, translation, case conversion, collation, date
 * and time formatting, number formatting, currency formatting, regex
 * replacement, phonetic matching, text analysis, and performance optimizations
 * using Boost.Locale.
 */
class LocaleWrapper {
private:
    // Thread-local cache for locale objects and conversion results
    static thread_local std::unordered_map<std::string, std::locale>
        locale_cache_;
    static thread_local std::unordered_map<std::string, std::string>
        conversion_cache_;
    static thread_local std::chrono::steady_clock::time_point
        last_cache_cleanup_;

    // Memory pool for efficient string allocations
    static thread_local std::pmr::unsynchronized_pool_resource memory_pool_;

    // Atomic counters for statistics
    static std::atomic<uint64_t> cache_hits_;
    static std::atomic<uint64_t> cache_misses_;
    static std::atomic<uint64_t> total_operations_;

public:
    /**
     * @brief Constructs a LocaleWrapper object with the specified locale
     * @param localeName The name of the locale to use. If empty, the global
     * locale is used
     */
    explicit LocaleWrapper(std::string_view localeName = "")
        : config_{std::string(localeName)} {
        locale_ = getOrCreateLocale(config_.name);
        ++total_operations_;
    }

    /**
     * @brief Constructs a LocaleWrapper object with advanced configuration
     * @param config The locale configuration
     */
    explicit LocaleWrapper(const LocaleConfig& config) : config_(config) {
        locale_ = getOrCreateLocale(config_.name);
        ++total_operations_;
    }

    /**
     * @brief Copy constructor with cache optimization
     */
    LocaleWrapper(const LocaleWrapper& other)
        : config_(other.config_), locale_(other.locale_) {
        ++total_operations_;
    }

    /**
     * @brief Move constructor
     */
    LocaleWrapper(LocaleWrapper&& other) noexcept
        : config_(std::move(other.config_)), locale_(std::move(other.locale_)) {
        ++total_operations_;
    }

    /**
     * @brief Assignment operators
     */
    LocaleWrapper& operator=(const LocaleWrapper& other) {
        if (this != &other) {
            config_ = other.config_;
            locale_ = other.locale_;
        }
        return *this;
    }

    LocaleWrapper& operator=(LocaleWrapper&& other) noexcept {
        if (this != &other) {
            config_ = std::move(other.config_);
            locale_ = std::move(other.locale_);
        }
        return *this;
    }

    /**
     * @brief Converts a string to UTF-8 encoding with caching
     * @param str The string to convert
     * @param fromCharset The original character set of the string
     * @return The UTF-8 encoded string
     */
    [[nodiscard]] static std::string toUtf8(std::string_view str,
                                            std::string_view fromCharset) {
        ++total_operations_;

        // Create cache key
        std::string cache_key = std::string("utf8_") +
                                std::string(fromCharset) + "_" +
                                std::string(str);

        // Check cache first
        if (auto cached = getCachedConversion(cache_key)) {
            return *cached;
        }

        // Perform conversion
        std::string result = ::boost::locale::conv::to_utf<char>(
            std::string(str), std::string(fromCharset));

        // Cache the result
        cacheConversion(cache_key, result);

        return result;
    }

    /**
     * @brief Batch converts multiple strings to UTF-8 encoding
     * @param strings Span of strings to convert
     * @param fromCharset The original character set
     * @return Vector of UTF-8 encoded strings
     */
    [[nodiscard]] static std::vector<std::string> batchToUtf8(
        std::span<const std::string> strings, std::string_view fromCharset) {
        std::vector<std::string> results;
        results.reserve(strings.size());

        for (const auto& str : strings) {
            results.emplace_back(toUtf8(str, fromCharset));
        }

        return results;
    }

    /**
     * @brief Converts a UTF-8 encoded string to another character set
     * @param str The UTF-8 encoded string to convert
     * @param toCharset The target character set
     * @return The converted string
     */
    [[nodiscard]] static std::string fromUtf8(std::string_view str,
                                              std::string_view toCharset) {
        return ::boost::locale::conv::from_utf<char>(std::string(str),
                                                     std::string(toCharset));
    }

    /**
     * @brief Normalizes a Unicode string
     * @param str The string to normalize
     * @param norm The normalization form to use (default is NFC)
     * @return The normalized string
     */
    [[nodiscard]] static std::string normalize(
        std::string_view str,
        ::boost::locale::norm_type norm = ::boost::locale::norm_default) {
        return ::boost::locale::normalize(std::string(str), norm);
    }

    /**
     * @brief Enhanced tokenization with caching and multiple boundary types
     * @param str The string to tokenize
     * @param localeName The name of the locale to use for tokenization
     * @param boundaryType The type of boundary (word, sentence, line,
     * character)
     * @return A vector of tokens
     */
    [[nodiscard]] static std::vector<std::string> tokenize(
        std::string_view str, std::string_view localeName = "",
        ::boost::locale::boundary::boundary_type boundaryType =
            ::boost::locale::boundary::word) {
        ++total_operations_;

        // Create cache key
        std::string cache_key = std::string("tokenize_") +
                                std::string(localeName) + "_" +
                                std::to_string(static_cast<int>(boundaryType)) +
                                "_" + std::string(str);

        // Check cache first
        if (auto cached = getCachedConversion(cache_key)) {
            // Deserialize cached result (simplified for demo)
            std::vector<std::string> tokens;
            std::istringstream iss(*cached);
            std::string token;
            while (std::getline(iss, token, '\n')) {
                if (!token.empty()) {
                    tokens.push_back(token);
                }
            }
            return tokens;
        }

        std::locale loc = getOrCreateLocale(std::string(localeName));
        std::string s(str);
        ::boost::locale::boundary::ssegment_index map(boundaryType, s.begin(),
                                                      s.end(), loc);

        std::vector<std::string> tokens;
        tokens.reserve(64);  // Increased reserve for better performance

        for (const auto& token : map) {
            if (!token.str().empty() &&
                !std::all_of(token.str().begin(), token.str().end(),
                             ::isspace)) {
                tokens.emplace_back(token.str());
            }
        }

        // Cache the result (serialize tokens)
        std::ostringstream oss;
        for (const auto& token : tokens) {
            oss << token << '\n';
        }
        cacheConversion(cache_key, oss.str());

        return tokens;
    }

    /**
     * @brief Advanced text analysis with comprehensive metrics
     * @param text The text to analyze
     * @param localeName The locale for analysis
     * @return TextAnalysis structure with detailed metrics
     */
    [[nodiscard]] static TextAnalysis analyzeText(
        std::string_view text, std::string_view localeName = "") {
        ++total_operations_;

        TextAnalysis analysis;
        std::string textStr(text);
        std::locale loc = getOrCreateLocale(std::string(localeName));

        // Character count (Unicode-aware)
        analysis.characterCount =
            ::boost::locale::conv::utf_to_utf<char>(textStr).length();

        // Word tokenization and frequency analysis
        auto words =
            tokenize(text, localeName, ::boost::locale::boundary::word);
        analysis.wordCount = words.size();

        for (const auto& word : words) {
            std::string lowerWord = ::boost::locale::to_lower(word, loc);
            analysis.wordFrequency[lowerWord]++;
        }

        // Sentence count
        auto sentences =
            tokenize(text, localeName, ::boost::locale::boundary::sentence);
        analysis.sentenceCount = sentences.size();

        // Paragraph count (simple heuristic)
        analysis.paragraphCount =
            std::count(textStr.begin(), textStr.end(), '\n') + 1;

        // Simple readability score (Flesch-like)
        if (analysis.sentenceCount > 0 && analysis.wordCount > 0) {
            double avgWordsPerSentence =
                static_cast<double>(analysis.wordCount) /
                analysis.sentenceCount;
            double avgSyllablesPerWord = estimateAverageSyllables(words);
            analysis.readabilityScore = 206.835 -
                                        (1.015 * avgWordsPerSentence) -
                                        (84.6 * avgSyllablesPerWord);
        }

        // Language detection (simplified)
        analysis.dominantLanguage = detectLanguage(textStr);
        analysis.languages.push_back(analysis.dominantLanguage);

        return analysis;
    }

    /**
     * @brief Translates a string to the specified locale
     * @param str The string to translate
     * @param domain The domain for the translation (not used in this
     * implementation)
     * @param localeName The name of the locale to use for translation
     * @return The translated string
     */
    [[nodiscard]] static std::string translate(
        std::string_view str, std::string_view /*domain*/,
        std::string_view localeName = "") {
        ::boost::locale::generator gen;
        std::locale loc = gen(std::string(localeName));
        return ::boost::locale::translate(std::string(str)).str(loc);
    }

    /**
     * @brief Converts a string to uppercase
     * @param str The string to convert
     * @return The uppercase string
     */
    [[nodiscard]] std::string toUpper(std::string_view str) const {
        return ::boost::locale::to_upper(std::string(str), locale_);
    }

    /**
     * @brief Converts a string to lowercase
     * @param str The string to convert
     * @return The lowercase string
     */
    [[nodiscard]] std::string toLower(std::string_view str) const {
        return ::boost::locale::to_lower(std::string(str), locale_);
    }

    /**
     * @brief Converts a string to title case
     * @param str The string to convert
     * @return The title case string
     */
    [[nodiscard]] std::string toTitle(std::string_view str) const {
        return ::boost::locale::to_title(std::string(str), locale_);
    }

    /**
     * @brief Compares two strings using locale-specific collation rules
     * @param str1 The first string to compare
     * @param str2 The second string to compare
     * @return An integer less than, equal to, or greater than zero if str1 is
     * found, respectively, to be less than, to match, or be greater than str2
     */
    [[nodiscard]] int compare(std::string_view str1,
                              std::string_view str2) const {
        return static_cast<int>(::boost::locale::comparator<
                                char, ::boost::locale::collate_level::primary>(
            locale_)(std::string(str1), std::string(str2)));
    }

    /**
     * @brief Formats a date and time according to the specified format
     * @param dateTime The date and time to format
     * @param format The format string
     * @return The formatted date and time string
     */
    [[nodiscard]] static std::string formatDate(
        const ::boost::posix_time::ptime& dateTime, std::string_view format) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << ::boost::locale::format(std::string(format)) % dateTime;
        return oss.str();
    }

    /**
     * @brief Formats a number with the specified precision
     * @param number The number to format
     * @param precision The number of decimal places
     * @return The formatted number string
     */
    [[nodiscard]] static std::string formatNumber(double number,
                                                  int precision = 2) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << std::fixed << std::setprecision(precision) << number;
        return oss.str();
    }

    /**
     * @brief Formats a currency amount
     * @param amount The amount to format
     * @param currency The currency code
     * @return The formatted currency string
     */
    [[nodiscard]] static std::string formatCurrency(double amount,
                                                    std::string_view currency) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << ::boost::locale::as::currency << std::string(currency) << amount;
        return oss.str();
    }

    /**
     * @brief Replaces occurrences of a regex pattern in a string with a format
     * string
     * @param str The string to search
     * @param regex The regex pattern to search for
     * @param format The format string to replace with
     * @return The resulting string after replacements
     */
    [[nodiscard]] static std::string regexReplace(std::string_view str,
                                                  const ::boost::regex& regex,
                                                  std::string_view format) {
        return ::boost::regex_replace(
            std::string(str), regex, std::string(format),
            ::boost::match_default | ::boost::format_all);
    }

    /**
     * @brief Formats a string with named arguments
     * @tparam Args The types of the arguments
     * @param formatString The format string
     * @param args The arguments to format
     * @return The formatted string
     */
    template <typename... Args>
    [[nodiscard]] std::string format(std::string_view formatString,
                                     Args&&... args) const {
        return (::boost::locale::format(std::string(formatString)) % ... %
                std::forward<Args>(args))
            .str(locale_);
    }

    /**
     * @brief Gets the current locale
     * @return The current locale
     */
    [[nodiscard]] const std::locale& getLocale() const noexcept {
        return locale_;
    }

    /**
     * @brief Sets a new locale with configuration update
     * @param localeName The name of the new locale
     */
    void setLocale(std::string_view localeName) {
        config_.name = std::string(localeName);
        locale_ = getOrCreateLocale(config_.name);
    }

    /**
     * @brief Phonetic matching using Soundex algorithm
     * @param word1 First word to compare
     * @param word2 Second word to compare
     * @return PhoneticMatch result with similarity score
     */
    [[nodiscard]] static PhoneticMatch phoneticMatch(std::string_view word1,
                                                     std::string_view word2) {
        ++total_operations_;

        PhoneticMatch result;
        result.original = std::string(word1) + " vs " + std::string(word2);
        result.algorithm = "Soundex";

        std::string soundex1 = generateSoundex(word1);
        std::string soundex2 = generateSoundex(word2);

        result.phonetic = soundex1 + " vs " + soundex2;
        result.similarity = (soundex1 == soundex2) ? 1.0 : 0.0;

        return result;
    }

    /**
     * @brief Fuzzy string matching with Levenshtein distance
     * @param str1 First string
     * @param str2 Second string
     * @return Similarity score between 0.0 and 1.0
     */
    [[nodiscard]] static double fuzzyMatch(std::string_view str1,
                                           std::string_view str2) {
        ++total_operations_;

        if (str1.empty() && str2.empty())
            return 1.0;
        if (str1.empty() || str2.empty())
            return 0.0;

        size_t distance = levenshteinDistance(str1, str2);
        size_t maxLen = std::max(str1.length(), str2.length());

        return 1.0 - (static_cast<double>(distance) / maxLen);
    }

    /**
     * @brief Gets performance statistics
     * @return Map of performance metrics
     */
    [[nodiscard]] static std::unordered_map<std::string, uint64_t>
    getStatistics() {
        return {{"cache_hits", cache_hits_.load()},
                {"cache_misses", cache_misses_.load()},
                {"total_operations", total_operations_.load()},
                {"cache_hit_ratio",
                 cache_hits_.load() + cache_misses_.load() > 0
                     ? (cache_hits_.load() * 100) /
                           (cache_hits_.load() + cache_misses_.load())
                     : 0}};
    }

    /**
     * @brief Resets performance statistics
     */
    static void resetStatistics() {
        cache_hits_.store(0);
        cache_misses_.store(0);
        total_operations_.store(0);
    }

    /**
     * @brief Clears all caches manually
     */
    static void clearCaches() {
        locale_cache_.clear();
        conversion_cache_.clear();
        last_cache_cleanup_ = std::chrono::steady_clock::now();
    }

private:
    LocaleConfig config_;
    std::locale locale_;
    static constexpr std::size_t BUFFER_SIZE = 4096;
    static constexpr std::size_t CACHE_SIZE = 1024;

    /**
     * @brief Gets or creates a locale from cache
     * @param localeName The locale name
     * @return The locale object
     */
    static std::locale getOrCreateLocale(const std::string& localeName) {
        cleanupCacheIfNeeded();

        auto it = locale_cache_.find(localeName);
        if (it != locale_cache_.end()) {
            ++cache_hits_;
            return it->second;
        }

        ++cache_misses_;
        ::boost::locale::generator gen;
        std::locale loc = gen(localeName.empty() ? "C" : localeName);

        if (locale_cache_.size() < CACHE_SIZE) {
            locale_cache_[localeName] = loc;
        }

        return loc;
    }

    /**
     * @brief Cleans up cache if needed
     */
    static void cleanupCacheIfNeeded() {
        auto now = std::chrono::steady_clock::now();
        if (now - last_cache_cleanup_ > std::chrono::minutes(30)) {
            locale_cache_.clear();
            conversion_cache_.clear();
            last_cache_cleanup_ = now;
        }
    }

    /**
     * @brief Gets cached conversion result
     * @param key Cache key
     * @return Cached result or empty optional
     */
    static std::optional<std::string> getCachedConversion(
        const std::string& key) {
        cleanupCacheIfNeeded();
        auto it = conversion_cache_.find(key);
        if (it != conversion_cache_.end()) {
            ++cache_hits_;
            return it->second;
        }
        ++cache_misses_;
        return std::nullopt;
    }

    /**
     * @brief Caches a conversion result
     * @param key Cache key
     * @param result Result to cache
     */
    static void cacheConversion(const std::string& key,
                                const std::string& result) {
        if (conversion_cache_.size() < CACHE_SIZE) {
            conversion_cache_[key] = result;
        }
    }

    /**
     * @brief Estimates average syllables per word (simplified heuristic)
     * @param words Vector of words
     * @return Average syllables per word
     */
    static double estimateAverageSyllables(
        const std::vector<std::string>& words) {
        if (words.empty())
            return 1.0;

        size_t totalSyllables = 0;
        for (const auto& word : words) {
            // Simple syllable counting heuristic
            size_t syllables = 1;  // At least one syllable
            for (size_t i = 1; i < word.length(); ++i) {
                char c = std::tolower(word[i]);
                char prev = std::tolower(word[i - 1]);
                if ((c == 'a' || c == 'e' || c == 'i' || c == 'o' ||
                     c == 'u') &&
                    !(prev == 'a' || prev == 'e' || prev == 'i' ||
                      prev == 'o' || prev == 'u')) {
                    syllables++;
                }
            }
            // Adjust for silent 'e'
            if (word.length() > 1 && std::tolower(word.back()) == 'e') {
                syllables = std::max(size_t{1}, syllables - 1);
            }
            totalSyllables += syllables;
        }

        return static_cast<double>(totalSyllables) / words.size();
    }

    /**
     * @brief Simple language detection based on character patterns
     * @param text Text to analyze
     * @return Detected language code
     */
    static std::string detectLanguage(const std::string& text) {
        // Simplified language detection based on character frequency
        std::unordered_map<char, size_t> charFreq;
        for (char c : text) {
            if (std::isalpha(c)) {
                charFreq[std::tolower(c)]++;
            }
        }

        // Simple heuristics for common languages
        if (charFreq['e'] > text.length() * 0.1) {
            return "en";  // English has high 'e' frequency
        } else if (charFreq['a'] > text.length() * 0.08) {
            return "es";  // Spanish has high 'a' frequency
        } else if (charFreq['i'] > text.length() * 0.08) {
            return "it";  // Italian has high 'i' frequency
        }

        return "unknown";
    }

    /**
     * @brief Generates Soundex code for phonetic matching
     * @param word Input word
     * @return Soundex code
     */
    static std::string generateSoundex(std::string_view word) {
        if (word.empty())
            return "0000";

        std::string soundex;
        soundex.reserve(4);

        // First character (uppercase)
        soundex += std::toupper(word[0]);

        // Soundex mapping
        std::unordered_map<char, char> soundexMap = {
            {'B', '1'}, {'F', '1'}, {'P', '1'}, {'V', '1'}, {'C', '2'},
            {'G', '2'}, {'J', '2'}, {'K', '2'}, {'Q', '2'}, {'S', '2'},
            {'X', '2'}, {'Z', '2'}, {'D', '3'}, {'T', '3'}, {'L', '4'},
            {'M', '5'}, {'N', '5'}, {'R', '6'}};

        char lastCode = '0';
        for (size_t i = 1; i < word.length() && soundex.length() < 4; ++i) {
            char c = std::toupper(word[i]);
            auto it = soundexMap.find(c);
            if (it != soundexMap.end() && it->second != lastCode) {
                soundex += it->second;
                lastCode = it->second;
            } else if (c == 'A' || c == 'E' || c == 'I' || c == 'O' ||
                       c == 'U' || c == 'Y' || c == 'H' || c == 'W') {
                lastCode = '0';  // Reset for vowels and H, W
            }
        }

        // Pad with zeros
        while (soundex.length() < 4) {
            soundex += '0';
        }

        return soundex;
    }

    /**
     * @brief Calculates Levenshtein distance between two strings
     * @param str1 First string
     * @param str2 Second string
     * @return Edit distance
     */
    static size_t levenshteinDistance(std::string_view str1,
                                      std::string_view str2) {
        const size_t len1 = str1.length();
        const size_t len2 = str2.length();

        std::vector<std::vector<size_t>> dp(len1 + 1,
                                            std::vector<size_t>(len2 + 1));

        // Initialize base cases
        for (size_t i = 0; i <= len1; ++i)
            dp[i][0] = i;
        for (size_t j = 0; j <= len2; ++j)
            dp[0][j] = j;

        // Fill the DP table
        for (size_t i = 1; i <= len1; ++i) {
            for (size_t j = 1; j <= len2; ++j) {
                if (str1[i - 1] == str2[j - 1]) {
                    dp[i][j] = dp[i - 1][j - 1];
                } else {
                    dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1],
                                             dp[i - 1][j - 1]});
                }
            }
        }

        return dp[len1][len2];
    }
};

// Static member definitions
inline thread_local std::unordered_map<std::string, std::locale>
    LocaleWrapper::locale_cache_{};
inline thread_local std::unordered_map<std::string, std::string>
    LocaleWrapper::conversion_cache_{};
inline thread_local std::chrono::steady_clock::time_point
    LocaleWrapper::last_cache_cleanup_{};
inline thread_local std::pmr::unsynchronized_pool_resource
    LocaleWrapper::memory_pool_{};
inline std::atomic<uint64_t> LocaleWrapper::cache_hits_{0};
inline std::atomic<uint64_t> LocaleWrapper::cache_misses_{0};
inline std::atomic<uint64_t> LocaleWrapper::total_operations_{0};

}  // namespace atom::extra::boost

#endif  // ATOM_EXTRA_BOOST_LOCALE_HPP
