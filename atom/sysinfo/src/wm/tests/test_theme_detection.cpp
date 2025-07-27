/**
 * @file test_theme_detection.cpp
 * @brief Theme detection and monitoring tests
 */

#include "wm.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

using namespace atom::system::wm;

// Simple test framework
class TestRunner {
public:
    static void run(const std::string& testName, std::function<bool()> test) {
        std::cout << "Running test: " << testName << "... ";
        try {
            if (test()) {
                std::cout << "PASSED" << std::endl;
                passed_++;
            } else {
                std::cout << "FAILED" << std::endl;
                failed_++;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << std::endl;
            failed_++;
        }
        total_++;
    }

    static void printSummary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total: " << total_ << std::endl;
        std::cout << "Passed: " << passed_ << std::endl;
        std::cout << "Failed: " << failed_ << std::endl;
        std::cout << "Success Rate: " << (total_ > 0 ? (passed_ * 100 / total_) : 0) << "%" << std::endl;
    }

    static int getFailedCount() { return failed_; }

private:
    static int total_;
    static int passed_;
    static int failed_;
};

int TestRunner::total_ = 0;
int TestRunner::passed_ = 0;
int TestRunner::failed_ = 0;

bool testBasicThemeDetection() {
    auto result = getThemeInfo();

    if (isError(result)) {
        WMError error = getError(result);
        // Only acceptable error is platform not supported
        return error == WMError::PLATFORM_NOT_SUPPORTED;
    }

    const auto& theme = getValue(result);

    std::cout << "\n  Theme Type: " << themeTypeToString(theme.type) << std::endl;
    std::cout << "  Theme Name: " << theme.name << std::endl;
    std::cout << "  Variant: " << theme.variant << std::endl;
    std::cout << "  Follows System: " << (theme.followsSystemTheme ? "Yes" : "No") << std::endl;

    if (!theme.backgroundColor.empty()) {
        std::cout << "  Background: " << theme.backgroundColor << std::endl;
    }
    if (!theme.foregroundColor.empty()) {
        std::cout << "  Foreground: " << theme.foregroundColor << std::endl;
    }
    if (!theme.accentColor.empty()) {
        std::cout << "  Accent: " << theme.accentColor << std::endl;
    }

    // Basic validation
    if (theme.type == ThemeType::UNKNOWN && theme.name.empty()) {
        // This might be acceptable on some platforms
        std::cout << "  Warning: Theme information is minimal" << std::endl;
    }

    return true;
}

bool testThemeManagerClass() {
    ThemeManager tm;

    // Test getCurrentTheme
    auto result = tm.getCurrentTheme();

    if (isError(result)) {
        WMError error = getError(result);
        return error == WMError::PLATFORM_NOT_SUPPORTED;
    }

    const auto& theme = getValue(result);
    std::cout << "\n  ThemeManager::getCurrentTheme() returned: " << theme.name << std::endl;

    // Test that it matches the standalone function
    auto standaloneResult = getThemeInfo();
    if (!isError(standaloneResult)) {
        const auto& standaloneTheme = getValue(standaloneResult);
        if (theme.type != standaloneTheme.type || theme.name != standaloneTheme.name) {
            std::cout << "  Warning: ThemeManager result differs from standalone function" << std::endl;
        }
    }

    return true;
}

bool testThemeMonitoring() {
    ThemeManager tm;

    std::cout << "\n  Testing theme monitoring (brief test)..." << std::endl;

    bool callbackCalled = false;
    auto callback = [&callbackCalled](const ThemeInfo& theme) {
        callbackCalled = true;
        std::cout << "  Theme change detected: " << theme.name << std::endl;
    };

    // Start monitoring
    bool started = tm.startMonitoring(callback, std::chrono::milliseconds(100));

    if (!started) {
        std::cout << "  Theme monitoring not supported or failed to start" << std::endl;
        return true; // This is acceptable
    }

    std::cout << "  Theme monitoring started successfully" << std::endl;

    // Check monitoring status
    if (!tm.isMonitoring()) {
        return false;
    }

    // Wait a short time (theme changes are unlikely in this short period)
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop monitoring
    tm.stopMonitoring();

    if (tm.isMonitoring()) {
        return false; // Should have stopped
    }

    std::cout << "  Theme monitoring stopped successfully" << std::endl;

    // Test that we can't start monitoring twice
    bool started1 = tm.startMonitoring(callback);
    bool started2 = tm.startMonitoring(callback);

    if (started1 && started2) {
        return false; // Should not be able to start twice
    }

    tm.stopMonitoring();

    return true;
}

bool testThemeColorValidation() {
    auto result = getThemeInfo();

    if (isError(result)) {
        return true; // Skip if theme info not available
    }

    const auto& theme = getValue(result);

    // Validate color format (if provided)
    auto validateColor = [](const std::string& color) -> bool {
        if (color.empty()) return true; // Empty is acceptable

        // Should start with # for hex colors
        if (color[0] != '#') return false;

        // Should be 7 characters (#RRGGBB) or 4 characters (#RGB)
        if (color.length() != 7 && color.length() != 4) return false;

        // All characters after # should be hex digits
        for (size_t i = 1; i < color.length(); ++i) {
            char c = color[i];
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'A' && c <= 'F') ||
                  (c >= 'a' && c <= 'f'))) {
                return false;
            }
        }

        return true;
    };

    if (!validateColor(theme.backgroundColor)) {
        std::cout << "\n  Invalid background color format: " << theme.backgroundColor << std::endl;
        return false;
    }

    if (!validateColor(theme.foregroundColor)) {
        std::cout << "\n  Invalid foreground color format: " << theme.foregroundColor << std::endl;
        return false;
    }

    if (!validateColor(theme.accentColor)) {
        std::cout << "\n  Invalid accent color format: " << theme.accentColor << std::endl;
        return false;
    }

    std::cout << "\n  Color format validation passed" << std::endl;

    return true;
}

bool testThemeConsistency() {
    auto result = getThemeInfo();

    if (isError(result)) {
        return true; // Skip if theme info not available
    }

    const auto& theme = getValue(result);

    // Check logical consistency
    if (theme.type == ThemeType::LIGHT) {
        // Light theme should have light background (if specified)
        if (!theme.backgroundColor.empty()) {
            // This is a heuristic - light themes typically have backgrounds starting with F, E, D, etc.
            if (theme.backgroundColor.length() >= 2) {
                char firstHex = theme.backgroundColor[1];
                if (firstHex < 'A' && firstHex < '8') {
                    std::cout << "\n  Warning: Light theme has dark background color" << std::endl;
                }
            }
        }
    } else if (theme.type == ThemeType::DARK) {
        // Dark theme should have dark background (if specified)
        if (!theme.backgroundColor.empty()) {
            if (theme.backgroundColor.length() >= 2) {
                char firstHex = theme.backgroundColor[1];
                if (firstHex > '7') {
                    std::cout << "\n  Warning: Dark theme has light background color" << std::endl;
                }
            }
        }
    }

    // Variant should match type
    if (theme.type == ThemeType::LIGHT && theme.variant == "dark") {
        std::cout << "\n  Warning: Theme type/variant mismatch" << std::endl;
    }
    if (theme.type == ThemeType::DARK && theme.variant == "light") {
        std::cout << "\n  Warning: Theme type/variant mismatch" << std::endl;
    }

    return true;
}

bool testThemeStringConversion() {
    // Test all theme type conversions
    std::string light = themeTypeToString(ThemeType::LIGHT);
    std::string dark = themeTypeToString(ThemeType::DARK);
    std::string auto_theme = themeTypeToString(ThemeType::AUTO);
    std::string high_contrast = themeTypeToString(ThemeType::HIGH_CONTRAST);
    std::string custom = themeTypeToString(ThemeType::CUSTOM);
    std::string unknown = themeTypeToString(ThemeType::UNKNOWN);

    // All should return non-empty strings
    if (light.empty() || dark.empty() || auto_theme.empty() ||
        high_contrast.empty() || custom.empty() || unknown.empty()) {
        return false;
    }

    std::cout << "\n  Theme type string conversions work correctly" << std::endl;

    return true;
}

int runThemeDetectionTests() {
    std::cout << "Running Theme Detection Tests" << std::endl;
    std::cout << "=============================" << std::endl;

    TestRunner::run("basic theme detection", testBasicThemeDetection);
    TestRunner::run("ThemeManager class", testThemeManagerClass);
    TestRunner::run("theme monitoring", testThemeMonitoring);
    TestRunner::run("theme color validation", testThemeColorValidation);
    TestRunner::run("theme consistency", testThemeConsistency);
    TestRunner::run("theme string conversion", testThemeStringConversion);

    TestRunner::printSummary();
    return TestRunner::getFailedCount();
}

int main() {
    return runThemeDetectionTests();
}
