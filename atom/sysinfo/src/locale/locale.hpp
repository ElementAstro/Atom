/**
 * @file locale.hpp
 * @brief Enhanced system locale information functionality
 *
 * This file contains the main API for the enhanced locale module with
 * platform-specific implementations and advanced features.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_LOCALE_HPP
#define ATOM_SYSINFO_LOCALE_LOCALE_HPP

#include "common.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace atom::system {

// Re-export common types for backward compatibility
using locale::LocaleError;
using locale::LocaleInfo;
using locale::LocalePreferences;
using locale::MeasurementSystem;
using locale::PaperSize;
using locale::LocaleCategory;

/**
 * @class LocaleManager
 * @brief Advanced locale management with caching and callbacks
 */
class LocaleManager {
public:
    /**
     * @brief Constructor
     */
    LocaleManager();
    
    /**
     * @brief Destructor
     */
    ~LocaleManager();
    
    /**
     * @brief Gets the current system locale information
     * @param useCache Whether to use cached information
     * @return LocaleInfo structure with current locale data
     */
    auto getCurrentLocale(bool useCache = true) -> LocaleInfo;
    
    /**
     * @brief Sets the system locale
     * @param locale The locale identifier to set
     * @return LocaleError indicating success or failure
     */
    auto setLocale(const std::string& locale) -> LocaleError;
    
    /**
     * @brief Gets all available locales on the system
     * @param useCache Whether to use cached list
     * @return Vector of available locale identifiers
     */
    auto getAvailableLocales(bool useCache = true) -> std::vector<std::string>;
    
    /**
     * @brief Validates if a locale is available
     * @param locale The locale identifier to validate
     * @return true if valid and available, false otherwise
     */
    auto validateLocale(const std::string& locale) -> bool;
    
    /**
     * @brief Gets the default system locale
     * @return Default locale identifier
     */
    auto getDefaultLocale() -> std::string;
    
    /**
     * @brief Gets user's preferred languages
     * @return Vector of preferred language identifiers
     */
    auto getPreferredLanguages() -> std::vector<std::string>;
    
    /**
     * @brief Clears all cached locale information
     */
    void clearCache();
    
    /**
     * @brief Sets cache timeout duration
     * @param timeout Cache timeout in seconds
     */
    void setCacheTimeout(std::chrono::seconds timeout);
    
    /**
     * @brief Registers a callback for locale changes
     * @param callback The callback function
     * @return Registration ID for later removal
     */
    auto registerChangeCallback(std::function<void(const std::string&, const std::string&)> callback) -> size_t;
    
    /**
     * @brief Unregisters a locale change callback
     * @param id The registration ID
     */
    void unregisterChangeCallback(size_t id);
    
    /**
     * @brief Gets locale preferences
     * @return Current locale preferences
     */
    auto getPreferences() -> LocalePreferences;
    
    /**
     * @brief Sets locale preferences
     * @param preferences The preferences to set
     */
    void setPreferences(const LocalePreferences& preferences);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @class LocaleFormatter
 * @brief Advanced locale-aware formatting utilities
 */
class LocaleFormatter {
public:
    /**
     * @brief Constructor
     * @param locale The locale to use for formatting
     */
    explicit LocaleFormatter(const std::string& locale = "");
    
    /**
     * @brief Destructor
     */
    ~LocaleFormatter();
    
    /**
     * @brief Sets the locale for formatting
     * @param locale The locale identifier
     */
    void setLocale(const std::string& locale);
    
    /**
     * @brief Gets the current locale
     * @return Current locale identifier
     */
    auto getLocale() const -> std::string;
    
    /**
     * @brief Formats a number according to locale settings
     * @param number The number to format
     * @param precision Number of decimal places
     * @return Formatted number string
     */
    auto formatNumber(double number, int precision = -1) -> std::string;
    
    /**
     * @brief Formats currency according to locale settings
     * @param amount The amount to format
     * @param currencyCode Optional currency code override
     * @return Formatted currency string
     */
    auto formatCurrency(double amount, const std::string& currencyCode = "") -> std::string;
    
    /**
     * @brief Formats percentage according to locale settings
     * @param value The percentage value (0.0-1.0)
     * @param precision Number of decimal places
     * @return Formatted percentage string
     */
    auto formatPercentage(double value, int precision = 2) -> std::string;
    
    /**
     * @brief Formats date according to locale settings
     * @param timestamp The timestamp to format
     * @param style Format style (short, medium, long, full)
     * @return Formatted date string
     */
    auto formatDate(const std::chrono::system_clock::time_point& timestamp, 
                    const std::string& style = "short") -> std::string;
    
    /**
     * @brief Formats time according to locale settings
     * @param timestamp The timestamp to format
     * @param style Format style (short, medium, long, full)
     * @return Formatted time string
     */
    auto formatTime(const std::chrono::system_clock::time_point& timestamp, 
                    const std::string& style = "short") -> std::string;
    
    /**
     * @brief Formats date and time according to locale settings
     * @param timestamp The timestamp to format
     * @param dateStyle Date format style
     * @param timeStyle Time format style
     * @return Formatted date-time string
     */
    auto formatDateTime(const std::chrono::system_clock::time_point& timestamp, 
                        const std::string& dateStyle = "short",
                        const std::string& timeStyle = "short") -> std::string;
    
    /**
     * @brief Formats a list of items according to locale settings
     * @param items The items to format
     * @return Formatted list string
     */
    auto formatList(const std::vector<std::string>& items) -> std::string;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @class LocaleDetector
 * @brief Automatic locale detection utilities
 */
class LocaleDetector {
public:
    /**
     * @brief Detects the best locale based on system settings
     * @param fallback Fallback locale if detection fails
     * @return Detected locale identifier
     */
    static auto detectSystemLocale(const std::string& fallback = "en_US.UTF-8") -> std::string;
    
    /**
     * @brief Detects locale from environment variables
     * @return Detected locale from environment
     */
    static auto detectFromEnvironment() -> std::string;
    
    /**
     * @brief Detects locale from system configuration files
     * @return Detected locale from system config
     */
    static auto detectFromSystemConfig() -> std::string;
    
    /**
     * @brief Detects the best matching locale from available options
     * @param preferred Preferred locale identifier
     * @param available Available locale options
     * @return Best matching locale
     */
    static auto findBestMatch(const std::string& preferred, 
                              const std::vector<std::string>& available) -> std::string;
    
    /**
     * @brief Gets locale confidence score
     * @param locale The locale to score
     * @return Confidence score (0.0-1.0)
     */
    static auto getConfidenceScore(const std::string& locale) -> double;
};

// Backward compatibility functions
/**
 * @brief Retrieves the current system language and locale information
 * @return LocaleInfo containing the system's current locale settings
 */
auto getSystemLanguageInfo() -> LocaleInfo;

/**
 * @brief Displays locale information in a formatted manner
 * @param info The LocaleInfo instance to display
 */
void printLocaleInfo(const LocaleInfo& info);

/**
 * @brief Validates if a locale identifier is valid and available on the system
 * @param locale The locale identifier to validate
 * @return true if the locale is valid and available, false otherwise
 */
auto validateLocale(const std::string& locale) -> bool;

/**
 * @brief Attempts to set the system-wide locale
 * @param locale The locale identifier to set as the system locale
 * @return LocaleError indicating success or the reason for failure
 */
auto setSystemLocale(const std::string& locale) -> LocaleError;

/**
 * @brief Retrieves a list of all available locales on the system
 * @param useCache Whether to use cached results
 * @return A vector of locale identifier strings
 */
auto getAvailableLocales(bool useCache = true) -> std::vector<std::string>;

/**
 * @brief Gets the system's default locale
 * @return A string containing the default locale identifier
 */
auto getDefaultLocale() -> std::string;

/**
 * @brief Retrieves cached locale information
 * @return A reference to the cached LocaleInfo instance
 */
auto getCachedLocaleInfo() -> const LocaleInfo&;

/**
 * @brief Clears the locale information cache
 */
void clearLocaleCache();

} // namespace atom::system

#endif // ATOM_SYSINFO_LOCALE_LOCALE_HPP
