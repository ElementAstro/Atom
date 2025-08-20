#include "valid_string.hpp"

#include <algorithm>
#include <cstring>
#include <execution>  // For parallel algorithms
#include <format>     // C++20 string formatting
#include <iterator>
#include <latch>            // C++20 thread synchronization primitives
#include <memory_resource>  // C++17 memory resources
#include <span>             // C++20 span
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <version>  // Check standard library feature support

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#endif

namespace atom::utils {

// Forward declarations
template <StringLike T>
auto parallelValidation(T&& str, const ValidationOptions& options)
    -> std::expected<ValidationResult, std::string>;

template <StringLike T>
auto validateImpl(T&& str, const ValidationOptions& options)
    -> std::expected<ValidationResult, std::string>;

// Use PMR memory resources to improve small object performance
thread_local std::pmr::monotonic_buffer_resource threadLocalBuffer{4096};
thread_local std::pmr::polymorphic_allocator<char> threadLocalAllocator{
    &threadLocalBuffer};

namespace {
// Define bracket pair constants using std::array instead of C-style arrays
constexpr std::array<std::pair<char, char>, 4> bracketPairs{
    {{'(', ')'}, {'[', ']'}, {'{', '}'}, {'<', '>'}}};

// Get bracket type
constexpr auto getBracketType(char c) noexcept -> BracketType {
    switch (c) {
        case '(':
        case ')':
            return BracketType::Round;
        case '[':
        case ']':
            return BracketType::Square;
        case '{':
        case '}':
            return BracketType::Curly;
        case '<':
        case '>':
            return BracketType::Angle;
        default:
            return BracketType::Custom;
    }
}

// Constexpr function to check if a character is an opening bracket
constexpr bool isOpeningBracket(char c) noexcept {
    return c == '(' || c == '[' || c == '{' || c == '<';
}

// Constexpr function to check if a character is a closing bracket
constexpr bool isClosingBracket(char c) noexcept {
    return c == ')' || c == ']' || c == '}' || c == '>';
}

// Constexpr function to get corresponding opening bracket
constexpr char getOpeningBracket(char closing) noexcept {
    for (const auto& [open, close] : bracketPairs) {
        if (close == closing)
            return open;
    }
    return '\0';
}

// Create error message using std::format
std::string createMismatchedBracketMessage(char bracket, int position,
                                           bool isOpening) {
    if (isOpening) {
        return std::format(
            "Error: Opening bracket '{}' at position {} needs a closing "
            "bracket.",
            bracket, position);
    } else {
        return std::format(
            "Error: Closing bracket '{}' at position {} has no matching "
            "opening bracket.",
            bracket, position);
    }
}

// Optimize data processing using std::string_view and std::span
template <atom::utils::StringLike T>
std::span<const char> getDataSpan(const T& str) {
    using DecayedT = std::decay_t<T>;
    if constexpr (std::is_same_v<DecayedT, const char*> ||
                  std::is_same_v<DecayedT, char*>) {
        return std::span<const char>(str, std::strlen(str));
    } else {
        const char* data = reinterpret_cast<const char*>(str.data());
        return std::span<const char>(data, str.size());
    }
}

}  // namespace

namespace atom::utils {

// Parallel processing for large strings
template <StringLike T>
auto parallelValidation(T&& str, const ValidationOptions& options)
    -> std::expected<ValidationResult, std::string> {
    try {
        auto span = getDataSpan(str);
        const size_t length = span.size();

        // Only use parallel processing for large strings
        if (length < 10000) {
            return isValidBracket(std::forward<T>(str), options);
        }

        // Calculate optimal chunk size
        const size_t numThreads =
            std::max(2u, std::thread::hardware_concurrency());
        const size_t chunkSize = length / numThreads;

        std::vector<std::expected<ValidationResult, std::string>> results(
            numThreads);
        std::vector<std::thread> threads;
        std::latch completion_latch(
            numThreads);  // C++20 thread synchronization primitive

        // Process each chunk in parallel
        for (size_t i = 0; i < numThreads; ++i) {
            size_t start = i * chunkSize;
            size_t end = (i == numThreads - 1) ? length : (i + 1) * chunkSize;

            threads.emplace_back([&results, &completion_latch, i, &span, start,
                                  end, &options]() {
                try {
                    std::string_view chunk(span.data() + start, end - start);
                    auto chunkResult = isValidBracket(chunk, options);

                    // Adjust positions to be relative to original string if
                    // successful
                    if (chunkResult) {
                        for (auto& info : chunkResult->invalidBrackets) {
                            info.position += static_cast<int>(start);
                        }
                    }
                    results[i] = std::move(chunkResult);
                } catch (const std::exception& e) {
                    results[i] = std::unexpected(
                        std::format("Error in chunk {}: {}", i, e.what()));
                } catch (...) {
                    results[i] = std::unexpected(
                        std::format("Unknown error in chunk {}", i));
                }
                completion_latch.count_down();
            });
        }

        // Wait for all threads to complete
        completion_latch.wait();

        // Ensure all threads are properly joined
        for (auto& t : threads) {
            if (t.joinable())
                t.join();
        }

        // Merge results
        ValidationResult finalResult;
        finalResult.isValid = true;

        for (auto& result : results) {
            // Check if result has an error (doesn't have a value)
            if (!result.has_value()) {
                // Handle processing error cases
                if (finalResult.isValid) {
                    finalResult.isValid = false;
                    finalResult.errorMessages.push_back(result.error());
                }
                continue;
            }

            // Now we know result has a value, access it safely
            if (!result.value().isValid) {
                finalResult.isValid = false;

                // Use move semantics to efficiently merge results
                finalResult.invalidBrackets.insert(
                    finalResult.invalidBrackets.end(),
                    std::make_move_iterator(
                        result.value().invalidBrackets.begin()),
                    std::make_move_iterator(
                        result.value().invalidBrackets.end()));

                finalResult.errorMessages.insert(
                    finalResult.errorMessages.end(),
                    std::make_move_iterator(
                        result.value().errorMessages.begin()),
                    std::make_move_iterator(
                        result.value().errorMessages.end()));
            }
        }

        // Use parallel algorithm to sort bracket information
        if (!finalResult.invalidBrackets.empty()) {
            std::sort(std::execution::par_unseq,
                      finalResult.invalidBrackets.begin(),
                      finalResult.invalidBrackets.end(),
                      [](const BracketInfo& a, const BracketInfo& b) {
                          return a.position < b.position;
                      });
        }

        return finalResult;
    } catch (const std::exception& e) {
        return std::unexpected(
            std::format("Parallel validation error: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error in parallel validation");
    }
}

}  // namespace atom::utils

// Implement main validation logic using std::expected
template <StringLike T>
auto validateImpl(T&& str, const ValidationOptions& options)
    -> std::expected<ValidationResult, std::string> {
    // Input validation result
    ValidationResult result;

    try {
        auto span = getDataSpan(str);
        const size_t length = span.size();

        if (length == 0) {
            return result;  // Empty string is considered valid
        }

        // Use PMR allocator to improve performance
        std::pmr::vector<BracketInfo> stack(&threadLocalBuffer);
        stack.reserve(std::min(length, static_cast<size_t>(1024)));

        bool singleQuoteOpen = false;
        bool doubleQuoteOpen = false;

        // Main validation loop with SIMD-friendly structure
        for (std::size_t i = 0; i < length; ++i) {
            char current = span[i];

            // Process quotes
            if (options.validateQuotes) {
                if (current == '\'' && !doubleQuoteOpen) {
                    // Check escape sequences
                    bool isEscaped = (i > 0 && span[i - 1] == '\\');

                    // Calculate backslash count to handle \\' cases
                    if (isEscaped && options.ignoreEscaped) {
                        size_t backslashCount = 0;
                        size_t pos = i - 1;
                        while (pos < i && span[pos] == '\\') {
                            backslashCount++;
                            if (pos == 0)
                                break;
                            pos--;
                        }
                        isEscaped = (backslashCount % 2) == 1;
                    }

                    if (!options.ignoreEscaped || !isEscaped) {
                        singleQuoteOpen = !singleQuoteOpen;
                    }
                    continue;
                }

                if (current == '\"' && !singleQuoteOpen) {
                    // Check escape sequences
                    bool isEscaped = (i > 0 && span[i - 1] == '\\');

                    // Calculate backslash count to handle \\" cases
                    if (isEscaped && options.ignoreEscaped) {
                        size_t backslashCount = 0;
                        size_t pos = i - 1;
                        while (pos < i && span[pos] == '\\') {
                            backslashCount++;
                            if (pos == 0)
                                break;
                            pos--;
                        }
                        isEscaped = (backslashCount % 2) == 1;
                    }

                    if (!options.ignoreEscaped || !isEscaped) {
                        doubleQuoteOpen = !doubleQuoteOpen;
                    }
                    continue;
                }
            }

            // Skip characters inside quotes
            if (singleQuoteOpen || doubleQuoteOpen) {
                continue;
            }

            // Process standard brackets
            if (options.validateBrackets) {
                if (isOpeningBracket(current)) {
                    stack.emplace_back(current, static_cast<int>(i),
                                       getBracketType(current));
                } else if (isClosingBracket(current)) {
                    char expectedOpening = getOpeningBracket(current);

                    if (stack.empty() ||
                        stack.back().character != expectedOpening) {
                        auto message = createMismatchedBracketMessage(
                            current, static_cast<int>(i), false);
                        result.addError({current, static_cast<int>(i),
                                         getBracketType(current)},
                                        "{}", message);
                    } else {
                        stack.pop_back();
                    }
                }
                // Process custom brackets
                else if (options.allowCustomBrackets &&
                         !options.customBracketPairs.empty()) {
                    // Check if it's a custom opening bracket
                    auto it =
                        std::ranges::find_if(options.customBracketPairs,
                                             [current](const auto& pair) {
                                                 return pair.first == current;
                                             });

                    if (it != options.customBracketPairs.end()) {
                        stack.emplace_back(current, static_cast<int>(i),
                                           BracketType::Custom);
                    } else {
                        // Check if it's a custom closing bracket
                        auto closeIt = std::ranges::find_if(
                            options.customBracketPairs,
                            [current](const auto& pair) {
                                return pair.second == current;
                            });

                        if (closeIt != options.customBracketPairs.end()) {
                            char expectedOpening = closeIt->first;

                            if (stack.empty() ||
                                stack.back().character != expectedOpening) {
                                auto message = createMismatchedBracketMessage(
                                    current, static_cast<int>(i), false);
                                result.addError({current, static_cast<int>(i),
                                                 BracketType::Custom},
                                                "{}", message);
                            } else {
                                stack.pop_back();
                            }
                        }
                    }
                }
            }
        }

        // Process unclosed brackets
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto message = createMismatchedBracketMessage(it->character,
                                                          it->position, true);
            result.addError(*it, "{}", message);
        }

        // Handle unclosed quotes
        if (options.validateQuotes) {
            if (singleQuoteOpen) {
                result.addError("Error: Single quote is not closed.");

                // Could throw specific exception, but we use expected pattern
                // instead throw
                // QuoteMismatchException(QuoteMismatchException::QuoteType::Single);
            }

            if (doubleQuoteOpen) {
                result.addError("Error: Double quote is not closed.");

                // Could throw specific exception, but we use expected pattern
                // instead throw
                // QuoteMismatchException(QuoteMismatchException::QuoteType::Double);
            }
        }

        return result;
    } catch (const std::exception& e) {
        // Log error and return unexpected value
        return std::unexpected(std::format("Validation error: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown validation error occurred");
    }
}

// Implement isValidBracket using std::expected for error handling
template <StringLike T>
auto isValidBracket(T&& str, const ValidationOptions& options)
    -> std::expected<ValidationResult, std::string> {
    try {
        // Get string size
        size_t size;
        if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                      std::is_same_v<std::decay_t<T>, char*>) {
            size = std::strlen(str);
        } else {
            size = str.size();
        }

        // Use standard processing for small strings
        return validateImpl(std::forward<T>(str), options);
    } catch (const std::exception& e) {
        return std::unexpected(
            std::format("String validation failed: {}", e.what()));
    } catch (...) {
        return std::unexpected("Unknown error during string validation");
    }
}

// Full explicit template instantiation for all common string types
// Standard string types
template auto isValidBracket<std::string>(std::string&&,
                                          const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<const std::string&>(const std::string&,
                                                 const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<std::string&>(std::string&,
                                           const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

// String view types
template auto isValidBracket<std::string_view>(std::string_view&&,
                                               const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<const std::string_view&>(const std::string_view&,
                                                      const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<std::string_view&>(std::string_view&,
                                                const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

// C-string types
template auto isValidBracket<const char*>(const char*&&,
                                          const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<const char*&>(const char*&,
                                           const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<char*>(char*&&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<char*&>(char*&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

// Unicode string support instantiations
#ifdef __cpp_lib_char8_t
template auto isValidBracket<std::u8string>(std::u8string&&,
                                            const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<const std::u8string&>(const std::u8string&,
                                                   const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<std::u8string&>(std::u8string&,
                                             const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

template auto isValidBracket<std::u8string_view>(std::u8string_view&&,
                                                 const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<const std::u8string_view&>(
    const std::u8string_view&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto isValidBracket<std::u8string_view&>(std::u8string_view&,
                                                  const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
#endif

// Unicode char16_t support
// The StringLike concept automatically handles unicode string types,
// so we only need instantiations for basic string types

// Explicit instantiations for validateImpl
template auto validateImpl<std::string>(std::string&&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto validateImpl<const std::string&>(const std::string&,
                                               const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto validateImpl<std::string_view>(std::string_view&&,
                                             const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto validateImpl<const std::string_view&>(const std::string_view&,
                                                    const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto validateImpl<const char*>(const char*&&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;
template auto validateImpl<char*>(char*&&, const ValidationOptions&)
    -> std::expected<ValidationResult, std::string>;

}  // namespace atom::utils