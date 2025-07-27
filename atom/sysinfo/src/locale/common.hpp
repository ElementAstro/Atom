/**
 * @file common.hpp
 * @brief Common utilities and definitions for locale functionality
 *
 * This file contains shared utilities, constants, and helper functions
 * used across different platform-specific locale implementations.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_COMMON_HPP
#define ATOM_SYSINFO_LOCALE_COMMON_HPP

#include <chrono>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>

namespace atom::system::locale {

/**
 * @enum LocaleError
 * @brief Error codes for locale operations
 */
enum class LocaleError {
    None,                   /**< No error occurred */
    InvalidLocale,          /**< The specified locale is invalid or not recognized */
    SystemError,            /**< A system-level error occurred during the operation */
    UnsupportedPlatform,    /**< The operation is not supported on the current platform */
    CacheError,             /**< Error occurred during cache operations */
    FormatError,            /**< Error in locale formatting operations */
    DetectionError          /**< Error in locale detection */
};

/**
 * @enum LocaleCategory
 * @brief Categories of locale settings
 */
enum class LocaleCategory {
    All,            /**< All locale categories */
    Collate,        /**< String collation */
    CType,          /**< Character classification and conversion */
    Messages,       /**< Message catalogs */
    Monetary,       /**< Monetary formatting */
    Numeric,        /**< Numeric formatting */
    Time            /**< Date and time formatting */
};

/**
 * @enum MeasurementSystem
 * @brief Measurement systems
 */
enum class MeasurementSystem {
    Metric,         /**< Metric system (meters, kilograms, etc.) */
    Imperial,       /**< Imperial system (feet, pounds, etc.) */
    US              /**< US customary units */
};

/**
 * @enum PaperSize
 * @brief Standard paper sizes
 */
enum class PaperSize {
    A4,             /**< A4 paper size (210 × 297 mm) */
    Letter,         /**< US Letter (8.5 × 11 inches) */
    Legal,          /**< US Legal (8.5 × 14 inches) */
    A3,             /**< A3 paper size (297 × 420 mm) */
    A5,             /**< A5 paper size (148 × 210 mm) */
    Tabloid         /**< Tabloid (11 × 17 inches) */
};

/**
 * @struct LocaleInfo
 * @brief Comprehensive information about a system locale
 */
struct LocaleInfo {
    std::string languageCode;        /**< ISO 639 language code (e.g., "en") */
    std::string countryCode;         /**< ISO 3166 country code (e.g., "US") */
    std::string localeName;          /**< Full locale name (e.g., "en_US") */
    std::string languageDisplayName; /**< Human-readable language name */
    std::string countryDisplayName;  /**< Human-readable country name */
    std::string currencySymbol;      /**< Currency symbol (e.g., "$") */
    std::string currencyCode;        /**< ISO 4217 currency code (e.g., "USD") */
    std::string decimalSymbol;       /**< Decimal point symbol (e.g., ".") */
    std::string thousandSeparator;   /**< Thousands separator symbol (e.g., ",") */
    std::string dateFormat;          /**< Date format string */
    std::string timeFormat;          /**< Time format string */
    std::string characterEncoding;   /**< Character encoding (e.g., "UTF-8") */
    bool isRTL{false};              /**< Whether text is displayed right-to-left */
    std::string numberFormat;        /**< Number format pattern */
    MeasurementSystem measurementSystem{MeasurementSystem::Metric}; /**< Measurement system */
    PaperSize paperSize{PaperSize::A4}; /**< Default paper size */
    std::chrono::seconds cacheTimeout{300}; /**< Cache timeout duration in seconds */
    
    // Extended properties
    std::string scriptCode;          /**< ISO 15924 script code (e.g., "Latn") */
    std::string variantCode;         /**< Locale variant code */
    std::string timeZone;            /**< Default time zone */
    std::string firstDayOfWeek;      /**< First day of week (e.g., "Monday") */
    std::vector<std::string> weekendDays; /**< Weekend days */
    std::string amPmFormat;          /**< AM/PM format strings */
    std::string listSeparator;       /**< List separator (e.g., ";") */
    
    /**
     * @brief Equality comparison operator
     */
    bool operator==(const LocaleInfo& other) const;
    
    /**
     * @brief Inequality comparison operator
     */
    bool operator!=(const LocaleInfo& other) const;
};

/**
 * @struct LocalePreferences
 * @brief User locale preferences and settings
 */
struct LocalePreferences {
    std::vector<std::string> preferredLanguages; /**< Ordered list of preferred languages */
    std::string preferredCurrency;               /**< Preferred currency code */
    MeasurementSystem preferredMeasurement;      /**< Preferred measurement system */
    PaperSize preferredPaperSize;                /**< Preferred paper size */
    std::string preferredTimeZone;               /**< Preferred time zone */
    bool use24HourFormat{true};                  /**< Use 24-hour time format */
    std::string dateFormatPreference;            /**< Custom date format preference */
    std::string numberFormatPreference;          /**< Custom number format preference */
};

/**
 * @brief Converts LocaleError to string representation
 * @param error The error code to convert
 * @return String representation of the error
 */
auto localeErrorToString(LocaleError error) -> std::string;

/**
 * @brief Converts MeasurementSystem to string representation
 * @param system The measurement system to convert
 * @return String representation of the measurement system
 */
auto measurementSystemToString(MeasurementSystem system) -> std::string;

/**
 * @brief Converts string to MeasurementSystem
 * @param str The string to convert
 * @return Optional MeasurementSystem if conversion successful
 */
auto stringToMeasurementSystem(const std::string& str) -> std::optional<MeasurementSystem>;

/**
 * @brief Converts PaperSize to string representation
 * @param size The paper size to convert
 * @return String representation of the paper size
 */
auto paperSizeToString(PaperSize size) -> std::string;

/**
 * @brief Converts string to PaperSize
 * @param str The string to convert
 * @return Optional PaperSize if conversion successful
 */
auto stringToPaperSize(const std::string& str) -> std::optional<PaperSize>;

/**
 * @brief Parses a locale string into components
 * @param locale The locale string to parse (e.g., "en_US.UTF-8")
 * @return Parsed locale components
 */
auto parseLocaleString(const std::string& locale) -> LocaleInfo;

/**
 * @brief Validates a locale identifier format
 * @param locale The locale identifier to validate
 * @return true if the format is valid, false otherwise
 */
auto isValidLocaleFormat(const std::string& locale) -> bool;

/**
 * @brief Normalizes a locale identifier
 * @param locale The locale identifier to normalize
 * @return Normalized locale identifier
 */
auto normalizeLocaleIdentifier(const std::string& locale) -> std::string;

/**
 * @brief Gets the fallback locale for a given locale
 * @param locale The locale to get fallback for
 * @return Fallback locale identifier
 */
auto getFallbackLocale(const std::string& locale) -> std::string;

/**
 * @brief Checks if two locales are compatible
 * @param locale1 First locale to compare
 * @param locale2 Second locale to compare
 * @return true if locales are compatible, false otherwise
 */
auto areLocalesCompatible(const std::string& locale1, const std::string& locale2) -> bool;

/**
 * @brief Gets common locale aliases
 * @return Map of locale aliases to canonical names
 */
auto getLocaleAliases() -> const std::unordered_map<std::string, std::string>&;

/**
 * @brief Resolves a locale alias to its canonical name
 * @param alias The locale alias to resolve
 * @return Canonical locale name, or original if not an alias
 */
auto resolveLocaleAlias(const std::string& alias) -> std::string;

/**
 * @brief Type alias for locale change callback function
 */
using LocaleChangeCallback = std::function<void(const std::string& oldLocale, const std::string& newLocale)>;

/**
 * @brief Registers a callback for locale changes
 * @param callback The callback function to register
 * @return Registration ID for later removal
 */
auto registerLocaleChangeCallback(LocaleChangeCallback callback) -> size_t;

/**
 * @brief Unregisters a locale change callback
 * @param id The registration ID returned by registerLocaleChangeCallback
 */
void unregisterLocaleChangeCallback(size_t id);

/**
 * @brief Notifies all registered callbacks of a locale change
 * @param oldLocale The previous locale
 * @param newLocale The new locale
 */
void notifyLocaleChange(const std::string& oldLocale, const std::string& newLocale);

} // namespace atom::system::locale

#endif // ATOM_SYSINFO_LOCALE_COMMON_HPP
