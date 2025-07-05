/**
 * @file locale.hpp
 * @brief System locale information functionality
 *
 * This file contains definitions for retrieving and managing system locale
 * information across different platforms. It provides utilities for querying
 * the current system locale, available locales, and locale-specific formatting
 * settings.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_HPP
#define ATOM_SYSINFO_LOCALE_HPP

#include <chrono>
#include <string>
#include <vector>

namespace atom::system {

/**
 * @enum LocaleError
 * @brief Error codes for locale operations
 *
 * Defines possible error conditions that may occur during locale-related
 * operations.
 */
enum class LocaleError {
    None,          /**< No error occurred */
    InvalidLocale, /**< The specified locale is invalid or not recognized */
    SystemError,   /**< A system-level error occurred during the operation */
    UnsupportedPlatform /**< The operation is not supported on the current
                           platform */
};

/**
 * @struct LocaleInfo
 * @brief Comprehensive information about a system locale
 *
 * Contains detailed information about locale settings including language,
 * country, formatting preferences, and display characteristics.
 */
struct LocaleInfo {
    std::string languageCode;        /**< ISO 639 language code (e.g., "en") */
    std::string countryCode;         /**< ISO 3166 country code (e.g., "US") */
    std::string localeName;          /**< Full locale name (e.g., "en_US") */
    std::string languageDisplayName; /**< Human-readable language name */
    std::string countryDisplayName;  /**< Human-readable country name */
    std::string currencySymbol;      /**< Currency symbol (e.g., "$") */
    std::string decimalSymbol;       /**< Decimal point symbol (e.g., ".") */
    std::string
        thousandSeparator;  /**< Thousands separator symbol (e.g., ",") */
    std::string dateFormat; /**< Date format string */
    std::string timeFormat; /**< Time format string */
    std::string characterEncoding; /**< Character encoding (e.g., "UTF-8") */
    bool isRTL{false};        /**< Whether text is displayed right-to-left */
    std::string numberFormat; /**< Number format pattern */
    std::string measurementSystem; /**< Measurement system (e.g., "metric",
                                      "imperial") */
    std::string paperSize; /**< Default paper size (e.g., "A4", "Letter") */
    std::chrono::seconds cacheTimeout{
        300}; /**< Cache timeout duration in seconds */

    /**
     * @brief Equality comparison operator
     * @param other Another LocaleInfo instance to compare with
     * @return true if the locales are equivalent, false otherwise
     */
    bool operator==(const LocaleInfo& other) const;
};

/**
 * @brief Retrieves the current system language and locale information
 *
 * Queries the operating system for current locale settings and returns
 * a populated LocaleInfo structure with all available details.
 *
 * @return LocaleInfo containing the system's current locale settings
 */
auto getSystemLanguageInfo() -> LocaleInfo;

/**
 * @brief Displays locale information in a formatted manner
 *
 * Outputs the contents of a LocaleInfo structure to the standard output
 * in a human-readable format.
 *
 * @param info The LocaleInfo instance to display
 */
void printLocaleInfo(const LocaleInfo& info);

/**
 * @brief Validates if a locale identifier is valid and available on the system
 *
 * Checks whether the given locale string represents a valid locale that
 * can be used on the current system.
 *
 * @param locale The locale identifier to validate (e.g., "en_US")
 * @return true if the locale is valid and available, false otherwise
 */
auto validateLocale(const std::string& locale) -> bool;

/**
 * @brief Attempts to set the system-wide locale
 *
 * Tries to change the system's current locale settings to the specified locale.
 * May require administrative privileges on some platforms.
 *
 * @param locale The locale identifier to set as the system locale
 * @return LocaleError indicating success or the reason for failure
 */
auto setSystemLocale(const std::string& locale) -> LocaleError;

/**
 * @brief Retrieves a list of all available locales on the system
 *
 * Returns a collection of locale identifiers that are installed and
 * available for use on the current system.
 *
 * @return A vector of locale identifier strings
 */
auto getAvailableLocales() -> std::vector<std::string>;

/**
 * @brief Gets the system's default locale
 *
 * Retrieves the identifier of the default locale configured for the system.
 *
 * @return A string containing the default locale identifier
 */
auto getDefaultLocale() -> std::string;

/**
 * @brief Retrieves cached locale information
 *
 * Returns the cached LocaleInfo to avoid repeated system calls.
 * The information is refreshed if the cache timeout has expired.
 *
 * @return A reference to the cached LocaleInfo instance
 */
auto getCachedLocaleInfo() -> const LocaleInfo&;

/**
 * @brief Clears the locale information cache
 *
 * Forces the cache to be invalidated, ensuring that the next call to
 * getCachedLocaleInfo will retrieve fresh data from the system.
 */
void clearLocaleCache();

}  // namespace atom::system

#endif  // ATOM_SYSINFO_LOCALE_HPP