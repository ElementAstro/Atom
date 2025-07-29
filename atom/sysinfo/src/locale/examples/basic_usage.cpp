/**
 * @file basic_usage.cpp
 * @brief Basic usage example for the locale module
 *
 * This example demonstrates the basic functionality of the locale module
 * including getting system locale information, formatting numbers and dates,
 * and working with available locales.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "../locale.hpp"
#include <iostream>
#include <chrono>
#include <vector>

using namespace atom::system;

void demonstrateBasicLocaleInfo() {
    std::cout << "=== Basic Locale Information ===" << std::endl;

    // Get current system locale
    auto localeInfo = getSystemLanguageInfo();

    std::cout << "Current System Locale Information:" << std::endl;
    std::cout << "  Locale Name: " << localeInfo.localeName << std::endl;
    std::cout << "  Language Code: " << localeInfo.languageCode << std::endl;
    std::cout << "  Country Code: " << localeInfo.countryCode << std::endl;
    std::cout << "  Language Display Name: " << localeInfo.languageDisplayName << std::endl;
    std::cout << "  Country Display Name: " << localeInfo.countryDisplayName << std::endl;
    std::cout << "  Currency Symbol: " << localeInfo.currencySymbol << std::endl;
    std::cout << "  Currency Code: " << localeInfo.currencyCode << std::endl;
    std::cout << "  Decimal Symbol: " << localeInfo.decimalSymbol << std::endl;
    std::cout << "  Thousands Separator: " << localeInfo.thousandSeparator << std::endl;
    std::cout << "  Character Encoding: " << localeInfo.characterEncoding << std::endl;
    std::cout << "  Is RTL: " << (localeInfo.isRTL ? "Yes" : "No") << std::endl;
    std::cout << "  Time Zone: " << localeInfo.timeZone << std::endl;
    std::cout << "  First Day of Week: " << localeInfo.firstDayOfWeek << std::endl;
    std::cout << std::endl;
}

void demonstrateLocaleManager() {
    std::cout << "=== Locale Manager ===" << std::endl;

    LocaleManager manager;

    // Get available locales
    auto availableLocales = manager.getAvailableLocales();
    std::cout << "Available Locales (" << availableLocales.size() << " total):" << std::endl;

    // Show first 10 locales
    size_t count = 0;
    for (const auto& locale : availableLocales) {
        if (count >= 10) {
            std::cout << "  ... and " << (availableLocales.size() - 10) << " more" << std::endl;
            break;
        }
        std::cout << "  " << locale << std::endl;
        count++;
    }

    // Get default locale
    auto defaultLocale = manager.getDefaultLocale();
    std::cout << "Default Locale: " << defaultLocale << std::endl;

    // Get preferred languages
    auto preferredLanguages = manager.getPreferredLanguages();
    std::cout << "Preferred Languages:" << std::endl;
    for (const auto& lang : preferredLanguages) {
        std::cout << "  " << lang << std::endl;
    }

    std::cout << std::endl;
}

void demonstrateValidation() {
    std::cout << "=== Locale Validation ===" << std::endl;

    LocaleManager manager;

    // Test various locales
    std::vector<std::string> testLocales = {
        "en_US.UTF-8",
        "de_DE.UTF-8",
        "fr_FR.UTF-8",
        "invalid_locale",
        "C",
        "POSIX"
    };

    for (const auto& locale : testLocales) {
        bool isValid = manager.validateLocale(locale);
        std::cout << "  " << locale << ": " << (isValid ? "Valid" : "Invalid") << std::endl;
    }

    std::cout << std::endl;
}

void demonstrateBasicFormatting() {
    std::cout << "=== Basic Formatting ===" << std::endl;

    // Try different locales for formatting
    std::vector<std::string> locales = {"en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8"};

    for (const auto& locale : locales) {
        std::cout << "Formatting with locale: " << locale << std::endl;

        try {
            LocaleFormatter formatter(locale);

            // Number formatting
            double number = 1234567.89;
            std::cout << "  Number: " << formatter.formatNumber(number, 2) << std::endl;

            // Currency formatting
            double amount = 1234.56;
            std::cout << "  Currency: " << formatter.formatCurrency(amount) << std::endl;

            // Percentage formatting
            double percentage = 0.1234;
            std::cout << "  Percentage: " << formatter.formatPercentage(percentage, 2) << std::endl;

            // Date formatting
            auto now = std::chrono::system_clock::now();
            std::cout << "  Date: " << formatter.formatDate(now) << std::endl;
            std::cout << "  Time: " << formatter.formatTime(now) << std::endl;

            // List formatting
            std::vector<std::string> items = {"apple", "banana", "cherry", "date"};
            std::cout << "  List: " << formatter.formatList(items) << std::endl;

        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }

        std::cout << std::endl;
    }
}

void demonstrateLocaleDetection() {
    std::cout << "=== Locale Detection ===" << std::endl;

    // Detect system locale
    auto detected = LocaleDetector::detectSystemLocale();
    std::cout << "Detected System Locale: " << detected << std::endl;

    // Detect from environment
    auto envLocale = LocaleDetector::detectFromEnvironment();
    std::cout << "Environment Locale: " << envLocale << std::endl;

    // Test best match finding
    std::vector<std::string> available = {
        "en_US.UTF-8", "en_GB.UTF-8", "de_DE.UTF-8",
        "fr_FR.UTF-8", "es_ES.UTF-8", "C"
    };

    std::vector<std::string> preferred = {
        "en_CA.UTF-8",  // Should match en_US or en_GB
        "de_AT.UTF-8",  // Should match de_DE
        "zh_CN.UTF-8",  // Should fallback to C or en_US
        "invalid"       // Should fallback to first available
    };

    for (const auto& pref : preferred) {
        auto best = LocaleDetector::findBestMatch(pref, available);
        auto confidence = LocaleDetector::getConfidenceScore(best);
        std::cout << "Preferred: " << pref << " -> Best: " << best
                  << " (confidence: " << confidence << ")" << std::endl;
    }

    std::cout << std::endl;
}

void demonstrateCallbacks() {
    std::cout << "=== Locale Change Callbacks ===" << std::endl;

    LocaleManager manager;

    // Register a callback for locale changes
    auto callbackId = manager.registerChangeCallback(
        [](const std::string& oldLocale, const std::string& newLocale) {
            std::cout << "Locale changed from '" << oldLocale
                      << "' to '" << newLocale << "'" << std::endl;
        }
    );

    std::cout << "Registered callback with ID: " << callbackId << std::endl;

    // Try to change locale (this might not work on all systems)
    auto currentLocale = manager.getCurrentLocale();
    std::cout << "Current locale: " << currentLocale.localeName << std::endl;

    // Attempt to set a different locale
    auto result = manager.setLocale("C");
    if (result == LocaleError::None) {
        std::cout << "Successfully changed locale to C" << std::endl;
    } else {
        std::cout << "Failed to change locale: " << static_cast<int>(result) << std::endl;
    }

    // Unregister the callback
    manager.unregisterChangeCallback(callbackId);
    std::cout << "Unregistered callback" << std::endl;

    std::cout << std::endl;
}

int main() {
    std::cout << "Locale Module - Basic Usage Example" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    try {
        demonstrateBasicLocaleInfo();
        demonstrateLocaleManager();
        demonstrateValidation();
        demonstrateBasicFormatting();
        demonstrateLocaleDetection();
        demonstrateCallbacks();

        std::cout << "Example completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
