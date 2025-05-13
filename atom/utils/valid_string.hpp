#ifndef ATOM_UTILS_VALID_STRING_HPP
#define ATOM_UTILS_VALID_STRING_HPP

#include <array>
#include <concepts>
#include <expected>  // C++23 for error handling with result types
#include <format>    // C++20 string formatting
#include <optional>
#include <ranges>           // C++20 ranges library
#include <source_location>  // C++20 source code location
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/macro.hpp"

namespace atom::utils {

// Enhanced string type constraint
template <typename T>
concept CharPointer =
    std::is_pointer_v<std::remove_reference_t<T>> &&
    std::is_same_v<
        std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>,
        char>;

template <typename T>
concept Char8Pointer =
    std::is_pointer_v<std::remove_reference_t<T>> &&
    std::is_same_v<
        std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>,
        char8_t>;

template <typename T>
concept CharStringLike = requires(T s) {
    { std::ranges::data(s) } -> std::convertible_to<const char*>;
    { std::ranges::size(s) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept Char8StringLike = requires(T s) {
    { std::ranges::data(s) } -> std::convertible_to<const char8_t*>;
    { std::ranges::size(s) } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept StringLike = CharPointer<T> || Char8Pointer<T> || CharStringLike<T> ||
                     Char8StringLike<T>;

// Extended version with Unicode support
template <typename T>
concept UnicodeStringLike = StringLike<T> && requires(T s) {
    typename T::value_type;
    requires std::same_as<typename T::value_type, char> ||
                 std::same_as<typename T::value_type, char8_t> ||
                 std::same_as<typename T::value_type, char16_t> ||
                 std::same_as<typename T::value_type, char32_t>;
};

// Bracket type definition enum for extensibility
enum class BracketType : uint8_t {
    Round,   // ()
    Square,  // []
    Curly,   // {}
    Angle,   // <>
    Custom   // Custom bracket types
};

// Upgraded bracket info structure
struct BracketInfo {
    char character;
    int position;
    BracketType type;

    // Default constructor and equality operator
    constexpr BracketInfo() noexcept
        : character{}, position{}, type{BracketType::Round} {}
    constexpr BracketInfo(char c, int pos,
                          BracketType t = BracketType::Custom) noexcept
        : character{c}, position{pos}, type{t} {}

    constexpr bool operator==(const BracketInfo& other) const noexcept {
        return character == other.character && position == other.position &&
               type == other.type;
    }

    constexpr auto operator<=>(const BracketInfo& other) const noexcept =
        default;

    // Add method to get readable bracket type description
    [[nodiscard]] constexpr std::string_view getBracketTypeName()
        const noexcept {
        switch (type) {
            case BracketType::Round:
                return "round";
            case BracketType::Square:
                return "square";
            case BracketType::Curly:
                return "curly";
            case BracketType::Angle:
                return "angle";
            case BracketType::Custom:
                return "custom";
            default:
                return "unknown";
        }
    }
} ATOM_ALIGNAS(8);

// Validation options structure with enhanced configurability
struct ValidationOptions {
    bool validateQuotes{true};
    bool validateBrackets{true};
    bool ignoreEscaped{true};
    bool allowCustomBrackets{false};
    std::vector<std::pair<char, char>> customBracketPairs{};

    constexpr ValidationOptions() noexcept = default;
} ATOM_ALIGNAS(8);

// Redesigned validation result with optional error position storage
struct ValidationResult {
    bool isValid;
    std::vector<BracketInfo> invalidBrackets;
    std::vector<std::string> errorMessages;
    std::optional<std::source_location> sourceLocation;  // Track error source
    // Use std::vformat for runtime format strings
    template <typename... Args>
    void addError(const BracketInfo& info, std::string_view fmt,
                  Args&&... args) {
        isValid = false;
        invalidBrackets.push_back(info);
        std::format_args args_store =
            std::make_format_args(std::forward<Args>(args)...);
        errorMessages.push_back(std::vformat(fmt, args_store));
    }

    template <typename... Args>
    void addError(std::string_view fmt, Args&&... args) {
        isValid = false;
        std::format_args args_store =
            std::make_format_args(std::forward<Args>(args)...);
        errorMessages.push_back(std::vformat(fmt, args_store));
    }

    // Add error with source code location
    template <typename... Args>
    void addErrorWithLocation(const std::source_location& loc,
                              std::string_view fmt, Args&&... args) {
        isValid = false;
        sourceLocation = loc;
        std::format_args args_store =
            std::make_format_args(std::forward<Args>(args)...);
        errorMessages.push_back(std::vformat(fmt, args_store));
    }

    // Merge validation results
    void merge(ValidationResult&& other) {
        if (!other.isValid) {
            isValid = false;
            invalidBrackets.insert(
                invalidBrackets.end(),
                std::make_move_iterator(other.invalidBrackets.begin()),
                std::make_move_iterator(other.invalidBrackets.end()));
            errorMessages.insert(
                errorMessages.end(),
                std::make_move_iterator(other.errorMessages.begin()),
                std::make_move_iterator(other.errorMessages.end()));
        }
    }
} ATOM_ALIGNAS(64);

// Extended exception hierarchy with multiple validation exception types
class ATOM_ALIGNAS(64) ValidationException : public std::runtime_error {
public:
    explicit ValidationException(
        std::string_view message,
        std::source_location loc = std::source_location::current())
        : std::runtime_error(std::string(message)), result_{}, location_(loc) {
        result_.isValid = false;
        result_.errorMessages.push_back(std::string(message));
        result_.sourceLocation = loc;
    }

    explicit ValidationException(
        ValidationResult result,
        std::source_location loc = std::source_location::current())
        : std::runtime_error(!result.errorMessages.empty()
                                 ? result.errorMessages[0]
                                 : "Validation error"),
          result_(std::move(result)),
          location_(loc) {
        if (!result_.sourceLocation) {
            result_.sourceLocation = loc;
        }
    }

    [[nodiscard]] const ValidationResult& getResult() const noexcept {
        return result_;
    }

    [[nodiscard]] const std::source_location& getLocation() const noexcept {
        return location_;
    }

private:
    ValidationResult result_;
    std::source_location location_;
};

// Specific bracket mismatch exception
class BracketMismatchException : public ValidationException {
public:
    explicit BracketMismatchException(
        const BracketInfo& info, std::string_view message,
        std::source_location loc = std::source_location::current())
        : ValidationException(message, loc), bracketInfo_(info) {
        // Add bracket info to result
        ValidationResult& result = const_cast<ValidationResult&>(getResult());
        result.invalidBrackets.push_back(info);
    }

    [[nodiscard]] const BracketInfo& getBracketInfo() const noexcept {
        return bracketInfo_;
    }

private:
    BracketInfo bracketInfo_;
};

// Quote mismatch exception
class QuoteMismatchException : public ValidationException {
public:
    enum class QuoteType { Single, Double };

    explicit QuoteMismatchException(
        QuoteType type,
        std::source_location loc = std::source_location::current())
        : ValidationException(type == QuoteType::Single
                                  ? "Unclosed single quote"
                                  : "Unclosed double quote",
                              loc),
          quoteType_(type) {}

    [[nodiscard]] QuoteType getQuoteType() const noexcept { return quoteType_; }

private:
    QuoteType quoteType_;
};

// Function declaration using std::expected instead of exceptions
template <StringLike T>
auto isValidBracket(T&& str, const ValidationOptions& options = {})
    -> std::expected<ValidationResult, std::string>;

// Extended compile-time validation
template <StringLike T>
auto validateBracketsWithExceptions(T&& str,
                                    const ValidationOptions& options = {})
    -> void {
    auto result = isValidBracket(std::forward<T>(str), options);
    if (!result) {
        throw ValidationException(result.error());
    }
    if (!result->isValid) {
        throw ValidationException(std::move(result.value()));
    }
}

// Enhanced compile-time bracket validator
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

    // Updated validation method with support for more options
    constexpr static auto validate(
        std::span<const char, N> str,
        const ValidationOptions& options = {}) noexcept -> ValidationResult {
        ValidationResult result;
        std::array<char, N> stack{};
        int stackSize = 0;
        bool singleQuoteOpen = false;
        bool doubleQuoteOpen = false;

        constexpr auto isMatching = [](char open,
                                       char close) constexpr noexcept -> bool {
            return (open == '(' && close == ')') ||
                   (open == '{' && close == '}') ||
                   (open == '[' && close == ']') ||
                   (open == '<' && close == '>');
        };

        // Support for custom bracket pairs
        auto isCustomMatching =
            [&options](char open, char close) constexpr noexcept -> bool {
            return std::any_of(options.customBracketPairs.begin(),
                               options.customBracketPairs.end(),
                               [open, close](const auto& pair) {
                                   return pair.first == open &&
                                          pair.second == close;
                               });
        };

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

            // Process quotes
            if (options.validateQuotes) {
                if (current == '\'' && !doubleQuoteOpen &&
                    (!options.ignoreEscaped || !isEscaped)) {
                    singleQuoteOpen = !singleQuoteOpen;
                    continue;
                }

                if (current == '\"' && !singleQuoteOpen &&
                    (!options.ignoreEscaped || !isEscaped)) {
                    doubleQuoteOpen = !doubleQuoteOpen;
                    continue;
                }
            }

            if (singleQuoteOpen || doubleQuoteOpen) {
                continue;
            }

            // Process brackets
            if (options.validateBrackets) {
                // Standard brackets
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
                // Custom brackets
                else if (options.allowCustomBrackets) {
                    // Check if it's a custom opening bracket
                    bool isOpeningCustomBracket =
                        std::any_of(options.customBracketPairs.begin(),
                                    options.customBracketPairs.end(),
                                    options.customBracketPairs,
                                    [current](const auto& pair) {
                                        return pair.first == current;
                                    });

                    if (isOpeningCustomBracket) {
                        stack[stackSize++] = current;
                    } else {
                        // Check if it's a custom closing bracket
                        bool isClosingCustomBracket =
                            std::any_of(options.customBracketPairs.begin(),
                                        options.customBracketPairs.end(),
                                        options.customBracketPairs,
                                        [current](const auto& pair) {
                                            return pair.second == current;
                                        });

                        if (isClosingCustomBracket) {
                            if (stackSize == 0 ||
                                !isCustomMatching(stack[stackSize - 1],
                                                  current)) {
                                result.addError(static_cast<int>(i));
                            } else {
                                --stackSize;
                            }
                        }
                    }
                }
            }
        }

        // Handle unclosed brackets
        while (stackSize > 0) {
            result.addError(static_cast<int>(N - 1));
            --stackSize;
        }

        // Handle unclosed quotes
        if (options.validateQuotes) {
            if (singleQuoteOpen) {
                result.addError(static_cast<int>(N - 1));
            }

            if (doubleQuoteOpen) {
                result.addError(static_cast<int>(N - 1));
            }
        }

        return result;
    }
};

template <std::size_t N>
constexpr auto toArray(const char (&str)[N]) noexcept -> std::array<char, N> {
    std::array<char, N> arr{};
    std::copy_n(str, N, arr.begin());
    return arr;
}

// Add template specialization for string literals
template <std::size_t N>
constexpr auto validateBrackets(const char (&str)[N],
                                const ValidationOptions& options = {}) noexcept
    -> typename BracketValidator<N>::ValidationResult {
    return BracketValidator<N>::validate(std::span<const char, N>{str, N},
                                         options);
}

// Implement exception-free validation using std::expected
template <StringLike T>
constexpr auto validateStringNothrow(
    T&& str, const ValidationOptions& options = {}) noexcept
    -> std::expected<ValidationResult, std::string> {
    try {
        if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                      std::is_same_v<
                          std::remove_all_extents_t<std::remove_reference_t<T>>,
                          char>) {
            constexpr auto size = std::extent_v<std::remove_reference_t<T>>;
            auto result = validateBrackets(str, options);
            // Convert result format
            ValidationResult fullResult;
            if (!result.isValid()) {
                fullResult.isValid = false;
                for (const auto pos : result.getErrorPositions()) {
                    BracketInfo info{
                        '?', pos};  // Can't get exact bracket character info
                    fullResult.invalidBrackets.push_back(info);
                    fullResult.errorMessages.push_back(
                        std::format("Error at position {}", pos));
                }
            }
            return fullResult;
        } else {
            return isValidBracket(std::forward<T>(str), options);
        }
    } catch (const ValidationException& e) {
        return std::unexpected(e.what());
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Unexpected error: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error occurred");
    }
}

// Function returning ValidationResult
template <StringLike T>
auto validateString(T&& str, const ValidationOptions& options = {}) {
    if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                  std::is_same_v<
                      std::remove_all_extents_t<std::remove_reference_t<T>>,
                      char>) {
        constexpr auto size = std::extent_v<std::remove_reference_t<T>>;
        return validateBrackets(str, options);
    } else {
        auto result = isValidBracket(std::forward<T>(str), options);
        if (result) {
            return result.value();
        }
        ValidationResult error;
        error.isValid = false;
        error.addError(result.error());
        return error;
    }
}

// String instantiations declaration
extern template auto isValidBracket<std::string>(std::string&&,
                                                 const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<const std::string&>(
    const std::string&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<std::string_view>(std::string_view&&,
                                                      const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<const std::string_view&>(
    const std::string_view&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

// Unicode string support declarations
#ifdef __cpp_lib_char8_t
extern template auto isValidBracket<std::u8string>(std::u8string&&,
                                                   const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<const std::u8string&>(
    const std::u8string&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<std::u8string_view>(
    std::u8string_view&&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
extern template auto isValidBracket<const std::u8string_view&>(
    const std::u8string_view&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
#endif

}  // namespace atom::utils

#endif
