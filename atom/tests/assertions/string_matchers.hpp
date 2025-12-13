/**
 * @file string_matchers.hpp
 * @brief Advanced string matchers for test assertions
 * @details Provides specialized matchers for string comparisons and patterns
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_ASSERTIONS_STRING_MATCHERS_HPP
#define ATOM_TEST_ASSERTIONS_STRING_MATCHERS_HPP

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {
namespace string_matchers {

/**
 * @brief Case-insensitive string comparison helper
 */
inline bool caseInsensitiveEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if string is blank (empty or whitespace only)
 */
class IsBlankMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c);
        });
    }

    [[nodiscard]] std::string describe() const {
        return "is blank (empty or whitespace only)";
    }
};

inline auto IsBlank() { return IsBlankMatcher{}; }

/**
 * @brief Check if string is not blank
 */
class IsNotBlankMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        return !std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c);
        });
    }

    [[nodiscard]] std::string describe() const { return "is not blank"; }
};

inline auto IsNotBlank() { return IsNotBlankMatcher{}; }

/**
 * @brief Check string length
 */
class HasLengthMatcher {
public:
    explicit HasLengthMatcher(size_t expected) : expected_(expected) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value.length() == expected_;
    }

    [[nodiscard]] std::string describe() const {
        return "has length " + std::to_string(expected_);
    }

    [[nodiscard]] std::string describeMismatch(const std::string& value) const {
        return "has length " + std::to_string(value.length());
    }

private:
    size_t expected_;
};

inline auto HasLength(size_t expected) { return HasLengthMatcher(expected); }

/**
 * @brief Check string length in range
 */
class HasLengthBetweenMatcher {
public:
    HasLengthBetweenMatcher(size_t min, size_t max) : min_(min), max_(max) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value.length() >= min_ && value.length() <= max_;
    }

    [[nodiscard]] std::string describe() const {
        return "has length between " + std::to_string(min_) + " and " +
               std::to_string(max_);
    }

private:
    size_t min_;
    size_t max_;
};

inline auto HasLengthBetween(size_t min, size_t max) {
    return HasLengthBetweenMatcher(min, max);
}

/**
 * @brief Case-insensitive contains matcher
 */
class ContainsIgnoreCaseMatcher {
public:
    explicit ContainsIgnoreCaseMatcher(std::string substr)
        : substr_(std::move(substr)) {
        // Convert to lowercase for comparison
        std::transform(substr_.begin(), substr_.end(), substr_.begin(),
                       [](unsigned char c) { return std::tolower(c); });
    }

    [[nodiscard]] bool matches(const std::string& value) const {
        std::string lowerValue = value;
        std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return lowerValue.find(substr_) != std::string::npos;
    }

    [[nodiscard]] std::string describe() const {
        return "contains (ignoring case) \"" + substr_ + "\"";
    }

private:
    std::string substr_;
};

inline auto ContainsIgnoreCase(std::string substr) {
    return ContainsIgnoreCaseMatcher(std::move(substr));
}

/**
 * @brief Starts with (case-insensitive) matcher
 */
class StartsWithIgnoreCaseMatcher {
public:
    explicit StartsWithIgnoreCaseMatcher(std::string prefix)
        : prefix_(std::move(prefix)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.size() < prefix_.size()) return false;
        return caseInsensitiveEquals(value.substr(0, prefix_.size()), prefix_);
    }

    [[nodiscard]] std::string describe() const {
        return "starts with (ignoring case) \"" + prefix_ + "\"";
    }

private:
    std::string prefix_;
};

inline auto StartsWithIgnoreCase(std::string prefix) {
    return StartsWithIgnoreCaseMatcher(std::move(prefix));
}

/**
 * @brief Ends with (case-insensitive) matcher
 */
class EndsWithIgnoreCaseMatcher {
public:
    explicit EndsWithIgnoreCaseMatcher(std::string suffix)
        : suffix_(std::move(suffix)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.size() < suffix_.size()) return false;
        return caseInsensitiveEquals(
            value.substr(value.size() - suffix_.size()), suffix_);
    }

    [[nodiscard]] std::string describe() const {
        return "ends with (ignoring case) \"" + suffix_ + "\"";
    }

private:
    std::string suffix_;
};

inline auto EndsWithIgnoreCase(std::string suffix) {
    return EndsWithIgnoreCaseMatcher(std::move(suffix));
}

/**
 * @brief Contains all substrings matcher
 */
class ContainsAllMatcher {
public:
    ContainsAllMatcher(std::initializer_list<std::string> substrs)
        : substrs_(substrs) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return std::all_of(
            substrs_.begin(), substrs_.end(),
            [&value](const std::string& s) {
                return value.find(s) != std::string::npos;
            });
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "contains all of [";
        for (size_t i = 0; i < substrs_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << substrs_[i] << "\"";
        }
        oss << "]";
        return oss.str();
    }

private:
    std::vector<std::string> substrs_;
};

inline auto ContainsAll(std::initializer_list<std::string> substrs) {
    return ContainsAllMatcher(substrs);
}

/**
 * @brief Contains any substring matcher
 */
class ContainsAnyMatcher {
public:
    ContainsAnyMatcher(std::initializer_list<std::string> substrs)
        : substrs_(substrs) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return std::any_of(
            substrs_.begin(), substrs_.end(),
            [&value](const std::string& s) {
                return value.find(s) != std::string::npos;
            });
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "contains any of [";
        for (size_t i = 0; i < substrs_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << substrs_[i] << "\"";
        }
        oss << "]";
        return oss.str();
    }

private:
    std::vector<std::string> substrs_;
};

inline auto ContainsAny(std::initializer_list<std::string> substrs) {
    return ContainsAnyMatcher(substrs);
}

/**
 * @brief Contains none of substrings matcher
 */
class ContainsNoneMatcher {
public:
    ContainsNoneMatcher(std::initializer_list<std::string> substrs)
        : substrs_(substrs) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return std::none_of(
            substrs_.begin(), substrs_.end(),
            [&value](const std::string& s) {
                return value.find(s) != std::string::npos;
            });
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "contains none of [";
        for (size_t i = 0; i < substrs_.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << substrs_[i] << "\"";
        }
        oss << "]";
        return oss.str();
    }

private:
    std::vector<std::string> substrs_;
};

inline auto ContainsNone(std::initializer_list<std::string> substrs) {
    return ContainsNoneMatcher(substrs);
}

/**
 * @brief Is numeric string matcher
 */
class IsNumericMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return false;
        size_t start = 0;
        if (value[0] == '-' || value[0] == '+') start = 1;
        if (start >= value.size()) return false;

        bool hasDecimal = false;
        for (size_t i = start; i < value.size(); ++i) {
            if (value[i] == '.') {
                if (hasDecimal) return false;
                hasDecimal = true;
            } else if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string describe() const { return "is numeric"; }
};

inline auto IsNumeric() { return IsNumericMatcher{}; }

/**
 * @brief Is integer string matcher
 */
class IsIntegerMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return false;
        size_t start = 0;
        if (value[0] == '-' || value[0] == '+') start = 1;
        if (start >= value.size()) return false;

        return std::all_of(value.begin() + static_cast<long>(start),
                           value.end(), [](unsigned char c) {
                               return std::isdigit(c);
                           });
    }

    [[nodiscard]] std::string describe() const { return "is integer"; }
};

inline auto IsInteger() { return IsIntegerMatcher{}; }

/**
 * @brief Is alphanumeric matcher
 */
class IsAlphanumericMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalnum(c);
        });
    }

    [[nodiscard]] std::string describe() const { return "is alphanumeric"; }
};

inline auto IsAlphanumeric() { return IsAlphanumericMatcher{}; }

/**
 * @brief Is alphabetic matcher
 */
class IsAlphabeticMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return false;
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalpha(c);
        });
    }

    [[nodiscard]] std::string describe() const { return "is alphabetic"; }
};

inline auto IsAlphabetic() { return IsAlphabeticMatcher{}; }

/**
 * @brief Is uppercase matcher
 */
class IsUppercaseMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return !std::isalpha(c) || std::isupper(c);
        });
    }

    [[nodiscard]] std::string describe() const { return "is uppercase"; }
};

inline auto IsUppercase() { return IsUppercaseMatcher{}; }

/**
 * @brief Is lowercase matcher
 */
class IsLowercaseMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        return std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return !std::isalpha(c) || std::islower(c);
        });
    }

    [[nodiscard]] std::string describe() const { return "is lowercase"; }
};

inline auto IsLowercase() { return IsLowercaseMatcher{}; }

/**
 * @brief Matches word boundary regex
 */
class ContainsWordMatcher {
public:
    explicit ContainsWordMatcher(std::string word)
        : word_(std::move(word)),
          pattern_("\\b" + word_ + "\\b") {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return std::regex_search(value, pattern_);
    }

    [[nodiscard]] std::string describe() const {
        return "contains word \"" + word_ + "\"";
    }

private:
    std::string word_;
    std::regex pattern_;
};

inline auto ContainsWord(std::string word) {
    return ContainsWordMatcher(std::move(word));
}

/**
 * @brief Line count matcher
 */
class HasLineCountMatcher {
public:
    explicit HasLineCountMatcher(size_t expected) : expected_(expected) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return expected_ == 0;
        size_t count = std::count(value.begin(), value.end(), '\n');
        // Add 1 if string doesn't end with newline
        if (!value.empty() && value.back() != '\n') count++;
        return count == expected_;
    }

    [[nodiscard]] std::string describe() const {
        return "has " + std::to_string(expected_) + " line(s)";
    }

private:
    size_t expected_;
};

inline auto HasLineCount(size_t expected) {
    return HasLineCountMatcher(expected);
}

/**
 * @brief Matches trimmed string
 */
class WhenTrimmedMatcher {
public:
    template <typename InnerMatcher>
    explicit WhenTrimmedMatcher(InnerMatcher inner) : inner_(inner) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        std::string trimmed = value;
        // Trim left
        trimmed.erase(trimmed.begin(),
                      std::find_if(trimmed.begin(), trimmed.end(),
                                   [](unsigned char ch) {
                                       return !std::isspace(ch);
                                   }));
        // Trim right
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(),
                                   [](unsigned char ch) {
                                       return !std::isspace(ch);
                                   })
                          .base(),
                      trimmed.end());

        return inner_.matches(trimmed);
    }

    [[nodiscard]] std::string describe() const {
        return "when trimmed, " + inner_.describe();
    }

private:
    std::function<bool(const std::string&)> inner_;
};

/**
 * @brief Email format matcher
 */
class IsEmailMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        // Simple email regex
        static const std::regex emailPattern(
            R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(value, emailPattern);
    }

    [[nodiscard]] std::string describe() const {
        return "is a valid email address";
    }
};

inline auto IsEmail() { return IsEmailMatcher{}; }

/**
 * @brief URL format matcher
 */
class IsUrlMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        static const std::regex urlPattern(
            R"(^(https?|ftp)://[^\s/$.?#].[^\s]*$)", std::regex::icase);
        return std::regex_match(value, urlPattern);
    }

    [[nodiscard]] std::string describe() const { return "is a valid URL"; }
};

inline auto IsUrl() { return IsUrlMatcher{}; }

/**
 * @brief UUID format matcher
 */
class IsUuidMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        static const std::regex uuidPattern(
            R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
        return std::regex_match(value, uuidPattern);
    }

    [[nodiscard]] std::string describe() const { return "is a valid UUID"; }
};

inline auto IsUuid() { return IsUuidMatcher{}; }

/**
 * @brief JSON format matcher (basic)
 */
class IsJsonMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.empty()) return false;
        // Very basic check - starts with { or [ and ends with } or ]
        char first = value.front();
        char last = value.back();
        return (first == '{' && last == '}') || (first == '[' && last == ']');
    }

    [[nodiscard]] std::string describe() const {
        return "appears to be JSON";
    }
};

inline auto IsJson() { return IsJsonMatcher{}; }

/**
 * @brief Palindrome matcher
 */
class IsPalindromeMatcher {
public:
    [[nodiscard]] bool matches(const std::string& value) const {
        std::string cleaned;
        for (char c : value) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                cleaned += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }
        }
        std::string reversed = cleaned;
        std::reverse(reversed.begin(), reversed.end());
        return cleaned == reversed;
    }

    [[nodiscard]] std::string describe() const { return "is a palindrome"; }
};

inline auto IsPalindrome() { return IsPalindromeMatcher{}; }

}  // namespace string_matchers

// Import into atom::test namespace
using namespace string_matchers;

}  // namespace atom::test

// Assertion macros for string matchers
#define expect_string_blank(str) \
    expect_that(str, atom::test::IsBlank(), "Expected blank string")

#define expect_string_not_blank(str) \
    expect_that(str, atom::test::IsNotBlank(), "Expected non-blank string")

#define expect_string_length(str, len) \
    expect_that(str, atom::test::HasLength(len), "Expected length " #len)

#define expect_string_numeric(str) \
    expect_that(str, atom::test::IsNumeric(), "Expected numeric string")

#define expect_string_email(str) \
    expect_that(str, atom::test::IsEmail(), "Expected valid email")

#define expect_string_url(str) \
    expect_that(str, atom::test::IsUrl(), "Expected valid URL")

#define expect_string_uuid(str) \
    expect_that(str, atom::test::IsUuid(), "Expected valid UUID")

#endif  // ATOM_TEST_ASSERTIONS_STRING_MATCHERS_HPP
