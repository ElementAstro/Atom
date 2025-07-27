/**
 * @file advanced_formatting.cpp
 * @brief Advanced formatting example for the locale module
 *
 * This example demonstrates advanced formatting capabilities including
 * custom precision, different date/time styles, currency formatting
 * with different currencies, and locale-specific list formatting.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "../locale.hpp"
#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

using namespace atom::system;

void demonstrateNumberFormatting() {
    std::cout << "=== Advanced Number Formatting ===" << std::endl;
    
    std::vector<std::string> locales = {
        "en_US.UTF-8",  // US: 1,234.56
        "de_DE.UTF-8",  // German: 1.234,56
        "fr_FR.UTF-8",  // French: 1 234,56
        "en_IN.UTF-8",  // Indian: 1,234.56 (with lakh/crore system)
        "ar_SA.UTF-8"   // Arabic: ١٬٢٣٤٫٥٦ (if available)
    };
    
    std::vector<double> numbers = {
        1234.56,
        -9876.543,
        0.00123,
        1000000.789,
        0.0
    };
    
    for (const auto& locale : locales) {
        std::cout << "Locale: " << locale << std::endl;
        
        try {
            LocaleFormatter formatter(locale);
            
            for (const auto& number : numbers) {
                std::cout << "  " << std::setw(12) << number << " -> ";
                
                // Different precisions
                std::cout << "Default: " << std::setw(15) << formatter.formatNumber(number);
                std::cout << " | 0 dec: " << std::setw(12) << formatter.formatNumber(number, 0);
                std::cout << " | 4 dec: " << std::setw(15) << formatter.formatNumber(number, 4);
                std::cout << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateCurrencyFormatting() {
    std::cout << "=== Advanced Currency Formatting ===" << std::endl;
    
    struct CurrencyTest {
        std::string locale;
        std::string currency;
        double amount;
    };
    
    std::vector<CurrencyTest> tests = {
        {"en_US.UTF-8", "USD", 1234.56},
        {"en_US.UTF-8", "EUR", 1234.56},
        {"en_US.UTF-8", "JPY", 1234.56},
        {"de_DE.UTF-8", "EUR", 1234.56},
        {"de_DE.UTF-8", "USD", 1234.56},
        {"ja_JP.UTF-8", "JPY", 1234.56},
        {"ja_JP.UTF-8", "USD", 1234.56},
        {"en_GB.UTF-8", "GBP", 1234.56},
        {"fr_FR.UTF-8", "EUR", 1234.56}
    };
    
    for (const auto& test : tests) {
        try {
            LocaleFormatter formatter(test.locale);
            
            std::cout << "Locale: " << std::setw(12) << test.locale 
                      << " | Currency: " << test.currency 
                      << " | Amount: " << test.amount << std::endl;
            
            // Default currency formatting
            std::cout << "  Default: " << formatter.formatCurrency(test.amount) << std::endl;
            
            // With specific currency code
            std::cout << "  With " << test.currency << ": " 
                      << formatter.formatCurrency(test.amount, test.currency) << std::endl;
            
            // Negative amounts
            std::cout << "  Negative: " << formatter.formatCurrency(-test.amount) << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateDateTimeFormatting() {
    std::cout << "=== Advanced Date/Time Formatting ===" << std::endl;
    
    std::vector<std::string> locales = {
        "en_US.UTF-8",
        "de_DE.UTF-8",
        "fr_FR.UTF-8",
        "ja_JP.UTF-8",
        "ar_SA.UTF-8"
    };
    
    std::vector<std::string> dateStyles = {"short", "medium", "long", "full"};
    std::vector<std::string> timeStyles = {"short", "medium", "long", "full"};
    
    auto now = std::chrono::system_clock::now();
    auto past = now - std::chrono::hours(24 * 30); // 30 days ago
    auto future = now + std::chrono::hours(24 * 7); // 7 days from now
    
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> timePoints = {
        {"Now", now},
        {"30 days ago", past},
        {"7 days from now", future}
    };
    
    for (const auto& locale : locales) {
        std::cout << "Locale: " << locale << std::endl;
        
        try {
            LocaleFormatter formatter(locale);
            
            for (const auto& [label, timePoint] : timePoints) {
                std::cout << "  " << label << ":" << std::endl;
                
                // Different date styles
                for (const auto& style : dateStyles) {
                    std::cout << "    Date (" << style << "): " 
                              << formatter.formatDate(timePoint, style) << std::endl;
                }
                
                // Different time styles
                for (const auto& style : timeStyles) {
                    std::cout << "    Time (" << style << "): " 
                              << formatter.formatTime(timePoint, style) << std::endl;
                }
                
                // Combined date/time
                std::cout << "    DateTime: " 
                          << formatter.formatDateTime(timePoint, "medium", "short") << std::endl;
                
                std::cout << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstratePercentageFormatting() {
    std::cout << "=== Percentage Formatting ===" << std::endl;
    
    std::vector<std::string> locales = {
        "en_US.UTF-8",
        "de_DE.UTF-8",
        "fr_FR.UTF-8"
    };
    
    std::vector<double> percentages = {
        0.1234,    // 12.34%
        0.0056,    // 0.56%
        1.2345,    // 123.45%
        -0.0789,   // -7.89%
        0.0        // 0.00%
    };
    
    for (const auto& locale : locales) {
        std::cout << "Locale: " << locale << std::endl;
        
        try {
            LocaleFormatter formatter(locale);
            
            for (const auto& percentage : percentages) {
                std::cout << "  " << std::setw(8) << percentage << " -> ";
                
                // Different precisions
                std::cout << "0 dec: " << std::setw(10) << formatter.formatPercentage(percentage, 0);
                std::cout << " | 2 dec: " << std::setw(10) << formatter.formatPercentage(percentage, 2);
                std::cout << " | 4 dec: " << std::setw(12) << formatter.formatPercentage(percentage, 4);
                std::cout << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateListFormatting() {
    std::cout << "=== Locale-Specific List Formatting ===" << std::endl;
    
    std::vector<std::string> locales = {
        "en_US.UTF-8",  // "apple, banana, and cherry"
        "de_DE.UTF-8",  // "apple, banana und cherry"
        "fr_FR.UTF-8",  // "apple, banana et cherry"
        "es_ES.UTF-8",  // "apple, banana y cherry"
        "it_IT.UTF-8",  // "apple, banana e cherry"
        "ja_JP.UTF-8",  // "apple、bananaとcherry"
        "zh_CN.UTF-8"   // "apple、banana和cherry"
    };
    
    std::vector<std::vector<std::string>> lists = {
        {"apple"},
        {"apple", "banana"},
        {"apple", "banana", "cherry"},
        {"apple", "banana", "cherry", "date", "elderberry"},
        {"red", "green", "blue", "yellow", "purple", "orange"}
    };
    
    for (const auto& locale : locales) {
        std::cout << "Locale: " << locale << std::endl;
        
        try {
            LocaleFormatter formatter(locale);
            
            for (const auto& list : lists) {
                std::cout << "  " << list.size() << " items: " 
                          << formatter.formatList(list) << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

void demonstrateFormattingComparison() {
    std::cout << "=== Formatting Comparison Across Locales ===" << std::endl;
    
    std::vector<std::string> locales = {
        "en_US.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8", "ja_JP.UTF-8"
    };
    
    double number = 1234567.89;
    double currency = 1234.56;
    double percentage = 0.1234;
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> items = {"first", "second", "third"};
    
    std::cout << std::setw(12) << "Locale" 
              << std::setw(15) << "Number" 
              << std::setw(15) << "Currency" 
              << std::setw(12) << "Percentage" 
              << std::setw(15) << "Date" 
              << std::setw(20) << "List" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& locale : locales) {
        try {
            LocaleFormatter formatter(locale);
            
            std::cout << std::setw(12) << locale.substr(0, 5)
                      << std::setw(15) << formatter.formatNumber(number, 2)
                      << std::setw(15) << formatter.formatCurrency(currency)
                      << std::setw(12) << formatter.formatPercentage(percentage, 1)
                      << std::setw(15) << formatter.formatDate(now)
                      << std::setw(20) << formatter.formatList(items)
                      << std::endl;
                      
        } catch (const std::exception& e) {
            std::cout << std::setw(12) << locale.substr(0, 5)
                      << " Error: " << e.what() << std::endl;
        }
    }
    
    std::cout << std::endl;
}

int main() {
    std::cout << "Locale Module - Advanced Formatting Example" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        demonstrateNumberFormatting();
        demonstrateCurrencyFormatting();
        demonstrateDateTimeFormatting();
        demonstratePercentageFormatting();
        demonstrateListFormatting();
        demonstrateFormattingComparison();
        
        std::cout << "Advanced formatting example completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
