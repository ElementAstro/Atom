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
#include <cmath>
#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/tests/core/test.hpp"

namespace atom::test {
namespace matchers {

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
        oss << "was: " << value;
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
        oss << "is equal to " << expected_;
        return oss.str();
    }

private:
    T expected_;
};

template <typename T>
auto Eq(T expected) {
    return EqMatcher<T>(std::move(expected));
}

/**
 * @brief Not equal matcher
 */
template <typename T>
class NeMatcher : public Matcher<T> {
public:
    explicit NeMatcher(T expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const T& value) const override {
        return value != expected_;
    }

    [[nodiscard]] std::string describe() const override {
        std::ostringstream oss;
        oss << "is not equal to " << expected_;
        return oss.str();
    }

private:
    T expected_;
};

template <typename T>
auto Ne(T expected) {
    return NeMatcher<T>(std::move(expected));
}

/**
 * @brief Less than matcher
 */
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

/**
 * @brief Greater than matcher
 */
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

/**
 * @brief Less than or equal matcher
 */
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

/**
 * @brief Greater than or equal matcher
 */
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
        oss << "was " << value << " (difference: "
            << std::abs(value - expected_) << ")";
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

/**
 * @brief NaN matcher
 */
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

/**
 * @brief Infinity matcher
 */
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

/**
 * @brief Finite number matcher
 */
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

// ============================================================================
// Pointer Matchers
// ============================================================================

/**
 * @brief Null pointer matcher
 */
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

/**
 * @brief Not null matcher
 */
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

/**
 * @brief Pointee matcher - matches the value pointed to
 */
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

// ============================================================================
// String Matchers
// ============================================================================

/**
 * @brief String equality matcher (case-sensitive)
 */
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

/**
 * @brief String inequality matcher
 */
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

/**
 * @brief Case-insensitive string equality matcher
 */
class StrCaseEqMatcher : public Matcher<std::string> {
public:
    explicit StrCaseEqMatcher(std::string expected)
        : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const override {
        if (value.size() != expected_.size()) return false;
        for (size_t i = 0; i < value.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(value[i])) !=
                std::tolower(static_cast<unsigned char>(expected_[i]))) {
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

/**
 * @brief Substring matcher
 */
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

/**
 * @brief Starts with matcher
 */
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

/**
 * @brief Ends with matcher
 */
class EndsWithMatcher : public Matcher<std::string> {
public:
    explicit EndsWithMatcher(std::string suffix)
        : suffix_(std::move(suffix)) {}

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

/**
 * @brief Regex matcher
 */
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

/**
 * @brief Full regex match matcher
 */
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

// ============================================================================
// Container Matchers
// ============================================================================

/**
 * @brief Empty container matcher
 */
template <typename Container>
class IsEmptyMatcher : public Matcher<Container> {
public:
    [[nodiscard]] bool matches(const Container& value) const override {
        return value.empty();
    }

    [[nodiscard]] std::string describe() const override { return "is empty"; }

    [[nodiscard]] std::string describeMismatch(
        const Container& value) const override {
        std::ostringstream oss;
        oss << "has size " << value.size();
        return oss.str();
    }
};

template <typename Container = std::vector<int>>
auto IsEmpty() {
    return IsEmptyMatcher<Container>();
}

/**
 * @brief Size matcher
 */
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

    [[nodiscard]] std::string describeMismatch(
        const Container& value) const override {
        std::ostringstream oss;
        oss << "has size " << value.size();
        return oss.str();
    }

private:
    size_t expected_;
};

template <typename Container = std::vector<int>>
auto SizeIs(size_t expected) {
    return SizeIsMatcher<Container>(expected);
}

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
        oss << "contains " << element_;
        return oss.str();
    }

private:
    Element element_;
};

template <typename Element>
auto Contains(Element element) {
    return [element = std::move(element)]<typename Container>(
               const Container& c) {
        return std::find(c.begin(), c.end(), element) != c.end();
    };
}

/**
 * @brief Each element matches matcher
 */
template <typename InnerMatcher>
class EachMatcher {
public:
    explicit EachMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        return std::all_of(value.begin(), value.end(),
                           [this](const auto& elem) {
                               return inner_.matches(elem);
                           });
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

/**
 * @brief Elements are matcher (exact match in order)
 */
template <typename... Matchers>
class ElementsAreMatcher {
public:
    explicit ElementsAreMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        if (value.size() != sizeof...(Matchers)) return false;
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

template <typename... Matchers>
auto ElementsAre(Matchers... matchers) {
    return ElementsAreMatcher<Matchers...>(std::move(matchers)...);
}

/**
 * @brief Unordered elements matcher
 */
template <typename... Matchers>
class UnorderedElementsAreMatcher {
public:
    explicit UnorderedElementsAreMatcher(Matchers... matchers)
        : matchers_(std::move(matchers)...) {}

    template <typename Container>
    [[nodiscard]] bool matches(const Container& value) const {
        if (value.size() != sizeof...(Matchers)) return false;
        std::vector<bool> matched(value.size(), false);
        return matchImpl(value, matched, std::index_sequence_for<Matchers...>{});
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

template <typename... Matchers>
auto UnorderedElementsAre(Matchers... matchers) {
    return UnorderedElementsAreMatcher<Matchers...>(std::move(matchers)...);
}

// ============================================================================
// Logical Matchers
// ============================================================================

/**
 * @brief Not matcher (negation)
 */
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

/**
 * @brief AllOf matcher (conjunction)
 */
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

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "(";
        describeImpl(oss, std::index_sequence_for<Matchers...>{});
        oss << ")";
        return oss.str();
    }

private:
    template <size_t... Is>
    void describeImpl(std::ostringstream& oss,
                      std::index_sequence<Is...>) const {
        ((oss << (Is > 0 ? " and " : "") << std::get<Is>(matchers_).describe()),
         ...);
    }

    std::tuple<Matchers...> matchers_;
};

template <typename... Matchers>
auto AllOf(Matchers... matchers) {
    return AllOfMatcher<Matchers...>(std::move(matchers)...);
}

/**
 * @brief AnyOf matcher (disjunction)
 */
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

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "(";
        describeImpl(oss, std::index_sequence_for<Matchers...>{});
        oss << ")";
        return oss.str();
    }

private:
    template <size_t... Is>
    void describeImpl(std::ostringstream& oss,
                      std::index_sequence<Is...>) const {
        ((oss << (Is > 0 ? " or " : "") << std::get<Is>(matchers_).describe()),
         ...);
    }

    std::tuple<Matchers...> matchers_;
};

template <typename... Matchers>
auto AnyOf(Matchers... matchers) {
    return AnyOfMatcher<Matchers...>(std::move(matchers)...);
}

// ============================================================================
// Wildcard Matcher
// ============================================================================

/**
 * @brief Wildcard matcher (matches anything)
 */
template <typename T = void>
class AnyMatcher {
public:
    template <typename U>
    [[nodiscard]] bool matches(const U&) const {
        return true;
    }

    [[nodiscard]] std::string describe() const { return "is anything"; }
};

/**
 * @brief Underscore wildcard (matches any value)
 */
inline constexpr struct {
    template <typename T>
    bool matches(const T&) const {
        return true;
    }

    std::string describe() const { return "is anything"; }
} _ {};

template <typename T = void>
auto A() {
    return AnyMatcher<T>();
}

template <typename T = void>
auto An() {
    return AnyMatcher<T>();
}

// ============================================================================
// Optional Matchers
// ============================================================================

/**
 * @brief Has value matcher for optional types
 */
template <typename InnerMatcher>
class HasValueMatcher {
public:
    explicit HasValueMatcher(InnerMatcher inner) : inner_(std::move(inner)) {}

    template <typename Optional>
    [[nodiscard]] bool matches(const Optional& opt) const {
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

/**
 * @brief Optional is nullopt matcher
 */
struct IsNulloptMatcher {
    template <typename Optional>
    [[nodiscard]] bool matches(const Optional& opt) const {
        return !opt.has_value();
    }

    [[nodiscard]] std::string describe() const { return "is nullopt"; }
};

inline auto IsNullopt() { return IsNulloptMatcher{}; }

// ============================================================================
// Property/Field Matchers
// ============================================================================

/**
 * @brief Field matcher
 */
template <typename FieldType, typename Class, typename InnerMatcher>
class FieldMatcher {
public:
    FieldMatcher(FieldType Class::*field, InnerMatcher inner)
        : field_(field), inner_(std::move(inner)) {}

    [[nodiscard]] bool matches(const Class& obj) const {
        return inner_.matches(obj.*field_);
    }

    [[nodiscard]] std::string describe() const {
        return "field " + inner_.describe();
    }

private:
    FieldType Class::*field_;
    InnerMatcher inner_;
};

template <typename FieldType, typename Class, typename InnerMatcher>
auto Field(FieldType Class::*field, InnerMatcher inner) {
    return FieldMatcher<FieldType, Class, InnerMatcher>(field, std::move(inner));
}

/**
 * @brief Property matcher (via getter)
 */
template <typename GetterResult, typename Class, typename InnerMatcher>
class PropertyMatcher {
public:
    PropertyMatcher(GetterResult (Class::*getter)() const, InnerMatcher inner)
        : getter_(getter), inner_(std::move(inner)) {}

    [[nodiscard]] bool matches(const Class& obj) const {
        return inner_.matches((obj.*getter_)());
    }

    [[nodiscard]] std::string describe() const {
        return "property " + inner_.describe();
    }

private:
    GetterResult (Class::*getter_)() const;
    InnerMatcher inner_;
};

template <typename GetterResult, typename Class, typename InnerMatcher>
auto Property(GetterResult (Class::*getter)() const, InnerMatcher inner) {
    return PropertyMatcher<GetterResult, Class, InnerMatcher>(getter,
                                                              std::move(inner));
}

/**
 * @brief Result of matcher
 */
template <typename Func, typename InnerMatcher>
class ResultOfMatcher {
public:
    ResultOfMatcher(Func func, InnerMatcher inner)
        : func_(std::move(func)), inner_(std::move(inner)) {}

    template <typename T>
    [[nodiscard]] bool matches(const T& value) const {
        return inner_.matches(func_(value));
    }

    [[nodiscard]] std::string describe() const {
        return "result of function " + inner_.describe();
    }

private:
    Func func_;
    InnerMatcher inner_;
};

template <typename Func, typename InnerMatcher>
auto ResultOf(Func func, InnerMatcher inner) {
    return ResultOfMatcher<Func, InnerMatcher>(std::move(func),
                                               std::move(inner));
}

// ============================================================================
// Truly/Predicate Matcher
// ============================================================================

/**
 * @brief Predicate matcher
 */
template <typename Predicate>
class TrulyMatcher {
public:
    explicit TrulyMatcher(Predicate pred, std::string description = "")
        : pred_(std::move(pred)), description_(std::move(description)) {}

    template <typename T>
    [[nodiscard]] bool matches(const T& value) const {
        return pred_(value);
    }

    [[nodiscard]] std::string describe() const {
        return description_.empty() ? "satisfies predicate" : description_;
    }

private:
    Predicate pred_;
    std::string description_;
};

template <typename Predicate>
auto Truly(Predicate pred, std::string description = "") {
    return TrulyMatcher<Predicate>(std::move(pred), std::move(description));
}

}  // namespace matchers

// ============================================================================
// EXPECT_THAT macro support
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
            << "  Expected: " << matcher.describe() << "\n"
            << "  Actual: " << value;
    }
    return Expect(result, file, line, oss.str());
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
