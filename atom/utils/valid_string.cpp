#include "valid_string.hpp"

#include <algorithm>
#include <cstring>   // 添加 cstring 头文件获取 std::strlen
#include <iterator>  // 添加 iterator 头文件获取 std::data 和 std::size
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#endif

namespace atom::utils {

// 声明实现函数模板，移到文件前部
template <typename T>
auto isValidBracketImpl(T&& str) -> ValidationResult;

namespace {
// Bracket pairs for quick lookup
constexpr std::pair<char, char> bracketPairs[] = {
    {'(', ')'}, {'[', ']'}, {'{', '}'}, {'<', '>'}};

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

// Function to create error message for mismatched bracket
std::string createMismatchedBracketMessage(char bracket, int position,
                                           bool isOpening) {
    if (isOpening) {
        return "Error: Opening bracket '" + std::string(1, bracket) +
               "' at position " + std::to_string(position) +
               " needs a closing bracket.";
    } else {
        return "Error: Closing bracket '" + std::string(1, bracket) +
               "' at position " + std::to_string(position) +
               " has no matching opening bracket.";
    }
}

// Parallel processing for large strings
template <StringLike T>
ValidationResult parallelValidation(T&& str) {
    // 对于不同的字符串类型使用不同的方法获取长度和数据
    size_t length;
    const char* data;

    if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                  std::is_same_v<std::decay_t<T>, char*>) {
        length = std::strlen(str);
        data = str;
    } else {
        length = str.size();
        data = str.data();
    }

    // Only use parallelism for larger strings
    if (length < 10000) {
        return isValidBracketImpl(
            std::forward<T>(str));  // 避免递归，调用实现函数
    }

    // Calculate optimal chunk size based on hardware
    const size_t numThreads = std::max(2u, std::thread::hardware_concurrency());
    const size_t chunkSize = length / numThreads;

    std::vector<ValidationResult> results(numThreads);
    std::vector<std::thread> threads;

    // Process each chunk in parallel
    for (size_t i = 0; i < numThreads; ++i) {
        size_t start = i * chunkSize;
        size_t end = (i == numThreads - 1) ? length : (i + 1) * chunkSize;

        threads.emplace_back([&results, i, data, start, end]() {
            std::string chunk(data + start, end - start);
            results[i] = isValidBracketImpl(chunk);  // 调用实现函数

            // Adjust positions to be relative to the original string
            for (auto& info : results[i].invalidBrackets) {
                info.position += static_cast<int>(start);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        if (t.joinable())
            t.join();
    }

    // Merge results
    ValidationResult finalResult;
    finalResult.isValid = true;

    for (const auto& result : results) {
        if (!result.isValid) {
            finalResult.isValid = false;
            finalResult.invalidBrackets.insert(
                finalResult.invalidBrackets.end(),
                result.invalidBrackets.begin(), result.invalidBrackets.end());

            finalResult.errorMessages.insert(finalResult.errorMessages.end(),
                                             result.errorMessages.begin(),
                                             result.errorMessages.end());
        }
    }

    // Sort results by position for better readability
    std::sort(finalResult.invalidBrackets.begin(),
              finalResult.invalidBrackets.end(),
              [](const BracketInfo& a, const BracketInfo& b) {
                  return a.position < b.position;
              });

    return finalResult;
}
}  // namespace

// 实现函数
template <typename T>
auto isValidBracketImpl(T&& str) -> ValidationResult {
    // Input validation
    ValidationResult result;

    try {
        // 处理不同类型的字符串
        size_t length;
        const char* data_ptr;

        if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                      std::is_same_v<std::decay_t<T>, char*>) {
            length = std::strlen(str);
            data_ptr = str;
        } else {
            length = str.size();
            data_ptr = str.data();
        }

        if (length == 0) {
            return result;  // Empty string is valid
        }

        // Use a preallocated stack with capacity hint to avoid reallocations
        std::vector<BracketInfo> stack;
        stack.reserve(std::min(length, static_cast<decltype(length)>(1024)));

        bool singleQuoteOpen = false;
        bool doubleQuoteOpen = false;

        // Main validation loop with SIMD-friendly structure
        for (std::size_t i = 0; i < length; ++i) {
            char current = data_ptr[i];

            // Handle quotes
            if (current == '\'' && !doubleQuoteOpen) {
                // Check for escape sequences
                bool isEscaped = (i > 0 && data_ptr[i - 1] == '\\');
                // Count backslashes to handle cases like \\'
                if (isEscaped) {
                    size_t backslashCount = 0;
                    size_t pos = i - 1;
                    while (pos < i && data_ptr[pos] == '\\') {
                        backslashCount++;
                        if (pos == 0)
                            break;
                        pos--;
                    }
                    isEscaped = (backslashCount % 2) == 1;
                }

                if (!isEscaped) {
                    singleQuoteOpen = !singleQuoteOpen;
                }
                continue;
            }

            if (current == '\"' && !singleQuoteOpen) {
                // Check for escape sequences
                bool isEscaped = (i > 0 && data_ptr[i - 1] == '\\');
                // Count backslashes to handle cases like \\"
                if (isEscaped) {
                    size_t backslashCount = 0;
                    size_t pos = i - 1;
                    while (pos < i && data_ptr[pos] == '\\') {
                        backslashCount++;
                        if (pos == 0)
                            break;
                        pos--;
                    }
                    isEscaped = (backslashCount % 2) == 1;
                }

                if (!isEscaped) {
                    doubleQuoteOpen = !doubleQuoteOpen;
                }
                continue;
            }

            // Skip characters inside quotes
            if (singleQuoteOpen || doubleQuoteOpen) {
                continue;
            }

            // Handle brackets
            if (isOpeningBracket(current)) {
                stack.push_back({current, static_cast<int>(i)});
            } else if (isClosingBracket(current)) {
                char expectedOpening = getOpeningBracket(current);

                if (stack.empty() ||
                    stack.back().character != expectedOpening) {
                    auto message = createMismatchedBracketMessage(
                        current, static_cast<int>(i), false);
                    result.addError({current, static_cast<int>(i)}, message);
                } else {
                    stack.pop_back();
                }
            }
        }

        // Handle unclosed brackets
        for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
            auto message = createMismatchedBracketMessage(it->character,
                                                          it->position, true);
            result.addError(*it, message);
        }

        // Handle unclosed quotes
        if (singleQuoteOpen) {
            result.addError("Error: Single quote is not closed.");
        }

        if (doubleQuoteOpen) {
            result.addError("Error: Double quote is not closed.");
        }
    } catch (const std::exception& e) {
        result.isValid = false;
        result.errorMessages.push_back(
            std::string("Error during validation: ") + e.what());
    }

    return result;
}

// 修改为正确的函数模板声明
template <typename T>
auto isValidBracket(T&& str) -> ValidationResult {
    // 对于大型字符串使用并行处理，否则使用常规实现
    if constexpr (StringLike<std::decay_t<T>>) {
        // 根据字符串类型判断大小
        size_t size;
        if constexpr (std::is_same_v<std::decay_t<T>, const char*> ||
                      std::is_same_v<std::decay_t<T>, char*>) {
            size = std::strlen(str);
        } else {
            size = str.size();
        }

        if (size >= 10000) {
            return parallelValidation(std::forward<T>(str));
        }
    }
    return isValidBracketImpl(std::forward<T>(str));
}

// 对应的实现函数模板实例化
template ValidationResult isValidBracketImpl<std::string>(std::string&&);
template ValidationResult isValidBracketImpl<const std::string&>(
    const std::string&);
template ValidationResult isValidBracketImpl<std::string_view>(
    std::string_view&&);
template ValidationResult isValidBracketImpl<const std::string_view&>(
    const std::string_view&);
template ValidationResult isValidBracketImpl<const char*>(const char*&&);
template ValidationResult isValidBracket<const char*>(const char*&&);

}  // namespace atom::utils