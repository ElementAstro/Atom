/**
 * @file macos.hpp
 * @brief macOS-specific locale functionality
 *
 * This file contains macOS-specific implementations for locale operations
 * using Core Foundation and Cocoa APIs.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_MACOS_HPP
#define ATOM_SYSINFO_LOCALE_MACOS_HPP

#ifdef __APPLE__

#include "common.hpp"
#include <string>
#include <vector>
#include <chrono>

namespace atom::system::locale::macos {

/**
 * @brief Gets the system language information using macOS APIs
 * @return LocaleInfo structure with macOS-specific data
 */
auto getSystemLanguageInfo() -> LocaleInfo;

/**
 * @brief Gets all available locales on macOS
 * @return Vector of locale names
 */
auto getAvailableLocales() -> std::vector<std::string>;

/**
 * @brief Validates if a locale is available on macOS
 * @param locale The locale identifier to validate
 * @return true if the locale is valid and available, false otherwise
 */
auto validateLocale(const std::string& locale) -> bool;

/**
 * @brief Sets the system locale on macOS
 * @param locale The locale identifier to set
 * @return LocaleError indicating success or failure reason
 */
auto setSystemLocale(const std::string& locale) -> LocaleError;

/**
 * @brief Gets the default locale on macOS
 * @return Default locale identifier
 */
auto getDefaultLocale() -> std::string;

/**
 * @brief Gets the user's preferred languages from macOS preferences
 * @return Vector of preferred language identifiers
 */
auto getPreferredLanguages() -> std::vector<std::string>;

/**
 * @brief Gets the current locale from macOS system preferences
 * @return Current system locale
 */
auto getCurrentLocale() -> std::string;

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
 * @brief Gets locale information using Core Foundation
 * @param key The locale key to retrieve
 * @param locale Optional specific locale (uses current if empty)
 * @return The requested locale information
 */
auto getCFLocaleInfo(const std::string& key, const std::string& locale = "") -> std::string;

/**
 * @brief Gets the system region code
 * @return Region code (e.g., "US", "GB")
 */
auto getSystemRegion() -> std::string;

/**
 * @brief Gets the system language code
 * @return Language code (e.g., "en", "fr")
 */
auto getSystemLanguage() -> std::string;

/**
 * @brief Gets the calendar identifier for a locale
 * @param locale The locale to get calendar for
 * @return Calendar identifier (e.g., "gregorian", "buddhist")
 */
auto getCalendarIdentifier(const std::string& locale) -> std::string;

/**
 * @brief Gets the number formatting style for a locale
 * @param locale The locale to get number style for
 * @return Number formatting style information
 */
auto getNumberFormattingStyle(const std::string& locale) -> std::string;

/**
 * @brief Gets the date formatting style for a locale
 * @param locale The locale to get date style for
 * @return Date formatting style information
 */
auto getDateFormattingStyle(const std::string& locale) -> std::string;

/**
 * @brief Gets the time formatting style for a locale
 * @param locale The locale to get time style for
 * @return Time formatting style information
 */
auto getTimeFormattingStyle(const std::string& locale) -> std::string;

/**
 * @brief Checks if the system uses 24-hour time format
 * @return true if 24-hour format is used, false for 12-hour
 */
auto uses24HourFormat() -> bool;

/**
 * @brief Gets the decimal separator for a locale
 * @param locale The locale to get decimal separator for
 * @return Decimal separator character
 */
auto getDecimalSeparator(const std::string& locale) -> std::string;

/**
 * @brief Gets the thousands separator for a locale
 * @param locale The locale to get thousands separator for
 * @return Thousands separator character
 */
auto getThousandsSeparator(const std::string& locale) -> std::string;

/**
 * @brief Gets the list separator for a locale
 * @param locale The locale to get list separator for
 * @return List separator character
 */
auto getListSeparator(const std::string& locale) -> std::string;

/**
 * @brief Gets the quotation marks for a locale
 * @param locale The locale to get quotation marks for
 * @return Pair of opening and closing quotation marks
 */
auto getQuotationMarks(const std::string& locale) -> std::pair<std::string, std::string>;

/**
 * @brief Gets the exemplar character set for a locale
 * @param locale The locale to get exemplar characters for
 * @return String containing exemplar characters
 */
auto getExemplarCharacters(const std::string& locale) -> std::string;

} // namespace atom::system::locale::macos

#endif // __APPLE__

#endif // ATOM_SYSINFO_LOCALE_MACOS_HPP
