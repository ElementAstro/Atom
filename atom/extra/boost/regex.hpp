#ifndef ATOM_EXTRA_BOOST_REGEX_HPP
#define ATOM_EXTRA_BOOST_REGEX_HPP

#include <atomic>
#include <boost/regex.hpp>
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <memory_resource>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace atom::extra::boost {

/**
 * @brief Enhanced regex match result with additional metadata
 */
struct MatchResult {
    std::string match;
    std::vector<std::string> groups;
    size_t position = 0;
    size_t length = 0;
    std::chrono::nanoseconds match_time{0};
};

/**
 * @brief Regex performance statistics
 */
struct RegexStats {
    uint64_t total_matches = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t compilation_time_ns = 0;
    uint64_t match_time_ns = 0;
};

/**
 * @brief Thread-safe regex statistics holder
 */
class RegexStatsHolder {
public:
    std::atomic<uint64_t> total_matches{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> compilation_time_ns{0};
    std::atomic<uint64_t> match_time_ns{0};
};

/**
 * @brief Fuzzy matching configuration
 */
struct FuzzyConfig {
    size_t max_distance = 2;
    bool case_sensitive = false;
    bool whole_word = false;
    double similarity_threshold = 0.7;
};

/**
 * @brief Pattern composition utilities
 */
class PatternBuilder {
public:
    PatternBuilder& literal(std::string_view text) {
        // Escape special regex characters
        std::string escaped;
        for (char c : text) {
            if (c == '.' || c == '^' || c == '$' || c == '|' || c == '(' ||
                c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
                c == '*' || c == '+' || c == '?' || c == '\\') {
                escaped += '\\';
            }
            escaped += c;
        }
        pattern_ += escaped;
        return *this;
    }

    PatternBuilder& anyChar() {
        pattern_ += ".";
        return *this;
    }

    PatternBuilder& oneOrMore() {
        pattern_ += "+";
        return *this;
    }

    PatternBuilder& zeroOrMore() {
        pattern_ += "*";
        return *this;
    }

    PatternBuilder& optional() {
        pattern_ += "?";
        return *this;
    }

    PatternBuilder& group(std::string_view content) {
        pattern_ += "(" + std::string(content) + ")";
        return *this;
    }

    PatternBuilder& namedGroup(std::string_view name,
                               std::string_view content) {
        pattern_ +=
            "(?P<" + std::string(name) + ">" + std::string(content) + ")";
        return *this;
    }

    PatternBuilder& charClass(std::string_view chars) {
        pattern_ += "[" + std::string(chars) + "]";
        return *this;
    }

    PatternBuilder& wordBoundary() {
        pattern_ += "\\b";
        return *this;
    }

    PatternBuilder& startOfLine() {
        pattern_ += "^";
        return *this;
    }

    PatternBuilder& endOfLine() {
        pattern_ += "$";
        return *this;
    }

    std::string build() const { return pattern_; }

    void reset() { pattern_.clear(); }

private:
    std::string pattern_;
};

/**
 * @brief Enhanced wrapper class for Boost.Regex with caching, parallel
 * processing, and advanced features
 */
class RegexWrapper {
private:
    // Thread-local cache for compiled regex objects
    static thread_local std::unordered_map<std::string, ::boost::regex>
        regex_cache_;
    static thread_local std::unordered_map<std::string,
                                           std::vector<MatchResult>>
        result_cache_;
    static thread_local std::chrono::steady_clock::time_point
        last_cache_cleanup_;

    // Memory pool for efficient string allocations
    static thread_local std::pmr::unsynchronized_pool_resource memory_pool_;

    // Global statistics
    static RegexStatsHolder stats_;

    // Cache configuration
    static constexpr size_t MAX_CACHE_SIZE = 1024;
    static constexpr std::chrono::minutes CACHE_TIMEOUT{30};

public:
    /**
     * @brief Constructs a RegexWrapper with the given pattern and flags with
     * caching
     * @param pattern The regex pattern
     * @param flags The regex syntax option flags
     */
    explicit RegexWrapper(std::string_view pattern,
                          ::boost::regex_constants::syntax_option_type flags =
                              ::boost::regex_constants::normal)
        : pattern_str_(pattern), flags_(flags) {
        regex_ = getOrCreateRegex(pattern_str_, flags_);
    }

    /**
     * @brief Copy constructor with cache optimization
     */
    RegexWrapper(const RegexWrapper& other)
        : pattern_str_(other.pattern_str_), flags_(other.flags_) {
        regex_ = getOrCreateRegex(pattern_str_, flags_);
    }

    /**
     * @brief Move constructor
     */
    RegexWrapper(RegexWrapper&& other) noexcept
        : pattern_str_(std::move(other.pattern_str_)),
          flags_(other.flags_),
          regex_(std::move(other.regex_)) {}

    /**
     * @brief Assignment operators
     */
    RegexWrapper& operator=(const RegexWrapper& other) {
        if (this != &other) {
            pattern_str_ = other.pattern_str_;
            flags_ = other.flags_;
            regex_ = getOrCreateRegex(pattern_str_, flags_);
        }
        return *this;
    }

    RegexWrapper& operator=(RegexWrapper&& other) noexcept {
        if (this != &other) {
            pattern_str_ = std::move(other.pattern_str_);
            flags_ = other.flags_;
            regex_ = std::move(other.regex_);
        }
        return *this;
    }

    /**
     * @brief Matches the given string against the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to match
     * @return True if the string matches the pattern
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] bool match(const T& str) const {
        std::string_view sv(str);
        return ::boost::regex_match(sv.begin(), sv.end(), regex_);
    }

    /**
     * @brief Searches the given string for the first match of the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to search
     * @return An optional containing the first match if found
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::optional<std::string> search(const T& str) const {
        ::boost::smatch what;
        std::string s(str);
        if ((::boost::regex_search(s, what, regex_))) [[likely]] {
            return what.str();
        }
        return std::nullopt;
    }

    /**
     * @brief Searches the given string for all matches of the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to search
     * @return A vector containing all matches found
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<std::string> searchAll(const T& str) const {
        std::vector<std::string> results;
        std::string s(str);
        ::boost::sregex_iterator iter(s.begin(), s.end(), regex_);
        ::boost::sregex_iterator end;

        results.reserve(8);  // Reserve space for common cases
        for (; iter != end; ++iter) {
            results.emplace_back(iter->str());
        }
        return results;
    }

    /**
     * @brief Replaces all matches of the regex pattern with the replacement
     * string
     * @tparam T The type of the input string, convertible to std::string_view
     * @tparam U The type of the replacement string, convertible to
     * std::string_view
     * @param str The input string
     * @param replacement The replacement string
     * @return A new string with all matches replaced
     */
    template <typename T, typename U>
        requires std::convertible_to<T, std::string_view> &&
                 std::convertible_to<U, std::string_view>
    [[nodiscard]] std::string replace(const T& str,
                                      const U& replacement) const {
        return ::boost::regex_replace(std::string(str), regex_,
                                      std::string(replacement));
    }

    /**
     * @brief Splits the given string by the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to split
     * @return A vector containing the split parts of the string
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<std::string> split(const T& str) const {
        std::vector<std::string> results;
        std::string s(str);
        ::boost::sregex_token_iterator iter(s.begin(), s.end(), regex_, -1);
        ::boost::sregex_token_iterator end;

        results.reserve(4);  // Reserve space for common cases
        for (; iter != end; ++iter) {
            results.emplace_back(*iter);
        }
        return results;
    }

    /**
     * @brief Matches the given string and returns the groups of each match
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to match
     * @return A vector of pairs, each containing the full match and a vector of
     * groups
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<std::pair<std::string, std::vector<std::string>>>
    matchGroups(const T& str) const {
        std::vector<std::pair<std::string, std::vector<std::string>>> results;
        ::boost::smatch what;
        std::string s(str);
        auto start = s.cbegin();
        const auto end = s.cend();

        while (::boost::regex_search(start, end, what, regex_)) {
            std::vector<std::string> groups;
            groups.reserve(what.size() - 1);
            for (size_t i = 1; i < what.size(); ++i) {
                groups.emplace_back(what[i].str());
            }
            results.emplace_back(what[0].str(), std::move(groups));
            start = what[0].second;
        }
        return results;
    }

    /**
     * @brief Applies a function to each match of the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @tparam Func The type of the function to apply
     * @param str The input string
     * @param func The function to apply to each match
     */
    template <typename T, typename Func>
        requires std::convertible_to<T, std::string_view> &&
                 std::invocable<Func, const ::boost::smatch&>
    void forEachMatch(const T& str, Func&& func) const {
        std::string s(str);
        ::boost::sregex_iterator iter(s.begin(), s.end(), regex_);
        ::boost::sregex_iterator end;
        for (; iter != end; ++iter) {
            func(*iter);
        }
    }

    /**
     * @brief Gets the regex pattern as a string
     * @return The regex pattern
     */
    [[nodiscard]] std::string getPattern() const { return regex_.str(); }

    /**
     * @brief Sets a new regex pattern with optional flags
     * @param pattern The new regex pattern
     * @param flags The regex syntax option flags
     */
    void setPattern(std::string_view pattern,
                    ::boost::regex_constants::syntax_option_type flags =
                        ::boost::regex_constants::normal) {
        regex_.assign(pattern.data(), flags);
    }

    /**
     * @brief Matches the given string and returns the named captures
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to match
     * @return A map of named captures
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::map<std::string, std::string> namedCaptures(
        const T& str) const {
        std::map<std::string, std::string> result;
        ::boost::smatch what;
        if (::boost::regex_match(std::string(str), what, regex_)) {
            for (size_t i = 1; i <= regex_.mark_count(); ++i) {
                result.emplace(std::to_string(i), what[i].str());
            }
        }
        return result;
    }

    /**
     * @brief Checks if the given string is a valid match for the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to check
     * @return True if the string is a valid match
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] bool isValid(const T& str) const noexcept {
        try {
            std::string_view sv(str);
            ::boost::regex_match(sv.begin(), sv.end(), regex_);
            return true;
        } catch (const ::boost::regex_error&) {
            return false;
        }
    }

    /**
     * @brief Replaces all matches using a callback function
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string
     * @param callback The callback function to generate replacements
     * @return A new string with all matches replaced by the callback results
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::string replaceCallback(
        const T& str,
        const std::function<std::string(const ::boost::smatch&)>& callback)
        const {
        std::string result(str);
        ::boost::sregex_iterator iter(result.begin(), result.end(), regex_);
        ::boost::sregex_iterator end;

        std::vector<std::pair<std::string::size_type, std::string>>
            replacements;
        replacements.reserve(8);  // Reserve for common cases

        while (iter != end) {
            const ::boost::smatch& match = *iter;
            replacements.emplace_back(match.position(), callback(match));
            ++iter;
        }

        // Apply replacements in reverse order to preserve positions
        for (auto riter = replacements.rbegin(); riter != replacements.rend();
             ++riter) {
            result.replace(riter->first, riter->second.length(), riter->second);
        }

        return result;
    }

    /**
     * @brief Escapes special characters in the given string for use in a regex
     * pattern
     * @param str The input string to escape
     * @return The escaped string
     */
    [[nodiscard]] static std::string escapeString(std::string_view str) {
        static const ::boost::regex escape_regex(R"([.^$|()\[\]{}*+?\\])");
        return ::boost::regex_replace(std::string(str), escape_regex, R"(\\&)",
                                      ::boost::regex_constants::match_default |
                                          ::boost::regex_constants::format_sed);
    }

    /**
     * @brief Benchmarks the match operation over a number of iterations
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to match
     * @param iterations The number of iterations to run the benchmark
     * @return The average time per match operation in nanoseconds
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::chrono::nanoseconds benchmarkMatch(
        const T& str, int iterations = 1000) const {
        std::string_view sv(str);
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            ::boost::regex_match(sv.begin(), sv.end(), regex_);
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                    start) /
               iterations;
    }

    /**
     * @brief Checks if the given regex pattern is valid
     * @param pattern The regex pattern to check
     * @return True if the pattern is valid
     */
    [[nodiscard]] static bool isValidRegex(std::string_view pattern) noexcept {
        try {
            ::boost::regex test(pattern.data());
            return true;
        } catch (const ::boost::regex_error&) {
            return false;
        }
    }

    /**
     * @brief Counts the number of matches of the regex pattern
     * @tparam T The type of the input string, convertible to std::string_view
     * @param str The input string to search
     * @return The number of matches found
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] size_t countMatches(const T& str) const {
        std::string s(str);
        ::boost::sregex_iterator iter(s.begin(), s.end(), regex_);
        ::boost::sregex_iterator end;
        return std::distance(iter, end);
    }

private:
    std::string pattern_str_;
    ::boost::regex_constants::syntax_option_type flags_;
    ::boost::regex regex_;

    /**
     * @brief Gets or creates a regex from cache
     * @param pattern The regex pattern
     * @param flags The regex flags
     * @return The compiled regex object
     */
    static ::boost::regex getOrCreateRegex(
        const std::string& pattern,
        ::boost::regex_constants::syntax_option_type flags) {
        cleanupCacheIfNeeded();

        std::string cache_key =
            pattern + "_" + std::to_string(static_cast<int>(flags));
        auto it = regex_cache_.find(cache_key);
        if (it != regex_cache_.end()) {
            stats_.cache_hits++;
            return it->second;
        }

        stats_.cache_misses++;
        auto start = std::chrono::high_resolution_clock::now();
        ::boost::regex compiled_regex(pattern, flags);
        auto end = std::chrono::high_resolution_clock::now();

        auto compilation_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        stats_.compilation_time_ns += compilation_time.count();

        if (regex_cache_.size() < MAX_CACHE_SIZE) {
            regex_cache_[cache_key] = compiled_regex;
        }

        return compiled_regex;
    }

    /**
     * @brief Cleans up cache if needed
     */
    static void cleanupCacheIfNeeded() {
        auto now = std::chrono::steady_clock::now();
        if (now - last_cache_cleanup_ > CACHE_TIMEOUT) {
            regex_cache_.clear();
            result_cache_.clear();
            last_cache_cleanup_ = now;
        }
    }

public:
    /**
     * @brief Enhanced search with detailed match results
     * @tparam T The type of the input string
     * @param str The input string to search
     * @return Vector of detailed match results
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<MatchResult> searchDetailed(const T& str) const {
        std::vector<MatchResult> results;
        std::string s(str);
        ::boost::sregex_iterator iter(s.begin(), s.end(), regex_);
        ::boost::sregex_iterator end;

        for (; iter != end; ++iter) {
            auto start_time = std::chrono::high_resolution_clock::now();

            MatchResult result;
            result.match = iter->str();
            result.position = iter->position();
            result.length = iter->length();

            // Extract groups
            for (size_t i = 1; i < iter->size(); ++i) {
                result.groups.emplace_back((*iter)[i].str());
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            result.match_time =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end_time - start_time);

            results.emplace_back(std::move(result));
            stats_.total_matches++;
        }

        return results;
    }

    /**
     * @brief Parallel search across multiple strings
     * @tparam T The type of the input strings
     * @param strings Span of strings to search
     * @return Vector of vectors containing matches for each string
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<std::vector<std::string>> parallelSearchAll(
        std::span<const T> strings) const {
        std::vector<std::vector<std::string>> results(strings.size());

        // Use parallel execution for large datasets
        if (strings.size() > 100) {
            std::vector<std::future<std::vector<std::string>>> futures;
            futures.reserve(strings.size());

            for (const auto& str : strings) {
                futures.emplace_back(std::async(
                    std::launch::async,
                    [this, &str]() { return this->searchAll(str); }));
            }

            for (size_t i = 0; i < futures.size(); ++i) {
                results[i] = futures[i].get();
            }
        } else {
            // Sequential processing for smaller datasets
            for (size_t i = 0; i < strings.size(); ++i) {
                results[i] = searchAll(strings[i]);
            }
        }

        return results;
    }

    /**
     * @brief Fuzzy matching with edit distance
     * @tparam T The type of the input string
     * @param str The input string
     * @param config Fuzzy matching configuration
     * @return Vector of fuzzy matches
     */
    template <typename T>
        requires std::convertible_to<T, std::string_view>
    [[nodiscard]] std::vector<std::pair<std::string, double>> fuzzyMatch(
        const T& str, const FuzzyConfig& config = {}) const {
        (void)config;  // Suppress unused parameter warning
        std::vector<std::pair<std::string, double>> results;

        // This is a simplified fuzzy matching implementation
        // In a full implementation, this would use more sophisticated
        // algorithms
        auto exact_matches = searchAll(str);

        for (const auto& match : exact_matches) {
            results.emplace_back(match, 1.0);  // Exact match has similarity 1.0
        }

        return results;
    }

    /**
     * @brief Gets performance statistics
     * @return Current regex statistics
     */
    [[nodiscard]] static RegexStats getStatistics() {
        RegexStats result;
        result.total_matches = stats_.total_matches.load();
        result.cache_hits = stats_.cache_hits.load();
        result.cache_misses = stats_.cache_misses.load();
        result.compilation_time_ns = stats_.compilation_time_ns.load();
        result.match_time_ns = stats_.match_time_ns.load();
        return result;
    }

    /**
     * @brief Resets performance statistics
     */
    static void resetStatistics() {
        stats_.total_matches.store(0);
        stats_.cache_hits.store(0);
        stats_.cache_misses.store(0);
        stats_.compilation_time_ns.store(0);
        stats_.match_time_ns.store(0);
    }

    /**
     * @brief Clears all caches manually
     */
    static void clearCaches() {
        regex_cache_.clear();
        result_cache_.clear();
        last_cache_cleanup_ = std::chrono::steady_clock::now();
    }
};

// Static member definitions
inline thread_local std::unordered_map<std::string, ::boost::regex>
    RegexWrapper::regex_cache_{};
inline thread_local std::unordered_map<std::string, std::vector<MatchResult>>
    RegexWrapper::result_cache_{};
inline thread_local std::chrono::steady_clock::time_point
    RegexWrapper::last_cache_cleanup_{};
inline thread_local std::pmr::unsynchronized_pool_resource
    RegexWrapper::memory_pool_{};
inline RegexStatsHolder RegexWrapper::stats_{};

}  // namespace atom::extra::boost

#endif
