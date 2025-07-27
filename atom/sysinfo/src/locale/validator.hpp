/**
 * @file validator.hpp
 * @brief Comprehensive locale validation and testing utilities
 *
 * This file contains advanced validation, testing, and compatibility
 * checking utilities for locale operations.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_VALIDATOR_HPP
#define ATOM_SYSINFO_LOCALE_VALIDATOR_HPP

#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace atom::system::locale {

/**
 * @enum ValidationLevel
 * @brief Levels of locale validation
 */
enum class ValidationLevel {
    Basic,      /**< Basic format validation */
    Standard,   /**< Standard validation with system checks */
    Strict,     /**< Strict validation with full compatibility checks */
    Complete    /**< Complete validation with all features tested */
};

/**
 * @struct ValidationResult
 * @brief Result of locale validation
 */
struct ValidationResult {
    bool isValid{false};                    /**< Whether the locale is valid */
    ValidationLevel level{ValidationLevel::Basic}; /**< Level of validation performed */
    std::vector<std::string> errors;        /**< List of validation errors */
    std::vector<std::string> warnings;      /**< List of validation warnings */
    std::vector<std::string> suggestions;   /**< List of suggested alternatives */
    double confidenceScore{0.0};            /**< Confidence score (0.0-1.0) */
    std::unordered_map<std::string, std::string> metadata; /**< Additional metadata */
};

/**
 * @class LocaleValidator
 * @brief Comprehensive locale validation utilities
 */
class LocaleValidator {
public:
    /**
     * @brief Validates a locale with specified validation level
     * @param locale The locale to validate
     * @param level The validation level to use
     * @return ValidationResult with detailed validation information
     */
    static auto validate(const std::string& locale, ValidationLevel level = ValidationLevel::Standard) -> ValidationResult;
    
    /**
     * @brief Validates locale format (syntax only)
     * @param locale The locale to validate
     * @return ValidationResult with format validation
     */
    static auto validateFormat(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Validates locale availability on the system
     * @param locale The locale to validate
     * @return ValidationResult with availability validation
     */
    static auto validateAvailability(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Validates locale functionality (can be used for formatting)
     * @param locale The locale to validate
     * @return ValidationResult with functionality validation
     */
    static auto validateFunctionality(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Validates locale compatibility with system
     * @param locale The locale to validate
     * @return ValidationResult with compatibility validation
     */
    static auto validateCompatibility(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Checks if two locales are compatible
     * @param locale1 First locale
     * @param locale2 Second locale
     * @return Compatibility score (0.0-1.0)
     */
    static auto checkCompatibility(const std::string& locale1, const std::string& locale2) -> double;
    
    /**
     * @brief Gets suggested alternatives for an invalid locale
     * @param locale The invalid locale
     * @param maxSuggestions Maximum number of suggestions
     * @return Vector of suggested locale alternatives
     */
    static auto getSuggestions(const std::string& locale, size_t maxSuggestions = 5) -> std::vector<std::string>;
    
    /**
     * @brief Validates a list of locales and returns the best valid one
     * @param locales List of locales to validate
     * @param level Validation level to use
     * @return Best valid locale or empty string if none are valid
     */
    static auto findBestValidLocale(const std::vector<std::string>& locales, 
                                    ValidationLevel level = ValidationLevel::Standard) -> std::string;
    
    /**
     * @brief Runs comprehensive locale system tests
     * @return ValidationResult with system test results
     */
    static auto runSystemTests() -> ValidationResult;
    
    /**
     * @brief Tests locale formatting capabilities
     * @param locale The locale to test
     * @return ValidationResult with formatting test results
     */
    static auto testFormatting(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Tests locale sorting/collation capabilities
     * @param locale The locale to test
     * @return ValidationResult with collation test results
     */
    static auto testCollation(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Tests locale character classification
     * @param locale The locale to test
     * @return ValidationResult with character classification test results
     */
    static auto testCharacterClassification(const std::string& locale) -> ValidationResult;
    
    /**
     * @brief Benchmarks locale performance
     * @param locale The locale to benchmark
     * @param iterations Number of test iterations
     * @return Performance metrics in milliseconds
     */
    static auto benchmarkPerformance(const std::string& locale, size_t iterations = 1000) -> std::unordered_map<std::string, double>;
};

/**
 * @class LocaleTester
 * @brief Automated testing utilities for locale functionality
 */
class LocaleTester {
public:
    /**
     * @brief Test suite result
     */
    struct TestSuiteResult {
        size_t totalTests{0};
        size_t passedTests{0};
        size_t failedTests{0};
        std::vector<std::string> failureDetails;
        double executionTimeMs{0.0};
    };
    
    /**
     * @brief Runs a comprehensive test suite for a locale
     * @param locale The locale to test
     * @return TestSuiteResult with detailed test results
     */
    static auto runTestSuite(const std::string& locale) -> TestSuiteResult;
    
    /**
     * @brief Tests number formatting for a locale
     * @param locale The locale to test
     * @return Test result
     */
    static auto testNumberFormatting(const std::string& locale) -> bool;
    
    /**
     * @brief Tests currency formatting for a locale
     * @param locale The locale to test
     * @return Test result
     */
    static auto testCurrencyFormatting(const std::string& locale) -> bool;
    
    /**
     * @brief Tests date/time formatting for a locale
     * @param locale The locale to test
     * @return Test result
     */
    static auto testDateTimeFormatting(const std::string& locale) -> bool;
    
    /**
     * @brief Tests string collation for a locale
     * @param locale The locale to test
     * @return Test result
     */
    static auto testStringCollation(const std::string& locale) -> bool;
    
    /**
     * @brief Tests character case conversion for a locale
     * @param locale The locale to test
     * @return Test result
     */
    static auto testCaseConversion(const std::string& locale) -> bool;
    
    /**
     * @brief Tests locale switching performance
     * @param fromLocale Source locale
     * @param toLocale Target locale
     * @param iterations Number of switch iterations
     * @return Average switch time in milliseconds
     */
    static auto testSwitchingPerformance(const std::string& fromLocale, 
                                         const std::string& toLocale, 
                                         size_t iterations = 100) -> double;
    
    /**
     * @brief Stress tests locale operations
     * @param locale The locale to stress test
     * @param duration Test duration in seconds
     * @return Stress test results
     */
    static auto stressTest(const std::string& locale, double duration = 10.0) -> TestSuiteResult;
};

/**
 * @class LocaleCompatibilityChecker
 * @brief Utilities for checking locale compatibility across systems
 */
class LocaleCompatibilityChecker {
public:
    /**
     * @brief Compatibility report
     */
    struct CompatibilityReport {
        std::string locale;
        bool isPortable{false};
        std::vector<std::string> supportedPlatforms;
        std::vector<std::string> unsupportedPlatforms;
        std::vector<std::string> alternativeLocales;
        std::unordered_map<std::string, std::string> platformSpecificIssues;
    };
    
    /**
     * @brief Checks locale compatibility across platforms
     * @param locale The locale to check
     * @return CompatibilityReport with detailed compatibility information
     */
    static auto checkCrossPlatformCompatibility(const std::string& locale) -> CompatibilityReport;
    
    /**
     * @brief Finds portable locale alternatives
     * @param locale The locale to find alternatives for
     * @param maxAlternatives Maximum number of alternatives
     * @return Vector of portable locale alternatives
     */
    static auto findPortableAlternatives(const std::string& locale, size_t maxAlternatives = 3) -> std::vector<std::string>;
    
    /**
     * @brief Checks if a locale is likely to be available on most systems
     * @param locale The locale to check
     * @return Portability score (0.0-1.0)
     */
    static auto getPortabilityScore(const std::string& locale) -> double;
    
    /**
     * @brief Gets platform-specific locale recommendations
     * @param platform Target platform ("windows", "linux", "macos")
     * @param language Desired language code
     * @return Vector of recommended locales for the platform
     */
    static auto getPlatformRecommendations(const std::string& platform, const std::string& language) -> std::vector<std::string>;
};

} // namespace atom::system::locale

#endif // ATOM_SYSINFO_LOCALE_VALIDATOR_HPP
