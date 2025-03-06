#ifndef ATOM_UTILS_VALID_STRING_HPP
#define ATOM_UTILS_VALID_STRING_HPP

#include <array>
#include <concepts>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "atom/macro.hpp"

namespace atom::utils {

// Basic concepts for string validation
template <typename T>
concept StringLike = requires(T s) {
    { std::data(s) } -> std::convertible_to<const char*>;
    { std::size(s) } -> std::convertible_to<std::size_t>;
};

struct BracketInfo {
    char character;
    int position;

    // Default constructor and equality operator
    constexpr BracketInfo() noexcept : character{}, position{} {}
    constexpr BracketInfo(char c, int pos) noexcept
        : character{c}, position{pos} {}

    constexpr bool operator==(const BracketInfo& other) const noexcept {
        return character == other.character && position == other.position;
    }
} ATOM_ALIGNAS(8);

struct ValidationResult {
    bool isValid;
    std::vector<BracketInfo> invalidBrackets;
    std::vector<std::string> errorMessages;

    ValidationResult() noexcept : isValid{true} {}

    // Add methods for better encapsulation
    void addError(const BracketInfo& info, std::string message) noexcept {
        isValid = false;
        invalidBrackets.push_back(info);
        errorMessages.push_back(std::move(message));
    }

    void addError(std::string message) noexcept {
        isValid = false;
        errorMessages.push_back(std::move(message));
    }
} ATOM_ALIGNAS(64);

// Exception class for validation errors
class ValidationException : public std::runtime_error {
public:
    explicit ValidationException(const std::string& message)
        : std::runtime_error(message), result_{} {
        result_.isValid = false;
        result_.errorMessages.push_back(message);
    }

    explicit ValidationException(ValidationResult result)
        : std::runtime_error(!result.errorMessages.empty()
                                 ? result.errorMessages[0]
                                 : "Validation error"),
          result_(std::move(result)) {}

    [[nodiscard]] const ValidationResult& getResult() const noexcept {
        return result_;
    }

private:
    ValidationResult result_;
};

// Function declarations with noexcept specifications and constexpr where
// possible
auto isValidBracket(StringLike auto&& str) -> ValidationResult;

template <StringLike T>
auto validateBracketsWithExceptions(T&& str) -> void {
    auto result = isValidBracket(std::forward<T>(str));
    if (!result.isValid) {
        throw ValidationException(std::move(result));
    }
}

// Compile-time bracket validator
template <std::size_t N>
struct BracketValidator {
    struct ValidationResult {
    private:
        bool isValid_ = true;
        std::array<int, N> errorPositions_{};
        int errorCount_ = 0;

    public:
        constexpr void addError(int position) noexcept {
            if (errorCount_ < static_cast<int>(N)) {
                errorPositions_[errorCount_++] = position;
                isValid_ = false;
            }
        }

        [[nodiscard]] constexpr auto isValid() const noexcept -> bool {
            return isValid_;
        }

        [[nodiscard]] constexpr auto getErrorPositions() const noexcept
            -> std::span<const int> {
            return std::span<const int>{errorPositions_.data(),
                                        static_cast<size_t>(errorCount_)};
        }

        [[nodiscard]] constexpr auto getErrorCount() const noexcept -> int {
            return errorCount_;
        }
    };

    constexpr static auto validate(std::span<const char, N> str) noexcept
        -> ValidationResult {
        ValidationResult result;
        std::array<char, N> stack{};
        int stackSize = 0;
        bool singleQuoteOpen = false;
        bool doubleQuoteOpen = false;

        for (std::size_t i = 0; i < str.size(); ++i) {
            char current = str[i];

            if (current == '\0') {
                break;
            }

            auto isEscaped = [&str, i]() constexpr -> bool {
                int backslashCount = 0;
                std::size_t idx = i;
                while (idx > 0 && str[--idx] == '\\') {
                    ++backslashCount;
                }
                return (backslashCount % 2) == 1;
            }();

            if (current == '\'' && !doubleQuoteOpen && !isEscaped) {
                singleQuoteOpen = !singleQuoteOpen;
                continue;
            }

            if (current == '\"' && !singleQuoteOpen && !isEscaped) {
                doubleQuoteOpen = !doubleQuoteOpen;
                continue;
            }

            if (singleQuoteOpen || doubleQuoteOpen) {
                continue;
            }

            if (current == '(' || current == '{' || current == '[' ||
                current == '<') {
                stack[stackSize++] = current;
            } else if (current == ')' || current == '}' || current == ']' ||
                       current == '>') {
                if (stackSize == 0 ||
                    !isMatching(stack[stackSize - 1], current)) {
                    result.addError(static_cast<int>(i));
                } else {
                    --stackSize;
                }
            }
        }

        while (stackSize > 0) {
            result.addError(static_cast<int>(N - 1));
            --stackSize;
        }

        if (singleQuoteOpen) {
            result.addError(static_cast<int>(N - 1));
        }

        if (doubleQuoteOpen) {
            result.addError(static_cast<int>(N - 1));
        }

        return result;
    }

private:
    constexpr static auto isMatching(char open, char close) noexcept -> bool {
        return (open == '(' && close == ')') || (open == '{' && close == '}') ||
               (open == '[' && close == ']') || (open == '<' && close == '>');
    }
};

template <std::size_t N>
constexpr auto toArray(const char (&str)[N]) noexcept -> std::array<char, N> {
    std::array<char, N> arr{};
    for (std::size_t i = 0; i < N; ++i) {
        arr[i] = str[i];
    }
    return arr;
}

template <std::size_t N>
constexpr auto validateBrackets(const char (&str)[N]) noexcept ->
    typename BracketValidator<N>::ValidationResult {
    return BracketValidator<N>::validate(std::span<const char, N>{str, N});
}

// Helper function to validate any string-like object at compile time if
// possible
template <StringLike T>
auto validateString(T&& str) {
    if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                  std::is_same_v<
                      std::remove_all_extents_t<std::remove_reference_t<T>>,
                      char>) {
        constexpr auto size = std::extent_v<std::remove_reference_t<T>>;
        return validateBrackets(str);
    } else {
        return isValidBracket(std::forward<T>(str));
    }
}

}  // namespace atom::utils

#endif
