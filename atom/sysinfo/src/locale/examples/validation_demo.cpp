/**
 * @file validation_demo.cpp
 * @brief Validation and testing demonstration for the locale module
 *
 * This example demonstrates the comprehensive validation capabilities
 * including different validation levels, error reporting, suggestions,
 * compatibility checking, and performance testing.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "../validator.hpp"
#include "../locale.hpp"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace atom::system::locale;

void demonstrateValidationLevels() {
    std::cout << "=== Validation Levels Demonstration ===" << std::endl;
    
    std::vector<std::string> testLocales = {
        "en_US.UTF-8",      // Valid, common locale
        "de_DE.UTF-8",      // Valid, common locale
        "invalid_locale",   // Invalid format
        "xx_YY.UTF-8",      // Valid format, but non-existent
        "C",                // Special case
        "POSIX",            // Special case
        "en_US",            // Valid but no encoding
        "en",               // Language only
        ""                  // Empty string
    };
    
    std::vector<ValidationLevel> levels = {
        ValidationLevel::Basic,
        ValidationLevel::Standard,
        ValidationLevel::Strict,
        ValidationLevel::Complete
    };
    
    const char* levelNames[] = {"Basic", "Standard", "Strict", "Complete"};
    
    for (size_t i = 0; i < levels.size(); ++i) {
        std::cout << "Validation Level: " << levelNames[i] << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        for (const auto& locale : testLocales) {
            auto result = LocaleValidator::validate(locale, levels[i]);
            
            std::cout << "Locale: " << std::setw(15) << (locale.empty() ? "(empty)" : locale)
                      << " | Valid: " << std::setw(5) << (result.isValid ? "Yes" : "No")
                      << " | Confidence: " << std::setw(4) << std::fixed << std::setprecision(2) 
                      << result.confidenceScore << std::endl;
            
            if (!result.errors.empty()) {
                std::cout << "  Errors:" << std::endl;
                for (const auto& error : result.errors) {
                    std::cout << "    - " << error << std::endl;
                }
            }
            
            if (!result.warnings.empty()) {
                std::cout << "  Warnings:" << std::endl;
                for (const auto& warning : result.warnings) {
                    std::cout << "    - " << warning << std::endl;
                }
            }
            
            if (!result.suggestions.empty()) {
                std::cout << "  Suggestions:" << std::endl;
                for (const auto& suggestion : result.suggestions) {
                    std::cout << "    - " << suggestion << std::endl;
                }
            }
            
            std::cout << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateFormatValidation() {
    std::cout << "=== Format Validation Demonstration ===" << std::endl;
    
    std::vector<std::string> formatTests = {
        "en_US.UTF-8",          // Perfect format
        "en_US",                // No encoding
        "en",                   // Language only
        "en_US.ISO-8859-1",     // Different encoding
        "en_US@euro",           // With variant
        "en_US.UTF-8@euro",     // Full format with variant
        "C",                    // Special case
        "POSIX",                // Special case
        "en-US",                // Wrong separator
        "EN_us",                // Wrong case
        "english_US",           // Invalid language code
        "en_USA",               // Invalid country code
        "123_45",               // Numeric codes
        "en_US.UTF-8.extra",    // Too many parts
        "en_",                  // Incomplete
        "_US",                  // Missing language
        ".UTF-8",               // Missing locale
        "@variant",             // Missing locale
        "en US",                // Space instead of underscore
        ""                      // Empty
    };
    
    for (const auto& locale : formatTests) {
        auto result = LocaleValidator::validateFormat(locale);
        
        std::cout << "Format: " << std::setw(20) << (locale.empty() ? "(empty)" : locale)
                  << " | Valid: " << std::setw(5) << (result.isValid ? "Yes" : "No")
                  << " | Score: " << std::setw(4) << std::fixed << std::setprecision(2) 
                  << result.confidenceScore;
        
        if (!result.errors.empty()) {
            std::cout << " | Error: " << result.errors[0];
        }
        
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
}

void demonstrateCompatibilityChecking() {
    std::cout << "=== Compatibility Checking Demonstration ===" << std::endl;
    
    std::vector<std::pair<std::string, std::string>> compatibilityTests = {
        {"en_US.UTF-8", "en_US.UTF-8"},     // Identical
        {"en_US.UTF-8", "en_US"},           // Same but different encoding
        {"en_US.UTF-8", "en_GB.UTF-8"},     // Same language, different country
        {"en_US.UTF-8", "de_DE.UTF-8"},     // Different language
        {"en_US.UTF-8", "en_CA.UTF-8"},     // Same language, related country
        {"zh_CN.UTF-8", "zh_TW.UTF-8"},     // Same language, different region
        {"C", "POSIX"},                     // Special cases
        {"en", "en_US"},                    // Language vs full locale
        {"invalid", "en_US.UTF-8"}          // Invalid vs valid
    };
    
    for (const auto& [locale1, locale2] : compatibilityTests) {
        double compatibility = LocaleValidator::checkCompatibility(locale1, locale2);
        
        std::cout << "Compatibility: " << std::setw(15) << locale1 
                  << " <-> " << std::setw(15) << locale2
                  << " | Score: " << std::setw(4) << std::fixed << std::setprecision(2) 
                  << compatibility;
        
        if (compatibility >= 0.8) {
            std::cout << " (High)";
        } else if (compatibility >= 0.5) {
            std::cout << " (Medium)";
        } else if (compatibility > 0.0) {
            std::cout << " (Low)";
        } else {
            std::cout << " (None)";
        }
        
        std::cout << std::endl;
    }
    
    std::cout << std::endl;
}

void demonstrateSuggestions() {
    std::cout << "=== Suggestion System Demonstration ===" << std::endl;
    
    std::vector<std::string> invalidLocales = {
        "invalid_locale",
        "en-US",            // Wrong format
        "english",          // Wrong language code
        "en_USA",           // Wrong country code
        "de_DE.LATIN1",     // Old encoding name
        "fr_FR@EURO",       // Wrong case variant
        "",                 // Empty
        "xx_YY.UTF-8"       // Non-existent but valid format
    };
    
    for (const auto& locale : invalidLocales) {
        std::cout << "Invalid locale: " << (locale.empty() ? "(empty)" : locale) << std::endl;
        
        auto suggestions = LocaleValidator::getSuggestions(locale, 5);
        if (!suggestions.empty()) {
            std::cout << "  Suggestions:" << std::endl;
            for (size_t i = 0; i < suggestions.size(); ++i) {
                std::cout << "    " << (i + 1) << ". " << suggestions[i] << std::endl;
            }
        } else {
            std::cout << "  No suggestions available" << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateBestValidLocale() {
    std::cout << "=== Best Valid Locale Selection ===" << std::endl;
    
    std::vector<std::vector<std::string>> localeLists = {
        {"en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8"},           // All valid
        {"invalid1", "en_US.UTF-8", "invalid2"},                 // Mixed valid/invalid
        {"en-US", "en_US.UTF-8", "en_US"},                       // Different formats
        {"invalid1", "invalid2", "invalid3"},                    // All invalid
        {"C", "POSIX", "en_US.UTF-8"},                           // Special cases
        {}                                                        // Empty list
    };
    
    for (size_t i = 0; i < localeLists.size(); ++i) {
        const auto& locales = localeLists[i];
        
        std::cout << "Test " << (i + 1) << " - Input locales: ";
        if (locales.empty()) {
            std::cout << "(empty list)";
        } else {
            for (size_t j = 0; j < locales.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << locales[j];
            }
        }
        std::cout << std::endl;
        
        auto best = LocaleValidator::findBestValidLocale(locales, ValidationLevel::Standard);
        if (!best.empty()) {
            std::cout << "  Best valid locale: " << best << std::endl;
        } else {
            std::cout << "  No valid locale found" << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateSystemTests() {
    std::cout << "=== System Tests Demonstration ===" << std::endl;
    
    auto systemResult = LocaleValidator::runSystemTests();
    
    std::cout << "System Test Results:" << std::endl;
    std::cout << "  Valid: " << (systemResult.isValid ? "Yes" : "No") << std::endl;
    std::cout << "  Confidence: " << std::fixed << std::setprecision(2) 
              << systemResult.confidenceScore << std::endl;
    
    if (!systemResult.errors.empty()) {
        std::cout << "  Errors:" << std::endl;
        for (const auto& error : systemResult.errors) {
            std::cout << "    - " << error << std::endl;
        }
    }
    
    if (!systemResult.warnings.empty()) {
        std::cout << "  Warnings:" << std::endl;
        for (const auto& warning : systemResult.warnings) {
            std::cout << "    - " << warning << std::endl;
        }
    }
    
    if (!systemResult.metadata.empty()) {
        std::cout << "  Metadata:" << std::endl;
        for (const auto& [key, value] : systemResult.metadata) {
            std::cout << "    " << key << ": " << value << std::endl;
        }
    }
    
    std::cout << std::endl;
}

void demonstratePerformanceBenchmark() {
    std::cout << "=== Performance Benchmark ===" << std::endl;
    
    std::vector<std::string> testLocales = {
        "en_US.UTF-8",
        "de_DE.UTF-8",
        "fr_FR.UTF-8",
        "C"
    };
    
    for (const auto& locale : testLocales) {
        std::cout << "Benchmarking locale: " << locale << std::endl;
        
        auto metrics = LocaleValidator::benchmarkPerformance(locale, 1000);
        
        for (const auto& [operation, timeUs] : metrics) {
            std::cout << "  " << std::setw(20) << operation 
                      << ": " << std::setw(8) << std::fixed << std::setprecision(3) 
                      << timeUs << " μs/op" << std::endl;
        }
        
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "Locale Module - Validation and Testing Demonstration" << std::endl;
    std::cout << "====================================================" << std::endl;
    std::cout << std::endl;
    
    try {
        demonstrateValidationLevels();
        demonstrateFormatValidation();
        demonstrateCompatibilityChecking();
        demonstrateSuggestions();
        demonstrateBestValidLocale();
        demonstrateSystemTests();
        demonstratePerformanceBenchmark();
        
        std::cout << "Validation demonstration completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
