/**
 * @file string_utils.hpp
 * @brief String manipulation utility functions
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_STRING_UTILS_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_STRING_UTILS_HPP

#include <string>
#include <vector>

namespace atom::system::utils {

/**
 * @brief Trim whitespace from both ends of a string
 * @param str The string to trim
 * @return Trimmed string
 */
[[nodiscard]] auto trim(const std::string& str) -> std::string;

/**
 * @brief Trim whitespace from the left end of a string
 * @param str The string to trim
 * @return Left-trimmed string
 */
[[nodiscard]] auto ltrim(const std::string& str) -> std::string;

/**
 * @brief Trim whitespace from the right end of a string
 * @param str The string to trim
 * @return Right-trimmed string
 */
[[nodiscard]] auto rtrim(const std::string& str) -> std::string;

/**
 * @brief Convert string to lowercase
 * @param str The string to convert
 * @return Lowercase string
 */
[[nodiscard]] auto toLower(const std::string& str) -> std::string;

/**
 * @brief Convert string to uppercase
 * @param str The string to convert
 * @return Uppercase string
 */
[[nodiscard]] auto toUpper(const std::string& str) -> std::string;

/**
 * @brief Split string by delimiter
 * @param str The string to split
 * @param delimiter The delimiter character
 * @return Vector of split strings
 */
[[nodiscard]] auto split(const std::string& str, char delimiter) -> std::vector<std::string>;

/**
 * @brief Split string by delimiter string
 * @param str The string to split
 * @param delimiter The delimiter string
 * @return Vector of split strings
 */
[[nodiscard]] auto split(const std::string& str, const std::string& delimiter) -> std::vector<std::string>;

/**
 * @brief Join strings with delimiter
 * @param strings Vector of strings to join
 * @param delimiter The delimiter string
 * @return Joined string
 */
[[nodiscard]] auto join(const std::vector<std::string>& strings, const std::string& delimiter) -> std::string;

/**
 * @brief Replace all occurrences of a substring
 * @param str The source string
 * @param from The substring to replace
 * @param to The replacement string
 * @return String with replacements
 */
[[nodiscard]] auto replace(const std::string& str, const std::string& from, const std::string& to) -> std::string;

/**
 * @brief Check if string starts with prefix
 * @param str The string to check
 * @param prefix The prefix to look for
 * @return true if string starts with prefix
 */
[[nodiscard]] auto startsWith(const std::string& str, const std::string& prefix) -> bool;

/**
 * @brief Check if string ends with suffix
 * @param str The string to check
 * @param suffix The suffix to look for
 * @return true if string ends with suffix
 */
[[nodiscard]] auto endsWith(const std::string& str, const std::string& suffix) -> bool;

/**
 * @brief Check if string contains substring
 * @param str The string to search in
 * @param substring The substring to look for
 * @return true if string contains substring
 */
[[nodiscard]] auto contains(const std::string& str, const std::string& substring) -> bool;

/**
 * @brief Pad string to specified width with character
 * @param str The string to pad
 * @param width The target width
 * @param padChar The padding character
 * @param leftAlign Whether to left-align (pad right) or right-align (pad left)
 * @return Padded string
 */
[[nodiscard]] auto pad(const std::string& str, size_t width, char padChar = ' ', bool leftAlign = true) -> std::string;

/**
 * @brief Center string within specified width
 * @param str The string to center
 * @param width The target width
 * @param padChar The padding character
 * @return Centered string
 */
[[nodiscard]] auto center(const std::string& str, size_t width, char padChar = ' ') -> std::string;

/**
 * @brief Wrap text to specified line width
 * @param text The text to wrap
 * @param width The maximum line width
 * @return Vector of wrapped lines
 */
[[nodiscard]] auto wordWrap(const std::string& text, size_t width) -> std::vector<std::string>;

/**
 * @brief Escape special characters for specific format
 * @param str The string to escape
 * @param format The target format ("html", "xml", "json", "csv")
 * @return Escaped string
 */
[[nodiscard]] auto escape(const std::string& str, const std::string& format) -> std::string;

/**
 * @brief Remove ANSI color codes from string
 * @param str The string with potential ANSI codes
 * @return String without ANSI codes
 */
[[nodiscard]] auto removeAnsiCodes(const std::string& str) -> std::string;

/**
 * @brief Calculate display width of string (accounting for ANSI codes)
 * @param str The string to measure
 * @return Display width
 */
[[nodiscard]] auto displayWidth(const std::string& str) -> size_t;

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_STRING_UTILS_HPP