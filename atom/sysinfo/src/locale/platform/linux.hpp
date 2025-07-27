/**
 * @file linux.hpp
 * @brief Linux-specific locale functionality
 *
 * This file contains Linux-specific implementations for locale operations
 * using POSIX and GNU libc APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_LINUX_HPP
#define ATOM_SYSINFO_LOCALE_LINUX_HPP

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include "common.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace atom::system::locale::linux_impl {

/**
 * @brief Gets the system language information using POSIX APIs
 * @return LocaleInfo structure with Linux-specific data
 */
auto getSystemLanguageInfo() -> LocaleInfo;

/**
 * @brief Gets all available locales on Linux
 * @return Vector of locale names
 */
auto getAvailableLocales() -> std::vector<std::string>;

/**
 * @brief Validates if a locale is available on Linux
 * @param locale The locale identifier to validate
 * @return true if the locale is valid and available, false otherwise
 */
auto validateLocale(const std::string& locale) -> bool;

/**
 * @brief Sets the system locale on Linux
 * @param locale The locale identifier to set
 * @return LocaleError indicating success or failure reason
 */
auto setSystemLocale(const std::string& locale) -> LocaleError;

/**
 * @brief Gets the default locale on Linux
 * @return Default locale identifier
 */
auto getDefaultLocale() -> std::string;

/**
 * @brief Gets locale information from environment variables
 * @param category The locale category to get (LC_ALL, LC_LANG, etc.)
 * @return Locale string from environment
 */
auto getLocaleFromEnvironment(const std::string& category = "LC_ALL") -> std::string;

/**
 * @brief Gets the user's preferred languages from environment
 * @return Vector of preferred language identifiers
 */
auto getPreferredLanguages() -> std::vector<std::string>;

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

/**
 * @brief Gets locale information using nl_langinfo
 * @param item The langinfo item to retrieve
 * @param locale Optional locale to set temporarily
 * @return The requested locale information
 */
auto getLanguageInfo(int item, const std::string& locale = "") -> std::string;

/**
 * @brief Parses locale from /etc/locale.conf or similar system files
 * @return System default locale from configuration files
 */
auto getSystemLocaleFromConfig() -> std::string;

/**
 * @brief Gets locale information from systemd localectl if available
 * @return Locale information from systemd
 */
auto getLocaleFromSystemd() -> std::string;

/**
 * @brief Checks if a locale is installed on the system
 * @param locale The locale to check
 * @return true if locale is installed, false otherwise
 */
auto isLocaleInstalled(const std::string& locale) -> bool;

/**
 * @brief Gets the character encoding for a locale
 * @param locale The locale to get encoding for
 * @return Character encoding string
 */
auto getCharacterEncoding(const std::string& locale) -> std::string;

/**
 * @brief Gets collation information for a locale
 * @param locale The locale to get collation for
 * @return Collation sequence information
 */
auto getCollationInfo(const std::string& locale) -> std::string;

/**
 * @brief Gets numeric formatting information
 * @param locale The locale to get numeric info for
 * @return Numeric formatting details
 */
auto getNumericInfo(const std::string& locale) -> std::string;

/**
 * @brief Gets monetary formatting information
 * @param locale The locale to get monetary info for
 * @return Monetary formatting details
 */
auto getMonetaryInfo(const std::string& locale) -> std::string;

/**
 * @brief Gets time and date formatting information
 * @param locale The locale to get time info for
 * @return Time and date formatting details
 */
auto getTimeInfo(const std::string& locale) -> std::string;

/**
 * @brief Gets message catalog information
 * @param locale The locale to get message info for
 * @return Message catalog details
 */
auto getMessageInfo(const std::string& locale) -> std::string;

} // namespace atom::system::locale::linux_impl

#endif // defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#endif // ATOM_SYSINFO_LOCALE_LINUX_HPP
