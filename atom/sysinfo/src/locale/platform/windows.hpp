/**
 * @file windows.hpp
 * @brief Windows-specific locale functionality
 *
 * This file contains Windows-specific implementations for locale operations
 * using the Windows API.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_WINDOWS_HPP
#define ATOM_SYSINFO_LOCALE_WINDOWS_HPP

#ifdef _WIN32

#include "common.hpp"
#include <string>
#include <vector>

namespace atom::system::locale::windows {

/**
 * @brief Converts a wide string to a UTF-8 string
 * @param wstr The wide string to convert
 * @return The converted UTF-8 string
 */
auto wstringToString(const std::wstring& wstr) -> std::string;

/**
 * @brief Converts a UTF-8 string to a wide string
 * @param str The UTF-8 string to convert
 * @return The converted wide string
 */
auto stringToWstring(const std::string& str) -> std::wstring;

/**
 * @brief Retrieves locale information from Windows API
 * @param type The type of locale information to retrieve
 * @param localeName Optional specific locale name (uses user default if empty)
 * @return The locale information as a string
 */
auto getLocaleInfo(unsigned long type, const std::string& localeName = "") -> std::string;

/**
 * @brief Retrieves numeric locale information from Windows API
 * @param type The type of numeric locale information to retrieve
 * @param localeName Optional specific locale name (uses user default if empty)
 * @return The numeric locale information
 */
auto getLocaleInfoNumeric(unsigned long type, const std::string& localeName = "") -> int;

/**
 * @brief Gets the system language information using Windows API
 * @return LocaleInfo structure with Windows-specific data
 */
auto getSystemLanguageInfo() -> LocaleInfo;

/**
 * @brief Gets all available locales on Windows
 * @return Vector of locale names
 */
auto getAvailableLocales() -> std::vector<std::string>;

/**
 * @brief Validates if a locale is available on Windows
 * @param locale The locale identifier to validate
 * @return true if the locale is valid and available, false otherwise
 */
auto validateLocale(const std::string& locale) -> bool;

/**
 * @brief Sets the system locale on Windows
 * @param locale The locale identifier to set
 * @return LocaleError indicating success or failure reason
 */
auto setSystemLocale(const std::string& locale) -> LocaleError;

/**
 * @brief Gets the default locale on Windows
 * @return Default locale identifier
 */
auto getDefaultLocale() -> std::string;

/**
 * @brief Gets the user's preferred UI languages
 * @return Vector of preferred language identifiers
 */
auto getPreferredUILanguages() -> std::vector<std::string>;

/**
 * @brief Gets the system's default UI language
 * @return Default UI language identifier
 */
auto getSystemDefaultUILanguage() -> std::string;

/**
 * @brief Gets the user's default UI language
 * @return User default UI language identifier
 */
auto getUserDefaultUILanguage() -> std::string;

/**
 * @brief Checks if a locale supports right-to-left text
 * @param locale The locale to check
 * @return true if the locale supports RTL, false otherwise
 */
auto isRTLLocale(const std::string& locale) -> bool;

/**
 * @brief Gets the currency information for a locale
 * @param locale The locale to get currency info for
 * @return Pair of currency symbol and currency code
 */
auto getCurrencyInfo(const std::string& locale) -> std::pair<std::string, std::string>;

/**
 * @brief Gets the measurement system for a locale
 * @param locale The locale to get measurement system for
 * @return MeasurementSystem enum value
 */
auto getMeasurementSystem(const std::string& locale) -> MeasurementSystem;

/**
 * @brief Gets the paper size for a locale
 * @param locale The locale to get paper size for
 * @return PaperSize enum value
 */
auto getPaperSize(const std::string& locale) -> PaperSize;

/**
 * @brief Gets the first day of week for a locale
 * @param locale The locale to get first day of week for
 * @return Day name (e.g., "Monday", "Sunday")
 */
auto getFirstDayOfWeek(const std::string& locale) -> std::string;

/**
 * @brief Gets the weekend days for a locale
 * @param locale The locale to get weekend days for
 * @return Vector of weekend day names
 */
auto getWeekendDays(const std::string& locale) -> std::vector<std::string>;

/**
 * @brief Gets the time zone information for the system
 * @return Time zone identifier
 */
auto getSystemTimeZone() -> std::string;

/**
 * @brief Formats a number according to locale settings
 * @param number The number to format
 * @param locale The locale to use for formatting
 * @return Formatted number string
 */
auto formatNumber(double number, const std::string& locale) -> std::string;

/**
 * @brief Formats currency according to locale settings
 * @param amount The amount to format
 * @param locale The locale to use for formatting
 * @return Formatted currency string
 */
auto formatCurrency(double amount, const std::string& locale) -> std::string;

/**
 * @brief Formats date according to locale settings
 * @param timestamp The timestamp to format
 * @param locale The locale to use for formatting
 * @param format Optional custom format string
 * @return Formatted date string
 */
auto formatDate(const std::chrono::system_clock::time_point& timestamp,
                const std::string& locale,
                const std::string& format = "") -> std::string;

/**
 * @brief Formats time according to locale settings
 * @param timestamp The timestamp to format
 * @param locale The locale to use for formatting
 * @param format Optional custom format string
 * @return Formatted time string
 */
auto formatTime(const std::chrono::system_clock::time_point& timestamp,
                const std::string& locale,
                const std::string& format = "") -> std::string;

} // namespace atom::system::locale::windows

#endif // _WIN32

#endif // ATOM_SYSINFO_LOCALE_WINDOWS_HPP
