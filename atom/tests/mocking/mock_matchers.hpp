/**
 * @file mock_matchers.hpp
 * @brief Argument matchers for mock function verification
 * @details Provides flexible matchers for mock argument verification
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_MOCKING_MOCK_MATCHERS_HPP
#define ATOM_TEST_MOCKING_MOCK_MATCHERS_HPP

#include <functional>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace atom::test {
namespace mock_matchers {

/**
 * @brief Base interface for argument matchers
 */
template <typename T>
class ArgMatcher {
public:
    virtual ~ArgMatcher() = default;

    [[nodiscard]] virtual bool matches(const T& value) const = 0;
    [[nodiscard]] virtual std::string describe() const = 0;
};

/**
 * @brief Type-erased matcher wrapper
 */
template <typename T>
class MatcherWrapper {
public:
    MatcherWrapper() = default;

    template <typename M>
    MatcherWrapper(M matcher) : impl_(std::make_shared<Model<M>>(std::move(matcher))) {}

    [[nodiscard]] bool matches(const T& value) const {
        return impl_ ? impl_->matches(value) : true;
    }

    [[nodiscard]] std::string describe() const {
        return impl_ ? impl_->describe() : "anything";
    }

    [[nodiscard]] bool hasImpl() const { return impl_ != nullptr; }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual bool matches(const T& value) const = 0;
        virtual std::string describe() const = 0;
    };

    template <typename M>
    struct Model : Concept {
        explicit Model(M m) : matcher(std::move(m)) {}

        bool matches(const T& value) const override {
            return matcher.matches(value);
        }

        std::string describe() const override {
            return matcher.describe();
        }

        M matcher;
    };

    std::shared_ptr<Concept> impl_;
};

// ============================================================================
// Basic Matchers
// ============================================================================

/**
 * @brief Matches any value (wildcard)
 */
template <typename T>
class AnyMatcher {
public:
    [[nodiscard]] bool matches(const T&) const { return true; }
    [[nodiscard]] std::string describe() const { return "_"; }
};

/**
 * @brief Wildcard constant - matches any argument
 */
inline constexpr struct AnyArg {
    template <typename T>
    operator MatcherWrapper<T>() const {
        return MatcherWrapper<T>(AnyMatcher<T>{});
    }
} _ {};

template <typename T = void>
auto Any() {
    return AnyMatcher<T>{};
}

/**
 * @brief Equality matcher
 */
template <typename T>
class EqMatcher {
public:
    explicit EqMatcher(T expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const T& value) const {
        return value == expected_;
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Eq(" << expected_ << ")";
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
class NeMatcher {
public:
    explicit NeMatcher(T expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const T& value) const {
        return value != expected_;
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Ne(" << expected_ << ")";
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
class LtMatcher {
public:
    explicit LtMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const { return value < bound_; }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Lt(" << bound_ << ")";
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
 * @brief Less than or equal matcher
 */
template <typename T>
class LeMatcher {
public:
    explicit LeMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const { return value <= bound_; }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Le(" << bound_ << ")";
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
 * @brief Greater than matcher
 */
template <typename T>
class GtMatcher {
public:
    explicit GtMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const { return value > bound_; }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Gt(" << bound_ << ")";
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
 * @brief Greater than or equal matcher
 */
template <typename T>
class GeMatcher {
public:
    explicit GeMatcher(T bound) : bound_(std::move(bound)) {}

    [[nodiscard]] bool matches(const T& value) const { return value >= bound_; }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Ge(" << bound_ << ")";
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
// String Matchers
// ============================================================================

/**
 * @brief String equality matcher
 */
class StrEqMatcher {
public:
    explicit StrEqMatcher(std::string expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value == expected_;
    }

    [[nodiscard]] std::string describe() const {
        return "StrEq(\"" + expected_ + "\")";
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
class StrNeMatcher {
public:
    explicit StrNeMatcher(std::string expected) : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value != expected_;
    }

    [[nodiscard]] std::string describe() const {
        return "StrNe(\"" + expected_ + "\")";
    }

private:
    std::string expected_;
};

inline auto StrNe(std::string expected) {
    return StrNeMatcher(std::move(expected));
}

/**
 * @brief Case-insensitive string equality
 */
class StrCaseEqMatcher {
public:
    explicit StrCaseEqMatcher(std::string expected)
        : expected_(std::move(expected)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        if (value.size() != expected_.size()) return false;
        for (size_t i = 0; i < value.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(value[i])) !=
                std::tolower(static_cast<unsigned char>(expected_[i]))) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string describe() const {
        return "StrCaseEq(\"" + expected_ + "\")";
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
class HasSubstrMatcher {
public:
    explicit HasSubstrMatcher(std::string substr) : substr_(std::move(substr)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value.find(substr_) != std::string::npos;
    }

    [[nodiscard]] std::string describe() const {
        return "HasSubstr(\"" + substr_ + "\")";
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
class StartsWithMatcher {
public:
    explicit StartsWithMatcher(std::string prefix) : prefix_(std::move(prefix)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value.size() >= prefix_.size() &&
               value.compare(0, prefix_.size(), prefix_) == 0;
    }

    [[nodiscard]] std::string describe() const {
        return "StartsWith(\"" + prefix_ + "\")";
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
class EndsWithMatcher {
public:
    explicit EndsWithMatcher(std::string suffix) : suffix_(std::move(suffix)) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return value.size() >= suffix_.size() &&
               value.compare(value.size() - suffix_.size(), suffix_.size(),
                             suffix_) == 0;
    }

    [[nodiscard]] std::string describe() const {
        return "EndsWith(\"" + suffix_ + "\")";
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
class MatchesRegexMatcher {
public:
    explicit MatchesRegexMatcher(std::string pattern)
        : pattern_(std::move(pattern)), regex_(pattern_) {}

    [[nodiscard]] bool matches(const std::string& value) const {
        return std::regex_search(value, regex_);
    }

    [[nodiscard]] std::string describe() const {
        return "MatchesRegex(\"" + pattern_ + "\")";
    }

private:
    std::string pattern_;
    std::regex regex_;
};

inline auto MatchesRegex(std::string pattern) {
    return MatchesRegexMatcher(std::move(pattern));
}

// ============================================================================
// Pointer Matchers
// ============================================================================

/**
 * @brief Null pointer matcher
 */
template <typename T>
class IsNullMatcher {
public:
    [[nodiscard]] bool matches(const T& value) const { return value == nullptr; }
    [[nodiscard]] std::string describe() const { return "IsNull()"; }
};

template <typename T = void*>
auto IsNull() {
    return IsNullMatcher<T>{};
}

/**
 * @brief Not null matcher
 */
template <typename T>
class NotNullMatcher {
public:
    [[nodiscard]] bool matches(const T& value) const { return value != nullptr; }
    [[nodiscard]] std::string describe() const { return "NotNull()"; }
};

template <typename T = void*>
auto NotNull() {
    return NotNullMatcher<T>{};
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
        return "Pointee(" + inner_.describe() + ")";
    }

private:
    InnerMatcher inner_;
};

template <typename InnerMatcher>
auto Pointee(InnerMatcher inner) {
    return PointeeMatcher<InnerMatcher>(std::move(inner));
}

// ============================================================================
// Container Matchers
// ============================================================================

/**
 * @brief Size matcher
 */
template <typename Container>
class SizeIsMatcher {
public:
    explicit SizeIsMatcher(size_t expected) : expected_(expected) {}

    [[nodiscard]] bool matches(const Container& value) const {
        return value.size() == expected_;
    }

    [[nodiscard]] std::string describe() const {
        return "SizeIs(" + std::to_string(expected_) + ")";
    }

private:
    size_t expected_;
};

template <typename Container = std::vector<int>>
auto SizeIs(size_t expected) {
    return SizeIsMatcher<Container>(expected);
}

/**
 * @brief Empty container matcher
 */
template <typename Container>
class IsEmptyMatcher {
public:
    [[nodiscard]] bool matches(const Container& value) const {
        return value.empty();
    }

    [[nodiscard]] std::string describe() const { return "IsEmpty()"; }
};

template <typename Container = std::vector<int>>
auto IsEmpty() {
    return IsEmptyMatcher<Container>{};
}

/**
 * @brief Contains element matcher
 */
template <typename Container, typename Element>
class ContainsMatcher {
public:
    explicit ContainsMatcher(Element element) : element_(std::move(element)) {}

    [[nodiscard]] bool matches(const Container& value) const {
        return std::find(value.begin(), value.end(), element_) != value.end();
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "Contains(" << element_ << ")";
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
        return "Not(" + inner_.describe() + ")";
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
        oss << "AllOf(";
        describeImpl(oss, std::index_sequence_for<Matchers...>{});
        oss << ")";
        return oss.str();
    }

private:
    template <size_t... Is>
    void describeImpl(std::ostringstream& oss,
                      std::index_sequence<Is...>) const {
        ((oss << (Is > 0 ? ", " : "") << std::get<Is>(matchers_).describe()),
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
        oss << "AnyOf(";
        describeImpl(oss, std::index_sequence_for<Matchers...>{});
        oss << ")";
        return oss.str();
    }

private:
    template <size_t... Is>
    void describeImpl(std::ostringstream& oss,
                      std::index_sequence<Is...>) const {
        ((oss << (Is > 0 ? ", " : "") << std::get<Is>(matchers_).describe()),
         ...);
    }

    std::tuple<Matchers...> matchers_;
};

template <typename... Matchers>
auto AnyOf(Matchers... matchers) {
    return AnyOfMatcher<Matchers...>(std::move(matchers)...);
}

// ============================================================================
// Floating Point Matchers
// ============================================================================

/**
 * @brief Approximately equal matcher
 */
template <typename T>
class FloatNearMatcher {
public:
    FloatNearMatcher(T expected, T tolerance)
        : expected_(expected), tolerance_(tolerance) {}

    [[nodiscard]] bool matches(const T& value) const {
        return std::abs(value - expected_) <= tolerance_;
    }

    [[nodiscard]] std::string describe() const {
        std::ostringstream oss;
        oss << "FloatNear(" << expected_ << ", " << tolerance_ << ")";
        return oss.str();
    }

private:
    T expected_;
    T tolerance_;
};

template <typename T>
auto FloatNear(T expected, T tolerance) {
    return FloatNearMatcher<T>(expected, tolerance);
}

template <typename T>
auto DoubleNear(T expected, T tolerance) {
    return FloatNearMatcher<T>(expected, tolerance);
}

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
        return "Field(" + inner_.describe() + ")";
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
        return "Property(" + inner_.describe() + ")";
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

// ============================================================================
// Custom Predicate Matcher
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
        return description_.empty() ? "Truly(predicate)" : description_;
    }

private:
    Predicate pred_;
    std::string description_;
};

template <typename Predicate>
auto Truly(Predicate pred, std::string description = "") {
    return TrulyMatcher<Predicate>(std::move(pred), std::move(description));
}

// ============================================================================
// Reference Matchers
// ============================================================================

/**
 * @brief Reference equality matcher
 */
template <typename T>
class RefMatcher {
public:
    explicit RefMatcher(const T& ref) : ref_(&ref) {}

    [[nodiscard]] bool matches(const T& value) const { return &value == ref_; }

    [[nodiscard]] std::string describe() const { return "Ref(reference)"; }

private:
    const T* ref_;
};

template <typename T>
auto Ref(const T& ref) {
    return RefMatcher<T>(ref);
}

}  // namespace mock_matchers

// Import into atom::test namespace
using namespace mock_matchers;

}  // namespace atom::test

#endif  // ATOM_TEST_MOCKING_MOCK_MATCHERS_HPP
