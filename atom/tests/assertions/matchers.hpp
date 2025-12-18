/**
 * @file matchers.hpp
 * @brief GTest-style matchers for flexible assertions
 * @details Provides composable matchers for expressive test assertions
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_ASSERTIONS_MATCHERS_HPP
#define ATOM_TEST_ASSERTIONS_MATCHERS_HPP

#include <algorithm>
#include <cctype>
#include <cmath>
#include <concepts>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {
namespace matchers {

template <typename T>
concept StreamInsertable = requires(std::ostream& os, const T& v) {
    { os << v } -> std::same_as<std::ostream&>;
};

template <typename T>
concept HasMatches = requires(const T& m, const int& v) {
    { m.matches(v) } -> std::convertible_to<bool>;
};

/**
 * @brief Base interface for all matchers
 * @tparam T The type being matched
 */
template <typename T>
class Matcher {
public:
    virtual ~Matcher() = default;

    /**
     * @brief Check if the value matches
     * @param value The value to test
     * @return True if matches
     */
    [[nodiscard]] virtual bool matches(const T& value) const = 0;

    /**
     * @brief Get description of what was expected
     * @return Human-readable description
     */
    [[nodiscard]] virtual std::string describe() const = 0;

    /**
     * @brief Get description of the actual mismatch
     * @param value The actual value
     * @return Human-readable mismatch description
     */
    [[nodiscard]] virtual std::string describeMismatch(const T& value) const {
        std::ostringstream oss;
        oss << "was: ";
        if constexpr (StreamInsertable<T>) {
            oss << value;
        } else {
            oss << "<unprintable>";
        }
        return oss.str();
    }
};

/**
 * @brief Matcher result with detailed information
 */
struct MatchResult {
    bool success;
    std::string description;
    std::string mismatchDescription;

    explicit operator bool() const { return success; }
};

// ============================================================================
// Comparison Matchers
// ============================================================================

/**
 * @brief Equality matcher
 */
template <typename T>
class EqMatcher : public Matcher<T> {
public:
    explicit EqMatcher(T expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value == expected_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is equal to ";
        if constexpr (StreamInsertable<T>) {
            oss << expected_;
        } else {
            oss << "<unprintable>";
        }
        return oss.str();
    }

private:
    T expected_;
};

template <typename T>
auto Eq(T expected) {
    return EqMatcher<T>(std::move(expected));
}

inline auto FloatEq(float expected) { return Eq(expected); }

inline auto DoubleEq(double expected) { return Eq(expected); }

template <typename T>
class NeMatcher : public Matcher<T> {
public:
    explicit NeMatcher(T expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value != expected_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is not equal to ";
        if constexpr (StreamInsertable<T>) {
            oss << expected_;
        } else {
            oss << "<unprintable>";
        }
        return oss.str();
    }

private:
    T expected_;
};

template <typename T>
auto Ne(T expected) {
    return NeMatcher<T>(std::move(expected));
}

template <typename T>
class LtMatcher : public Matcher<T> {
public:
    explicit LtMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value < bound_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is less than " << bound_;
        return oss.str();
    }

private:
    T bound_;
};

template <typename T>
auto Lt(T bound) {
    return LtMatcher<T>(std::move(bound));
}

template <typename T>
class LeMatcher : public Matcher<T> {
public:
    explicit LeMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value <= bound_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is less than or equal to " << bound_;
        return oss.str();
    }

private:
    T bound_;
};

template <typename T>
auto Le(T bound) {
    return LeMatcher<T>(std::move(bound));
}

template <typename T>
class GtMatcher : public Matcher<T> {
public:
    explicit GtMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value > bound_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is greater than " << bound_;
        return oss.str();
    }

private:
    T bound_;
};

template <typename T>
auto Gt(T bound) {
    return GtMatcher<T>(std::move(bound));
}

template <typename T>
class GeMatcher : public Matcher<T> {
public:
    explicit GeMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value >= bound_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is greater than or equal to " << bound_;
        return oss.str();
    }

private:
    T bound_;
};

template <typename T>
auto Ge(T bound) {
    return GeMatcher<T>(std::move(bound));
}

// ============================================================================
// Floating Point Matchers
// ============================================================================

/**
 * @brief Near equality matcher for floating point
 */
template <typename T>
class NearMatcher : public Matcher<T> {
public:
    NearMatcher(T expected, T tolerance)
        : expected_(expected), tolerance_(tolerance) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return std::abs(value - expected_) <= tolerance_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is within " << tolerance_ << " of " << expected_;
        return oss.str();
    }

    [[nodiscard]] std::string describeMismatch(const T& value) const override {
        std::ostringstream oss;
        oss << "was ";
        if constexpr (StreamInsertable<T>) {
            oss << value;
        } else {
            oss << "<unprintable>";
        }
        oss << " (difference: " << std::abs(value - expected_) << ")";
        return oss.str();
    }

private:
    T expected_;
    T tolerance_;
};

template <typename T>
auto DoubleNear(T expected, T tolerance) {
    return NearMatcher<T>(expected, tolerance);
}

template <typename T>
auto FloatNear(T expected, T tolerance) {
    return NearMatcher<T>(expected, tolerance);
}

template <typename T>
class IsNanMatcher : public Matcher<T> {
public:
    [[nodiscard]] bool matches(const T& value) const override {
        return std::isnan(value);
    }

    [[nodiscard]] std::string describe() const override { return "is NaN"; }
};

template <typename T = double>
auto IsNan() {
    return IsNanMatcher<T>();
}

template <typename T>
class IsInfMatcher : public Matcher<T> {
public:
    [[nodiscard]] bool matches(const T& value) const override {
        return std::isinf(value);
    }

    [[nodiscard]] std::string describe() const override {
        return "is infinite";
    }
};

template <typename T = double>
auto IsInf() {
    return IsInfMatcher<T>();
}

template <typename T>
class IsFiniteMatcher : public Matcher<T> {
public:
    [[nodiscard]] bool matches(const T& value) const override {
        return std::isfinite(value);
    }

    [[nodiscard]] std::string describe() const override { return "is finite"; }
};

template <typename T = double>
auto IsFinite() {
    return IsFiniteMatcher<T>();
}

template <typename T>
class IsNullMatcher : public Matcher<T> {
public:
    [[nodiscard]] bool matches(const T& value) const override {
        return value == nullptr;
    }

    [[nodiscard]] std::string describe() const override { return "is null"; }
};

template <typename T = void*>
auto IsNull() {
    return IsNullMatcher<T>();
}

template <typename T>
class NotNullMatcher : public Matcher<T> {
public:
    [[nodiscard]] bool matches(const T& value) const override {
        return value != nullptr;
    }

    [[nodiscard]] std::string describe() const override {
        return "is not null";
    }
};

template <typename T = void*>
auto NotNull() {
    return NotNullMatcher<T>();
}

template <typename InnerMatcher>
class PointeeMatcher {
public:
    explicit PointeeMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename Ptr>
    [[nodiscard]] bool matches(const Ptr& ptr) const {
        return ptr != nullptr && inner_.matches(*ptr);
    }

    [[nodiscard]] std::string describe() const {
        return "points to a value that " + inner_.describe();
    }

private:
    InnerMatcher inner_;
};

template <typename InnerMatcher>
auto Pointee(InnerMatcher inner) {
    return PointeeMatcher<InnerMatcher>(std::move(inner));
}

class StrEqMatcher : public Matcher<std::string> {
public:
    explicit StrEqMatcher(std::string expected)
        : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return value == expected_;
    }

    [[nodiscard]] std::string describe() const override {
        return "equals \"" + expected_ + "\"";
    }

private:
    std::string expected_;
};

inline auto StrEq(std::string expected) {
    return StrEqMatcher(std::move(expected));
}

class StrNeMatcher : public Matcher<std::string> {
public:
    explicit StrNeMatcher(std::string expected)
        : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return value != expected_;
    }

    [[nodiscard]] std::string describe() const override {
        return "does not equal \"" + expected_ + "\"";
    }

private:
    std::string expected_;
};

inline auto StrNe(std::string expected) {
    return StrNeMatcher(std::move(expected));
}

class StrCaseEqMatcher : public Matcher<std::string> {
public:
    explicit StrCaseEqMatcher(std::string expected)
        : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        if (value.size() != expected_.size())
            return false;
        for (size_t i = 0; i < value.size(); ++i) {
            if (static_cast<char>(
                    std::tolower(static_cast<unsigned char>(value[i]))) !=
                static_cast<char>(
                    std::tolower(static_cast<unsigned char>(expected_[i])))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string describe() const override {
        return "equals (ignoring case) \"" + expected_ + "\"";
    }

private:
    std::string expected_;
};

inline auto StrCaseEq(std::string expected) {
    return StrCaseEqMatcher(std::move(expected));
}

class HasSubstrMatcher : public Matcher<std::string> {
public:
    explicit HasSubstrMatcher(std::string substr)
        : substr_(std::move(substr)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return value.find(substr_) != std::string::npos;
    }

    [[nodiscard]] std::string describe() const override {
        return "contains \"" + substr_ + "\"";
    }

private:
    std::string substr_;
};

inline auto HasSubstr(std::string substr) {
    return HasSubstrMatcher(std::move(substr));
}

class StartsWithMatcher : public Matcher<std::string> {
public:
    explicit StartsWithMatcher(std::string prefix)
        : prefix_(std::move(prefix)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return value.size() >= prefix_.size() &&
               value.compare(0, prefix_.size(), prefix_) == 0;
    }

    [[nodiscard]] std::string describe() const override {
        return "starts with \"" + prefix_ + "\"";
    }

private:
    std::string prefix_;
};

inline auto StartsWith(std::string prefix) {
    return StartsWithMatcher(std::move(prefix));
}

class EndsWithMatcher : public Matcher<std::string> {
public:
    explicit EndsWithMatcher(std::string suffix) : suffix_(std::move(suffix)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return value.size() >= suffix_.size() &&
               value.compare(value.size() - suffix_.size(), suffix_.size(),
                             suffix_) == 0;
    }

    [[nodiscard]] std::string describe() const override {
        return "ends with \"" + suffix_ + "\"";
    }

private:
    std::string suffix_;
};

inline auto EndsWith(std::string suffix) {
    return EndsWithMatcher(std::move(suffix));
}

class MatchesRegexMatcher : public Matcher<std::string> {
public:
    explicit MatchesRegexMatcher(std::string pattern)
        : pattern_(std::move(pattern)), regex_(pattern_) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return std::regex_search(value, regex_);
    }

    [[nodiscard]] std::string describe() const override {
        return "matches regex \"" + pattern_ + "\"";
    }

private:
    std::string pattern_;
    std::regex regex_;
};

inline auto MatchesRegex(std::string pattern) {
    return MatchesRegexMatcher(std::move(pattern));
}

class ContainsRegexMatcher : public Matcher<std::string> {
public:
    explicit ContainsRegexMatcher(std::string pattern)
        : pattern_(std::move(pattern)), regex_(pattern_) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        return std::regex_match(value, regex_);
    }

    [[nodiscard]] std::string describe() const override {
        return "fully matches regex \"" + pattern_ + "\"";
    }

private:
    std::string pattern_;
    std::regex regex_;
};

inline auto ContainsRegex(std::string pattern) {
    return ContainsRegexMatcher(std::move(pattern));
}

template <typename Container>
class IsEmptyMatcher : public Matcher<Container> {
public:
    [[nodiscard]] bool matches(const Container& value) const override {
        return value.empty();
    }

    [[nodiscard]] std::string describe() const override { return "is empty"; }
};

template <typename Container = std::vector<int>>
auto IsEmpty() {
    return IsEmptyMatcher<Container>();
}

template <typename Container>
class SizeIsMatcher : public Matcher<Container> {
public:
    explicit SizeIsMatcher(size_t expected) : expected_(expected) {}

    [[nodiscard]] bool matches(const Container& value) const override {
        return value.size() == expected_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "has size " << expected_;
        return oss.str();
    }

private:
    size_t expected_;
};

template <typename Container = std::vector<int>>
auto SizeIs(size_t expected) {
    return SizeIsMatcher<Container>(expected);
}

template <typename InnerMatcher>
class EachMatcher {
public:
    explicit EachMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        return std::all_of(
            value.begin(), value.end(),
            [this](const auto& elem) { return inner_.matches(elem); });
    }

    [[nodiscard]] std::string describe() const {
        return "each element " + inner_.describe();
    }

private:
    InnerMatcher inner_;
};

template <typename InnerMatcher>
auto Each(InnerMatcher inner) {
    return EachMatcher<InnerMatcher>(std::move(inner));
}

template <typename... Matchers>
class ElementsAreMatcher {
public:
    explicit ElementsAreMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        if (value.size() != sizeof...(Matchers))
            return false;
        return matchImpl(value, std::index_sequence_for<Matchers...>{});
    }

    [[nodiscard]] std::string describe() const {
        return "has exactly " + std::to_string(sizeof...(Matchers)) +
               " elements matching in order";
    }

private:
    template <typename Container, size_t... Is>
    bool matchImpl(const Container& value, std::index_sequence<Is...>) const {
        auto it = value.begin();
        return (... && std::get<Is>(matchers_).matches(*std::next(it, Is)));
    }

    std::tuple<Matchers...> matchers_;
};

template <typename M>
auto asMatcher(M m) {
    if constexpr (requires(const M& mm, const int& v) {
                      { mm.matches(v) } -> std::convertible_to<bool>;
                      { mm.describe() } -> std::convertible_to<std::string>;
                  }) {
        return m;
    } else {
        return Eq(std::move(m));
    }
}

template <typename... Ms>
auto ElementsAre(Ms... ms) {
    return ElementsAreMatcher<decltype(asMatcher(ms))...>(
        asMatcher(std::move(ms))...);
}

template <typename... Matchers>
class UnorderedElementsAreMatcher {
public:
    explicit UnorderedElementsAreMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        if (value.size() != sizeof...(Matchers))
            return false;
        std::vector<bool> matched(value.size(), false);
        return matchImpl(value, matched,
                         std::index_sequence_for<Matchers...>{});
    }

    [[nodiscard]] std::string describe() const {
        return "has exactly " + std::to_string(sizeof...(Matchers)) +
               " elements (in any order)";
    }

private:
    template <typename Container, size_t... Is>
    bool matchImpl(const Container& value, std::vector<bool>& matched,
                   std::index_sequence<Is...>) const {
        return (... && matchSingle(value, matched, std::get<Is>(matchers_)));
    }

    template <typename Container, typename M>
    bool matchSingle(const Container& value, std::vector<bool>& matched,
                     const M& matcher) const {
        size_t idx = 0;
        for (const auto& elem : value) {
            if (!matched[idx] && matcher.matches(elem)) {
                matched[idx] = true;
                return true;
            }
            ++idx;
        }
        return false;
    }

    std::tuple<Matchers...> matchers_;
};

template <typename... Ms>
auto UnorderedElementsAre(Ms... ms) {
    return UnorderedElementsAreMatcher<decltype(asMatcher(ms))...>(
        asMatcher(std::move(ms))...);
}

template <typename InnerMatcher>
class NotMatcher {
public:
    explicit NotMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename T>
    [[nodiscard]] bool matches(const T& value) const {
        return !inner_.matches(value);
    }

    [[nodiscard]] std::string describe() const {
        return "not (" + inner_.describe() + ")";
    }

private:
    InnerMatcher inner_;
};

template <typename InnerMatcher>
auto Not(InnerMatcher inner) {
    return NotMatcher<InnerMatcher>(std::move(inner));
}

template <typename... Matchers>
class AllOfMatcher {
public:
    explicit AllOfMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename T>
    [[nodiscard]] bool matches(const T& value) const {
        return std::apply(
            [&value](const auto&... m) { return (... && m.matches(value)); },
            matchers_);
    }

    [[nodiscard]] std::string describe() const { return "all conditions"; }

private:
    std::tuple<Matchers...> matchers_;
};

template <typename... Ms>
auto AllOf(Ms... ms) {
    return AllOfMatcher<decltype(asMatcher(ms))...>(
        asMatcher(std::move(ms))...);
}

template <typename... Matchers>
class AnyOfMatcher {
public:
    explicit AnyOfMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename T>
    [[nodiscard]] bool matches(const T& value) const {
        return std::apply(
            [&value](const auto&... m) { return (... || m.matches(value)); },
            matchers_);
    }

    [[nodiscard]] std::string describe() const { return "any condition"; }

private:
    std::tuple<Matchers...> matchers_;
};

template <typename... Ms>
auto AnyOf(Ms... ms) {
    return AnyOfMatcher<decltype(asMatcher(ms))...>(
        asMatcher(std::move(ms))...);
}

template <typename T = void>
class AnyMatcher {
public:
    template <typename U>
    [[nodiscard]] bool matches(const U&) const {
        return true;
    }

    [[nodiscard]] std::string describe() const { return "is anything"; }
};

inline constexpr struct {
    template <typename T>
    bool matches(const T&) const {
        return true;
    }

    std::string describe() const { return "is anything"; }
} _{};

template <typename InnerMatcher>
class HasValueMatcher {
public:
    explicit HasValueMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename Opt>
    [[nodiscard]] bool matches(const Opt& opt) const {
        return opt.has_value() && inner_.matches(*opt);
    }

    [[nodiscard]] std::string describe() const {
        return "has value that " + inner_.describe();
    }

private:
    InnerMatcher inner_;
};

template <typename InnerMatcher>
auto HasValue(InnerMatcher inner) {
    return HasValueMatcher<InnerMatcher>(std::move(inner));
}

template <typename InnerMatcher>
auto Optional(InnerMatcher inner) {
    return HasValue(std::move(inner));
}

// ============================================================================
// Container Matchers
// ============================================================================

/**
 * @brief Contains element matcher
 */
template <typename Container, typename Element>
class ContainsMatcher : public Matcher<Container> {
public:
    explicit ContainsMatcher(Element element) : element_(std::move(element)) {}

    [[nodiscard]] bool matches(const Container& value) const override {
        return std::find(value.begin(), value.end(), element_) != value.end();
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "contains ";
        if constexpr (StreamInsertable<Element>) {
            oss << element_;
        } else {
            oss << "<unprintable>";
        }
        return oss.str();
    }

private:
    Element element_;
};

template <typename Element>
struct ContainsValueMatcher {
    Element element;

    template <typename Container>
    [[nodiscard]] bool matches(const Container& c) const {
        return std::find(c.begin(), c.end(), element) != c.end();
    }

    [[nodiscard]] std::string describe() const { return "contains element"; }
};

template <typename Element>
auto Contains(Element element) {
    return ContainsValueMatcher<Element>{std::move(element)};
}

template <typename Container>
struct ContainerEqMatcher {
    Container expected;

    [[nodiscard]] bool matches(const Container& value) const {
        return value == expected;
    }

    [[nodiscard]] std::string describe() const { return "is equal"; }
};

template <typename Container>
auto ContainerEq(Container expected) {
    return ContainerEqMatcher<Container>{std::move(expected)};
}

// ============================================================================
// Logical Matchers
// ============================================================================

/**
 * @brief Assert with matcher
 */
template <typename T, typename MatcherType>
auto expectThatMatcher(const T& value, const MatcherType& matcher,
                       const char* file, int line,
                       const char* valueExpr) -> Expect {
    bool result = matcher.matches(value);
    std::ostringstream oss;
    if (!result) {
        oss << "Value of: " << valueExpr << "\n"
            << "  Expected: " << matcher.describe() << "\n";
        oss << "  Actual: ";
        if constexpr (StreamInsertable<T>) {
            oss << value;
        } else {
            oss << "<unprintable>";
        }
    }
    return Expect(result, file, line, oss.str());
}

}  // namespace matchers

template <typename T, typename MatcherType>
auto expectThatMatcher(const T& value, const MatcherType& matcher,
                       const char* file, int line,
                       const char* valueExpr) -> Expect {
    return matchers::expectThatMatcher(value, matcher, file, line, valueExpr);
}

}  // namespace atom::test

// Macros
#define EXPECT_THAT(value, matcher) \
    atom::test::expectThatMatcher(value, matcher, __FILE__, __LINE__, #value)

#define ASSERT_THAT(value, matcher) \
    atom::test::expectThatMatcher(value, matcher, __FILE__, __LINE__, #value)

// Import matchers into atom::test namespace
namespace atom::test {
using namespace matchers;
}

#endif  // ATOM_TEST_ASSERTIONS_MATCHERS_HPP
