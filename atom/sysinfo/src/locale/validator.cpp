#include "validator.hpp"
#include "locale.hpp"
#include <algorithm>
#include <chrono>
#include <regex>
#include <sstream>
#include <spdlog/spdlog.h>

namespace atom::system::locale {

namespace {
    // Common portable locales across platforms
    const std::vector<std::string> PORTABLE_LOCALES = {
        "C", "POSIX", "en_US.UTF-8", "en_GB.UTF-8", "de_DE.UTF-8", 
        "fr_FR.UTF-8", "es_ES.UTF-8", "it_IT.UTF-8", "ja_JP.UTF-8",
        "ko_KR.UTF-8", "zh_CN.UTF-8", "ru_RU.UTF-8"
    };
    
    // Platform-specific locale patterns
    const std::unordered_map<std::string, std::vector<std::string>> PLATFORM_PATTERNS = {
        {"windows", {"en-US", "de-DE", "fr-FR", "es-ES", "it-IT", "ja-JP", "ko-KR", "zh-CN", "ru-RU"}},
        {"linux", {"en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "es_ES.UTF-8", "it_IT.UTF-8", 
                   "ja_JP.UTF-8", "ko_KR.UTF-8", "zh_CN.UTF-8", "ru_RU.UTF-8"}},
        {"macos", {"en_US", "de_DE", "fr_FR", "es_ES", "it_IT", "ja_JP", "ko_KR", "zh_CN", "ru_RU"}}
    };
}

auto LocaleValidator::validate(const std::string& locale, ValidationLevel level) -> ValidationResult {
    ValidationResult result;
    result.level = level;
    
    if (locale.empty()) {
        result.errors.push_back("Locale string is empty");
        return result;
    }
    
    // Basic format validation
    auto formatResult = validateFormat(locale);
    result.errors.insert(result.errors.end(), formatResult.errors.begin(), formatResult.errors.end());
    result.warnings.insert(result.warnings.end(), formatResult.warnings.begin(), formatResult.warnings.end());
    
    if (level == ValidationLevel::Basic) {
        result.isValid = formatResult.isValid;
        result.confidenceScore = formatResult.confidenceScore;
        return result;
    }
    
    // Standard validation includes availability check
    if (level >= ValidationLevel::Standard) {
        auto availabilityResult = validateAvailability(locale);
        if (!availabilityResult.isValid) {
            result.errors.insert(result.errors.end(), availabilityResult.errors.begin(), availabilityResult.errors.end());
        }
        result.warnings.insert(result.warnings.end(), availabilityResult.warnings.begin(), availabilityResult.warnings.end());
    }
    
    // Strict validation includes functionality check
    if (level >= ValidationLevel::Strict) {
        auto functionalityResult = validateFunctionality(locale);
        if (!functionalityResult.isValid) {
            result.errors.insert(result.errors.end(), functionalityResult.errors.begin(), functionalityResult.errors.end());
        }
        result.warnings.insert(result.warnings.end(), functionalityResult.warnings.begin(), functionalityResult.warnings.end());
    }
    
    // Complete validation includes compatibility check
    if (level == ValidationLevel::Complete) {
        auto compatibilityResult = validateCompatibility(locale);
        if (!compatibilityResult.isValid) {
            result.warnings.insert(result.warnings.end(), compatibilityResult.errors.begin(), compatibilityResult.errors.end());
        }
        result.warnings.insert(result.warnings.end(), compatibilityResult.warnings.begin(), compatibilityResult.warnings.end());
    }
    
    result.isValid = result.errors.empty();
    
    // Calculate confidence score
    double baseScore = formatResult.confidenceScore;
    if (result.isValid) {
        baseScore += 0.3; // Bonus for being valid
    }
    if (result.warnings.empty()) {
        baseScore += 0.2; // Bonus for no warnings
    }
    result.confidenceScore = std::min(1.0, baseScore);
    
    // Add suggestions if not valid
    if (!result.isValid) {
        result.suggestions = getSuggestions(locale, 3);
    }
    
    return result;
}

auto LocaleValidator::validateFormat(const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Basic;
    
    if (locale.empty()) {
        result.errors.push_back("Empty locale string");
        return result;
    }
    
    // Check basic format using regex
    std::regex localeRegex(R"(^([a-z]{2,3})(?:_([A-Z]{2}))?(?:\.([^@]+))?(?:@(.+))?$)");
    if (!std::regex_match(locale, localeRegex)) {
        // Check for special cases like "C" and "POSIX"
        if (locale != "C" && locale != "POSIX") {
            result.errors.push_back("Invalid locale format: " + locale);
            result.suggestions.push_back("Expected format: language[_COUNTRY][.encoding][@variant]");
            return result;
        }
    }
    
    // Parse and validate components
    auto info = parseLocaleString(locale);
    
    // Validate language code
    if (info.languageCode.length() < 2 || info.languageCode.length() > 3) {
        result.warnings.push_back("Unusual language code length: " + info.languageCode);
    }
    
    // Validate country code
    if (!info.countryCode.empty() && info.countryCode.length() != 2) {
        result.warnings.push_back("Invalid country code length: " + info.countryCode);
    }
    
    // Check for common encoding
    if (!info.characterEncoding.empty()) {
        std::vector<std::string> commonEncodings = {"UTF-8", "utf8", "ISO-8859-1", "ASCII"};
        if (std::find(commonEncodings.begin(), commonEncodings.end(), info.characterEncoding) == commonEncodings.end()) {
            result.warnings.push_back("Uncommon character encoding: " + info.characterEncoding);
        }
    }
    
    result.isValid = result.errors.empty();
    result.confidenceScore = result.isValid ? (result.warnings.empty() ? 1.0 : 0.8) : 0.2;
    
    return result;
}

auto LocaleValidator::validateAvailability(const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Standard;
    
    try {
        atom::system::LocaleManager manager;
        if (!manager.validateLocale(locale)) {
            result.errors.push_back("Locale not available on system: " + locale);
            
            // Check if it's in available locales list
            auto available = manager.getAvailableLocales();
            if (std::find(available.begin(), available.end(), locale) == available.end()) {
                result.errors.push_back("Locale not found in system locale list");
            }
        } else {
            result.isValid = true;
            result.confidenceScore = 1.0;
        }
    } catch (const std::exception& e) {
        result.errors.push_back("Error checking locale availability: " + std::string(e.what()));
    }
    
    return result;
}

auto LocaleValidator::validateFunctionality(const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Strict;
    
    try {
        // Test basic functionality
        atom::system::LocaleFormatter formatter(locale);
        
        // Test number formatting
        std::string numberTest = formatter.formatNumber(1234.56, 2);
        if (numberTest.empty()) {
            result.errors.push_back("Number formatting failed");
        }
        
        // Test currency formatting
        std::string currencyTest = formatter.formatCurrency(123.45);
        if (currencyTest.empty()) {
            result.warnings.push_back("Currency formatting may not work properly");
        }
        
        // Test date formatting
        auto now = std::chrono::system_clock::now();
        std::string dateTest = formatter.formatDate(now);
        if (dateTest.empty()) {
            result.warnings.push_back("Date formatting may not work properly");
        }
        
        result.isValid = result.errors.empty();
        result.confidenceScore = result.isValid ? (result.warnings.empty() ? 1.0 : 0.7) : 0.3;
        
    } catch (const std::exception& e) {
        result.errors.push_back("Functionality test failed: " + std::string(e.what()));
    }
    
    return result;
}

auto LocaleValidator::validateCompatibility(const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Complete;
    
    // Check if locale is in portable list
    if (std::find(PORTABLE_LOCALES.begin(), PORTABLE_LOCALES.end(), locale) != PORTABLE_LOCALES.end()) {
        result.isValid = true;
        result.confidenceScore = 1.0;
        result.metadata["portability"] = "high";
    } else {
        result.warnings.push_back("Locale may not be portable across all platforms");
        result.confidenceScore = 0.6;
        result.metadata["portability"] = "medium";
    }
    
    // Check encoding compatibility
    auto info = parseLocaleString(locale);
    if (info.characterEncoding == "UTF-8" || info.characterEncoding == "utf8") {
        result.metadata["encoding_compatibility"] = "excellent";
    } else if (info.characterEncoding.empty()) {
        result.warnings.push_back("No explicit encoding specified");
        result.metadata["encoding_compatibility"] = "unknown";
    } else {
        result.warnings.push_back("Non-UTF-8 encoding may cause compatibility issues");
        result.metadata["encoding_compatibility"] = "limited";
    }
    
    return result;
}

auto LocaleValidator::checkCompatibility(const std::string& locale1, const std::string& locale2) -> double {
    if (locale1 == locale2) return 1.0;
    
    auto info1 = parseLocaleString(locale1);
    auto info2 = parseLocaleString(locale2);
    
    double score = 0.0;
    
    // Language compatibility
    if (info1.languageCode == info2.languageCode) {
        score += 0.5;
        
        // Country compatibility
        if (info1.countryCode == info2.countryCode) {
            score += 0.3;
        } else if (info1.countryCode.empty() || info2.countryCode.empty()) {
            score += 0.1; // Partial compatibility
        }
        
        // Encoding compatibility
        if (info1.characterEncoding == info2.characterEncoding) {
            score += 0.2;
        } else if ((info1.characterEncoding == "UTF-8" || info1.characterEncoding == "utf8") &&
                   (info2.characterEncoding == "UTF-8" || info2.characterEncoding == "utf8")) {
            score += 0.2;
        } else if (info1.characterEncoding.empty() || info2.characterEncoding.empty()) {
            score += 0.1;
        }
    }
    
    return std::min(1.0, score);
}

auto LocaleValidator::getSuggestions(const std::string& locale, size_t maxSuggestions) -> std::vector<std::string> {
    std::vector<std::string> suggestions;
    
    if (locale.empty()) {
        suggestions = {"en_US.UTF-8", "C", "POSIX"};
        suggestions.resize(std::min(suggestions.size(), maxSuggestions));
        return suggestions;
    }
    
    auto info = parseLocaleString(locale);
    
    // Try to get available locales
    try {
        atom::system::LocaleManager manager;
        auto available = manager.getAvailableLocales();
        
        // Find similar locales
        std::vector<std::pair<std::string, double>> scored;
        for (const auto& availableLocale : available) {
            double score = checkCompatibility(locale, availableLocale);
            if (score > 0.3) { // Only suggest reasonably compatible locales
                scored.emplace_back(availableLocale, score);
            }
        }
        
        // Sort by score
        std::sort(scored.begin(), scored.end(), 
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Extract top suggestions
        for (const auto& [suggestedLocale, score] : scored) {
            if (suggestions.size() >= maxSuggestions) break;
            suggestions.push_back(suggestedLocale);
        }
        
    } catch (...) {
        // Fallback suggestions
        if (!info.languageCode.empty()) {
            suggestions.push_back(info.languageCode + "_" + info.languageCode + ".UTF-8");
        }
    }
    
    // Add common fallbacks if we don't have enough suggestions
    if (suggestions.size() < maxSuggestions) {
        std::vector<std::string> fallbacks = {"en_US.UTF-8", "C", "POSIX"};
        for (const auto& fallback : fallbacks) {
            if (suggestions.size() >= maxSuggestions) break;
            if (std::find(suggestions.begin(), suggestions.end(), fallback) == suggestions.end()) {
                suggestions.push_back(fallback);
            }
        }
    }
    
    suggestions.resize(std::min(suggestions.size(), maxSuggestions));
    return suggestions;
}

auto LocaleValidator::findBestValidLocale(const std::vector<std::string>& locales, ValidationLevel level) -> std::string {
    std::vector<std::pair<std::string, double>> scored;
    
    for (const auto& locale : locales) {
        auto result = validate(locale, level);
        if (result.isValid) {
            scored.emplace_back(locale, result.confidenceScore);
        }
    }
    
    if (scored.empty()) {
        return "";
    }
    
    // Sort by confidence score
    std::sort(scored.begin(), scored.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return scored[0].first;
}

auto LocaleValidator::runSystemTests() -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Complete;

    try {
        atom::system::LocaleManager manager;

        // Test getting current locale
        auto currentLocale = manager.getCurrentLocale();
        if (currentLocale.localeName.empty()) {
            result.errors.push_back("Failed to get current locale");
        } else {
            result.metadata["current_locale"] = currentLocale.localeName;
        }

        // Test getting available locales
        auto available = manager.getAvailableLocales();
        if (available.empty()) {
            result.errors.push_back("No available locales found");
        } else {
            result.metadata["available_count"] = std::to_string(available.size());
        }

        // Test default locale
        auto defaultLocale = manager.getDefaultLocale();
        if (defaultLocale.empty()) {
            result.warnings.push_back("No default locale found");
        } else {
            result.metadata["default_locale"] = defaultLocale;
        }

        result.isValid = result.errors.empty();
        result.confidenceScore = result.isValid ? (result.warnings.empty() ? 1.0 : 0.8) : 0.3;

    } catch (const std::exception& e) {
        result.errors.push_back("System test failed: " + std::string(e.what()));
    }

    return result;
}

auto LocaleValidator::testFormatting(const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Strict;

    try {
        atom::system::LocaleFormatter formatter(locale);

        // Test various formatting operations
        std::vector<std::string> testResults;

        // Number formatting tests
        testResults.push_back(formatter.formatNumber(1234.56, 2));
        testResults.push_back(formatter.formatNumber(-9876.54, 3));
        testResults.push_back(formatter.formatPercentage(0.1234, 2));

        // Currency formatting tests
        testResults.push_back(formatter.formatCurrency(123.45));
        testResults.push_back(formatter.formatCurrency(-67.89));

        // Date/time formatting tests
        auto now = std::chrono::system_clock::now();
        testResults.push_back(formatter.formatDate(now));
        testResults.push_back(formatter.formatTime(now));
        testResults.push_back(formatter.formatDateTime(now));

        // List formatting test
        std::vector<std::string> items = {"apple", "banana", "cherry"};
        testResults.push_back(formatter.formatList(items));

        // Check if any formatting failed
        size_t failedTests = 0;
        for (const auto& testResult : testResults) {
            if (testResult.empty()) {
                failedTests++;
            }
        }

        if (failedTests == 0) {
            result.isValid = true;
            result.confidenceScore = 1.0;
            result.metadata["formatting_tests_passed"] = std::to_string(testResults.size());
        } else {
            result.warnings.push_back("Some formatting tests failed: " + std::to_string(failedTests) + "/" + std::to_string(testResults.size()));
            result.confidenceScore = 1.0 - (static_cast<double>(failedTests) / testResults.size());
        }

    } catch (const std::exception& e) {
        result.errors.push_back("Formatting test failed: " + std::string(e.what()));
    }

    return result;
}

auto LocaleValidator::testCollation([[maybe_unused]] const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Strict;

    // This is a simplified collation test
    // In a real implementation, you would test locale-specific sorting
    try {
        std::vector<std::string> testStrings = {"apple", "Banana", "cherry", "Dog"};
        std::vector<std::string> sorted = testStrings;

        // Simple sort test (would use locale-specific collation in real implementation)
        std::sort(sorted.begin(), sorted.end());

        result.isValid = true;
        result.confidenceScore = 0.8; // Lower confidence as this is a simplified test
        result.metadata["collation_test"] = "basic_sort_completed";

    } catch (const std::exception& e) {
        result.errors.push_back("Collation test failed: " + std::string(e.what()));
    }

    return result;
}

auto LocaleValidator::testCharacterClassification([[maybe_unused]] const std::string& locale) -> ValidationResult {
    ValidationResult result;
    result.level = ValidationLevel::Strict;

    // This is a simplified character classification test
    try {
        // Test basic character classification
        std::string testChars = "aA1!αΑ";
        bool hasLower = false, hasUpper = false, hasDigit = false, hasPunct = false, hasAlpha = false;

        for (char c : testChars) {
            if (std::islower(c)) hasLower = true;
            if (std::isupper(c)) hasUpper = true;
            if (std::isdigit(c)) hasDigit = true;
            if (std::ispunct(c)) hasPunct = true;
            if (std::isalpha(c)) hasAlpha = true;
        }

        if (hasLower && hasUpper && hasDigit && hasPunct && hasAlpha) {
            result.isValid = true;
            result.confidenceScore = 0.9;
            result.metadata["character_classification"] = "all_categories_detected";
        } else {
            result.warnings.push_back("Some character categories not properly classified");
            result.confidenceScore = 0.6;
        }

    } catch (const std::exception& e) {
        result.errors.push_back("Character classification test failed: " + std::string(e.what()));
    }

    return result;
}

auto LocaleValidator::benchmarkPerformance(const std::string& locale, size_t iterations) -> std::unordered_map<std::string, double> {
    std::unordered_map<std::string, double> results;

    try {
        atom::system::LocaleFormatter formatter(locale);

        // Benchmark number formatting
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            formatter.formatNumber(1234.56 + i, 2);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results["number_formatting_us"] = static_cast<double>(duration.count()) / iterations;

        // Benchmark currency formatting
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            formatter.formatCurrency(123.45 + i);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results["currency_formatting_us"] = static_cast<double>(duration.count()) / iterations;

        // Benchmark date formatting
        auto now = std::chrono::system_clock::now();
        start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            formatter.formatDate(now);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        results["date_formatting_us"] = static_cast<double>(duration.count()) / iterations;

    } catch (const std::exception& e) {
        spdlog::error("Performance benchmark failed: {}", e.what());
    }

    return results;
}

} // namespace atom::system::locale
