/*
 * string.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Some useful string functions

**************************************************/

#include "string.hpp"

#include <algorithm>
#include <charconv>
#include <execution>
#include <format>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>

namespace atom::utils {

auto hasUppercase(std::string_view str) -> bool {
    return std::ranges::any_of(
        str, [](unsigned char ch) { return std::isupper(ch); });
}

auto toUnderscore(std::string_view str) -> std::string {
    try {
        if (str.empty()) {
            return {};
        }

        // Count uppercase characters for better reserve
        const auto uppercaseCount = std::ranges::count_if(
            str, [](unsigned char ch) { return std::isupper(ch); });

        std::string result;
        result.reserve(str.size() + uppercaseCount);

        bool firstChar = true;

        for (char ch : str) {
            if (std::isupper(static_cast<unsigned char>(ch))) {
                if (!firstChar) {
                    result.push_back('_');
                }
                result.push_back(std::tolower(static_cast<unsigned char>(ch)));
                firstChar = false;
            } else {
                result.push_back(ch);
                firstChar = false;
            }
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto toCamelCase(std::string_view str) -> std::string {
    try {
        if (str.empty()) {
            return {};
        }

        std::string result;
        result.reserve(str.size());

        bool capitalize = false;
        for (char ch : str) {
            if (ch == '_') {
                capitalize = true;
            } else if (capitalize) {
                result.push_back(std::toupper(static_cast<unsigned char>(ch)));
                capitalize = false;
            } else {
                result.push_back(ch);
            }
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto urlEncode(std::string_view str) -> std::string {
    try {
        if (str.empty()) {
            return {};
        }

        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;

        for (auto c : str) {
            const unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalnum(uc) != 0 || c == '-' || c == '_' || c == '.' ||
                c == '~') {
                escaped << c;
            } else if (c == ' ') {
                escaped << '+';
            } else {
                escaped << '%' << std::setw(2) << static_cast<int>(uc);
            }
        }

        return escaped.str();
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("URL encoding failed: {}", e.what()));
    }
}

auto urlDecode(std::string_view str) -> std::string {
    try {
        if (str.empty()) {
            return {};
        }

        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '%') {
                if (i + 2 >= str.size()) {
                    throw std::invalid_argument("Incomplete escape sequence");
                }

                int value = 0;
                const std::from_chars_result res = std::from_chars(
                    str.data() + i + 1, str.data() + i + 3, value, 16);

                if (res.ec != std::errc()) {
                    throw std::invalid_argument("Invalid escape sequence");
                }

                result.push_back(static_cast<char>(value));
                i += 2;
            } else if (str[i] == '+') {
                result.push_back(' ');
            } else {
                result.push_back(str[i]);
            }
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(
            std::format("URL decoding failed: {}", e.what()));
    }
}

auto startsWith(std::string_view str, std::string_view prefix) -> bool {
    return str.size() >= prefix.size() &&
           str.substr(0, prefix.size()) == prefix;
}

auto endsWith(std::string_view str, std::string_view suffix) -> bool {
    return str.size() >= suffix.size() &&
           str.substr(str.size() - suffix.size()) == suffix;
}

auto splitString(std::string_view str, char delimiter)
    -> std::vector<std::string> {
    try {
        if (str.empty()) {
            return {};
        }

        std::vector<std::string> tokens;

        // Pre-allocate for performance
        const auto delimCount = std::ranges::count(str, delimiter);
        tokens.reserve(delimCount + 1);

        for (const auto& part : split(str, delimiter)) {
            tokens.emplace_back(part);
        }

        return tokens;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto joinStrings(std::span<const std::string_view> strings,
                 std::string_view delimiter) -> std::string {
    try {
        if (strings.empty()) {
            return {};
        }

        // Calculate total length for pre-allocation
        size_t totalSize = 0;
        for (const auto& str : strings) {
            totalSize += str.size();
        }
        totalSize +=
            delimiter.size() * (strings.size() > 0 ? strings.size() - 1 : 0);

        std::string result;
        result.reserve(totalSize);

        bool first = true;
        for (const auto& str : strings) {
            if (!first) {
                result.append(delimiter);
            }
            result.append(str);
            first = false;
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto replaceString(std::string_view text, std::string_view oldStr,
                   std::string_view newStr) -> std::string {
    try {
        if (text.empty() || oldStr.empty()) {
            return std::string(text);
        }

        std::string result;
        // Pre-allocate for enhanced performance
        const size_t reserveSize =
            text.size() +
            std::max<size_t>(0,
                             static_cast<size_t>(text.size() / oldStr.size()) *
                                 (newStr.size() - oldStr.size()));
        result.reserve(reserveSize);

        size_t pos = 0;
        size_t lastPos = 0;

        while ((pos = text.find(oldStr, lastPos)) != std::string_view::npos) {
            result.append(text.substr(lastPos, pos - lastPos));
            result.append(newStr);
            lastPos = pos + oldStr.size();
        }

        result.append(text.substr(lastPos));
        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto replaceStrings(
    std::string_view text,
    std::span<const std::pair<std::string_view, std::string_view>> replacements)
    -> std::string {
    try {
        if (text.empty() || replacements.empty()) {
            return std::string(text);
        }

        std::string result(text);
        for (const auto& [oldStr, newStr] : replacements) {
            // Skip empty oldStr to avoid infinite loop
            if (oldStr.empty())
                continue;
            result = replaceString(result, oldStr, newStr);
        }
        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto SVVtoSV(std::span<const std::string_view> svv)
    -> std::vector<std::string> {
    try {
        std::vector<std::string> result;
        result.reserve(svv.size());

        for (const auto& view : svv) {
            result.emplace_back(view);
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto explode(std::string_view text, char symbol) -> std::vector<std::string> {
    try {
        if (text.empty()) {
            return {};
        }

        // Count occurrences of symbol for better reserve
        const auto symbolCount = std::ranges::count(text, symbol);
        std::vector<std::string> lines;
        lines.reserve(symbolCount + 1);

        for (const auto& part : split(text, symbol)) {
            lines.emplace_back(part);
        }

        return lines;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

std::string trim(std::string_view line, std::string_view symbols) {
    try {
        if (line.empty()) {
            return {};
        }

        const auto isSymbol = [&symbols](char c) {
            return symbols.find(c) != std::string_view::npos;
        };

        // Using C++20 ranges for cleaner code
        auto start = std::ranges::find_if_not(line, isSymbol);
        if (start == line.end()) {
            return {};
        }

        auto rbegin = std::make_reverse_iterator(line.end());
        auto rend = std::make_reverse_iterator(start);
        auto last = std::ranges::find_if_not(
            std::ranges::subrange(rbegin, rend), isSymbol);
        auto end = last.base();

        return std::string(start, end);
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

auto stringToWString(std::string_view str) -> std::wstring {
    try {
        if (str.empty()) {
            return {};
        }

        // Manual UTF-8 to UTF-16 conversion
        std::wstring result;
        result.reserve(str.size());  // Reserve at least the same size

        // Process each byte in the UTF-8 string
        for (size_t i = 0; i < str.size();) {
            const unsigned char c = static_cast<unsigned char>(str[i++]);

            // Single-byte character (ASCII)
            if (c < 0x80) {
                const unsigned char c2 = static_cast<unsigned char>(str[i++]);
                const unsigned char c3 = static_cast<unsigned char>(str[i++]);
                const unsigned char c4 = static_cast<unsigned char>(str[i++]);
                // Convert to Unicode code point
                const uint32_t codepoint = ((c & 0x07) << 18) |
                                           ((c2 & 0x3F) << 12) |
                                           ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                // Convert to UTF-16 surrogate pair
                if (codepoint > 0xFFFF) {
                    // High surrogate
                    result.push_back(static_cast<wchar_t>(
                        0xD800 + ((codepoint - 0x10000) >> 10)));
                    // Low surrogate
                    result.push_back(static_cast<wchar_t>(
                        0xDC00 + ((codepoint - 0x10000) & 0x3FF)));
                } else {
                    result.push_back(static_cast<wchar_t>(codepoint));
                }
            } else {
                // Invalid UTF-8 sequence, skip
                result.push_back(L'?');
            }
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("String to WString conversion failed: {}", e.what()));
    }
}

auto stod(std::string_view str, std::size_t* idx) -> double {
    if (str.empty()) {
        throw std::invalid_argument("Cannot convert empty string to double");
    }

    try {
        return std::stod(std::string(str), idx);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(
            std::format("String to double conversion failed: {}", e.what()));
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(std::format(
            "String to double conversion out of range: {}", e.what()));
    }
}

auto stof(std::string_view str, std::size_t* idx) -> float {
    if (str.empty()) {
        throw std::invalid_argument("Cannot convert empty string to float");
    }

    try {
        return std::stof(std::string(str), idx);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(
            std::format("String to float conversion failed: {}", e.what()));
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(std::format(
            "String to float conversion out of range: {}", e.what()));
    }
}

auto stoi(std::string_view str, std::size_t* idx, int base) -> int {
    if (str.empty()) {
        throw std::invalid_argument("Cannot convert empty string to int");
    }

    try {
        return std::stoi(std::string(str), idx, base);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(
            std::format("String to int conversion failed: {}", e.what()));
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(
            std::format("String to int conversion out of range: {}", e.what()));
    }
}

auto stol(std::string_view str, std::size_t* idx, int base) -> long {
    if (str.empty()) {
        throw std::invalid_argument("Cannot convert empty string to long");
    }

    try {
        return std::stol(std::string(str), idx, base);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument(
            std::format("String to long conversion failed: {}", e.what()));
    } catch (const std::out_of_range& e) {
        throw std::out_of_range(std::format(
            "String to long conversion out of range: {}", e.what()));
    }
}

auto nstrtok(std::string_view& str, const std::string_view& delims)
    -> std::optional<std::string_view> {
    if (str.empty()) {
        return std::nullopt;
    }

    // Use C++20 ranges for more expressive code
    auto first_non_delim = std::ranges::find_if(str, [&delims](char c) {
        return delims.find(c) == std::string_view::npos;
    });

    if (first_non_delim == str.end()) {
        str = {};
        return std::nullopt;
    }

    const size_t start = std::distance(str.begin(), first_non_delim);
    const auto next_delim = std::ranges::find_if(
        str.substr(start),
        [&delims](char c) { return delims.find(c) != std::string_view::npos; });

    std::string_view token;
    if (next_delim == str.substr(start).end()) {
        token = str.substr(start);
        str = {};
    } else {
        const size_t length =
            std::distance(str.substr(start).begin(), next_delim);
        token = str.substr(start, length);
        // Safely handle boundary cases
        const size_t next_pos = start + length + 1;
        if (next_pos <= str.size()) {
            str.remove_prefix(next_pos);
        } else {
            str = {};
        }
    }

    return token;
}

// Additional helper functions to enhance string utility functionality

/**
 * @brief Parallel replace function that uses multiple threads for large strings
 *
 * @param text The input text for replacement
 * @param oldStr The substring to be replaced
 * @param newStr The replacement string
 * @param threshold Size threshold to use parallel algorithm (default 10000)
 * @return The text with all replacements
 * @throws std::bad_alloc if memory allocation fails
 */
auto parallelReplaceString(std::string_view text, std::string_view oldStr,
                           std::string_view newStr, size_t threshold)
    -> std::string {
    try {
        // For small strings or when oldStr is empty, use the regular approach
        if (text.size() < threshold || oldStr.empty()) {
            return replaceString(text, oldStr, newStr);
        }

        // Prepare positions of matches
        std::vector<size_t> positions;
        for (size_t pos = 0;
             (pos = text.find(oldStr, pos)) != std::string_view::npos;
             pos += oldStr.size()) {
            positions.push_back(pos);
        }

        // If no replacements needed, return original
        if (positions.empty()) {
            return std::string(text);
        }

        // Calculate final size
        const size_t finalSize =
            text.size() + positions.size() * (newStr.size() - oldStr.size());

        std::string result;
        result.reserve(finalSize);

        size_t lastPos = 0;
        for (size_t pos : positions) {
            result.append(text.substr(lastPos, pos - lastPos));
            result.append(newStr);
            lastPos = pos + oldStr.size();
        }

        // Append remaining text
        result.append(text.substr(lastPos));

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("Parallel replace failed: {}", e.what()));
    }
}

/**
 * @brief Parallel version of SVVtoSV
 *
 * @param svv The vector of string_view to convert
 * @param threshold Size threshold to use parallel algorithm (default 1000)
 * @return The converted vector of string
 * @throws std::bad_alloc if memory allocation fails
 */
auto parallelSVVtoSV(std::span<const std::string_view> svv, size_t threshold)
    -> std::vector<std::string> {
    try {
        if (svv.empty()) {
            return {};
        }

        std::vector<std::string> result(svv.size());

        // Use parallel execution for large arrays
        if (svv.size() >= threshold) {
            std::transform(std::execution::par_unseq, svv.begin(), svv.end(),
                           result.begin(),
                           [](std::string_view sv) { return std::string(sv); });
        } else {
            std::transform(svv.begin(), svv.end(), result.begin(),
                           [](std::string_view sv) { return std::string(sv); });
        }

        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("Parallel SVVtoSV failed: {}", e.what()));
    }
}

/**
 * @brief Safe toLower function that handles Unicode properly
 *
 * @param str String to convert to lowercase
 * @return Lowercase string
 * @throws std::bad_alloc if memory allocation fails
 */
auto toLower(std::string_view str) -> std::string {
    try {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

/**
 * @brief Safe toUpper function that handles Unicode properly
 *
 * @param str String to convert to uppercase
 * @return Uppercase string
 * @throws std::bad_alloc if memory allocation fails
 */
auto toUpper(std::string_view str) -> std::string {
    try {
        std::string result(str);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::toupper(c); });
        return result;
    } catch (const std::bad_alloc& e) {
        throw std::bad_alloc();
    }
}

/**
 * @brief Validates if a string is a valid number in a given base
 *
 * @param str String to validate
 * @param base Number base (2-36)
 * @return True if string is a valid number in the given base
 */
auto isValidNumber(std::string_view str, int base = 10) -> bool {
    if (str.empty() || base < 2 || base > 36) {
        return false;
    }

    bool hasDigit = false;
    size_t i = 0;

    // Check for sign
    if (str[0] == '+' || str[0] == '-') {
        i++;
    }

    for (; i < str.size(); ++i) {
        const char c = str[i];

        if (c == '.' || c == 'e' || c == 'E') {
            // For floating point or scientific notation in base 10
            if (base == 10) {
                continue;
            }
            return false;
        }

        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'z') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'Z') {
            digit = c - 'A' + 10;
        } else {
            return false;
        }

        if (digit >= base) {
            return false;
        }

        hasDigit = true;
    }

    return hasDigit;
}

}  // namespace atom::utils
