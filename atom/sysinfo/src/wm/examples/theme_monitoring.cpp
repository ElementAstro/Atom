/**
 * @file theme_monitoring.cpp
 * @brief Theme monitoring and detection example
 * 
 * This example demonstrates how to monitor theme changes and
 * detect system theme preferences.
 */

#include "atom/sysinfo/wm/wm.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

using namespace atom::system::wm;

std::atomic<bool> running{true};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
        running = false;
    }
}

void printThemeInfo(const ThemeInfo& theme) {
    std::cout << "Theme Information:" << std::endl;
    std::cout << "  Type: " << themeTypeToString(theme.type) << std::endl;
    std::cout << "  Name: " << theme.name << std::endl;
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
    std::cout << std::endl;
}

void getCurrentTheme() {
    std::cout << "=== Current Theme ===" << std::endl;
    
    ThemeManager tm;
    auto result = tm.getCurrentTheme();
    
    if (isError(result)) {
        std::cout << "Error getting current theme: " << errorToString(getError(result)) << std::endl;
        return;
    }
    
    const auto& theme = getValue(result);
    printThemeInfo(theme);
}

void monitorThemeChanges() {
    std::cout << "=== Theme Monitoring ===" << std::endl;
    std::cout << "Monitoring theme changes... (Press Ctrl+C to stop)" << std::endl;
    std::cout << "Try changing your system theme (light/dark mode) to see changes detected." << std::endl;
    std::cout << std::endl;
    
    ThemeManager tm;
    
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    bool monitoringStarted = tm.startMonitoring([](const ThemeInfo& theme) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "Theme changed at " << std::ctime(&time_t);
        printThemeInfo(theme);
    }, std::chrono::milliseconds(2000)); // Check every 2 seconds
    
    if (!monitoringStarted) {
        std::cout << "Failed to start theme monitoring." << std::endl;
        return;
    }
    
    std::cout << "Theme monitoring started successfully." << std::endl;
    
    // Keep the program running
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    tm.stopMonitoring();
    std::cout << "Theme monitoring stopped." << std::endl;
}

void detectThemePreferences() {
    std::cout << "=== Theme Preference Detection ===" << std::endl;
    
    auto result = getThemeInfo();
    if (isError(result)) {
        std::cout << "Error getting theme info: " << errorToString(getError(result)) << std::endl;
        return;
    }
    
    const auto& theme = getValue(result);
    
    // Analyze theme preferences
    std::cout << "Theme Analysis:" << std::endl;
    
    switch (theme.type) {
        case ThemeType::LIGHT:
            std::cout << "  User prefers light themes" << std::endl;
            std::cout << "  Recommended: Use light color schemes in your application" << std::endl;
            break;
        case ThemeType::DARK:
            std::cout << "  User prefers dark themes" << std::endl;
            std::cout << "  Recommended: Use dark color schemes in your application" << std::endl;
            break;
        case ThemeType::AUTO:
            std::cout << "  User follows system theme preference" << std::endl;
            std::cout << "  Recommended: Implement automatic theme switching" << std::endl;
            break;
        case ThemeType::HIGH_CONTRAST:
            std::cout << "  User requires high contrast themes" << std::endl;
            std::cout << "  Recommended: Use high contrast colors and clear borders" << std::endl;
            break;
        case ThemeType::CUSTOM:
            std::cout << "  User has a custom theme" << std::endl;
            std::cout << "  Recommended: Allow theme customization in your application" << std::endl;
            break;
        default:
            std::cout << "  Theme preference unknown" << std::endl;
            std::cout << "  Recommended: Provide theme selection options" << std::endl;
            break;
    }
    
    if (theme.followsSystemTheme) {
        std::cout << "  Theme follows system settings - consider implementing system theme detection" << std::endl;
    }
    
    // Color recommendations
    if (!theme.accentColor.empty()) {
        std::cout << "  System accent color: " << theme.accentColor << std::endl;
        std::cout << "  Recommended: Use this color for highlights and active elements" << std::endl;
    }
    
    std::cout << std::endl;
}

void demonstrateThemeAdaptation() {
    std::cout << "=== Theme Adaptation Example ===" << std::endl;
    
    auto result = getThemeInfo();
    if (isError(result)) {
        std::cout << "Error getting theme info: " << errorToString(getError(result)) << std::endl;
        return;
    }
    
    const auto& theme = getValue(result);
    
    // Simulate application theme adaptation
    std::cout << "Adapting application theme based on system settings:" << std::endl;
    
    if (theme.type == ThemeType::DARK) {
        std::cout << "  Setting application to dark mode" << std::endl;
        std::cout << "  Background: #2E2E2E" << std::endl;
        std::cout << "  Text: #FFFFFF" << std::endl;
        std::cout << "  Accent: " << (theme.accentColor.empty() ? "#0078D4" : theme.accentColor) << std::endl;
    } else if (theme.type == ThemeType::LIGHT) {
        std::cout << "  Setting application to light mode" << std::endl;
        std::cout << "  Background: #FFFFFF" << std::endl;
        std::cout << "  Text: #000000" << std::endl;
        std::cout << "  Accent: " << (theme.accentColor.empty() ? "#0078D4" : theme.accentColor) << std::endl;
    } else {
        std::cout << "  Using default theme with system accent color" << std::endl;
        if (!theme.accentColor.empty()) {
            std::cout << "  Accent: " << theme.accentColor << std::endl;
        }
    }
    
    std::cout << std::endl;
}

int main() {
    std::cout << "Theme Monitoring Example" << std::endl;
    std::cout << "========================" << std::endl;
    
    try {
        // Show current theme
        getCurrentTheme();
        
        // Detect theme preferences
        detectThemePreferences();
        
        // Demonstrate theme adaptation
        demonstrateThemeAdaptation();
        
        // Ask user if they want to monitor changes
        std::cout << "Would you like to monitor theme changes? (y/n): ";
        char choice;
        std::cin >> choice;
        
        if (choice == 'y' || choice == 'Y') {
            monitorThemeChanges();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
