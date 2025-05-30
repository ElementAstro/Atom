#ifndef ATOM_EXTRA_BOOST_REGEX_HPP
#define ATOM_EXTRA_BOOST_REGEX_HPP

#include <boost/regex.hpp>
#include <chrono>
#include <concepts>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace atom::extra::boost {

/**
 * @brief A wrapper class for Boost.Regex providing various regex operations
 */
class RegexWrapper {
public:
    /**
     * @brief Constructs a RegexWrapper with the given pattern and flags
     * @param pattern The regex pattern
     * @param flags The regex syntax option flags
     */
    explicit RegexWrapper(std::string_view pattern,
                          ::boost::regex_constants::syntax_option_type flags =
                              ::boost::regex_constants::normal)
        : regex_(pattern.data(), flags) {}

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
    ::boost::regex regex_;
};

}  // namespace atom::extra::boost

#endif
