/**
 * @file shortcut_detector_example.cpp
 * @brief Example demonstrating keyboard shortcut detection
 *
 * This example shows how to use the ShortcutDetector to:
 * - Check if specific shortcuts are captured
 * - Detect keyboard hooks
 * - Identify processes with keyboard hooks
 * - Test various shortcut combinations
 */

#include "atom/system/shortcut/detector.hpp"
#include "atom/system/shortcut/shortcut.h"

#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace shortcut_detector;

void demonstrateBasicShortcutCheck() {
    std::cout << "\n=== Basic Shortcut Detection ===" << std::endl;

    ShortcutDetector detector;

    // Test common shortcuts
    std::vector<Shortcut> shortcuts = {
        Shortcut(VK_C, true, false, false, false),    // Ctrl+C
        Shortcut(VK_V, true, false, false, false),    // Ctrl+V
        Shortcut(VK_TAB, false, true, false, false),  // Alt+Tab
        Shortcut(VK_F4, false, true, false, false),   // Alt+F4
        Shortcut(VK_L, false, false, false, true),    // Win+L
        Shortcut(VK_D, false, false, false, true)     // Win+D
    };

    const char* shortcutNames[] = {"Ctrl+C", "Ctrl+V", "Alt+Tab",
                                   "Alt+F4", "Win+L",  "Win+D"};

    for (size_t i = 0; i < shortcuts.size(); ++i) {
        auto result = detector.isShortcutCaptured(shortcuts[i]);

        std::cout << "  " << shortcutNames[i] << ": ";

        switch (result.status) {
            case ShortcutStatus::Available:
                std::cout << "✓ Available" << std::endl;
                break;
            case ShortcutStatus::CapturedBySystem:
                std::cout << "⚠ Captured by System" << std::endl;
                break;
            case ShortcutStatus::CapturedByApplication:
                std::cout << "⚠ Captured by Application";
                if (!result.capturingProcess.empty()) {
                    std::cout << " (" << result.capturingProcess << ")";
                }
                std::cout << std::endl;
                break;
            case ShortcutStatus::Unknown:
                std::cout << "? Unknown" << std::endl;
                break;
        }

        if (!result.message.empty()) {
            std::cout << "    Message: " << result.message << std::endl;
        }
    }
}

void demonstrateKeyboardHookDetection() {
    std::cout << "\n=== Keyboard Hook Detection ===" << std::endl;

    ShortcutDetector detector;

    bool hasHook = detector.hasKeyboardHookInstalled();

    if (hasHook) {
        std::cout << "⚠ Keyboard hook detected!" << std::endl;
    } else {
        std::cout << "✓ No keyboard hook detected" << std::endl;
    }
}

void demonstrateProcessHookEnumeration() {
    std::cout << "\n=== Processes with Keyboard Hooks ===" << std::endl;

    ShortcutDetector detector;

    auto processes = detector.getProcessesWithKeyboardHooks();

    if (processes.empty()) {
        std::cout << "✓ No processes with keyboard hooks found" << std::endl;
    } else {
        std::cout << "Found " << processes.size()
                  << " process(es) with keyboard hooks:" << std::endl;
        for (const auto& process : processes) {
            std::cout << "  - " << process << std::endl;
        }
    }
}

void demonstrateCustomShortcuts() {
    std::cout << "\n=== Custom Shortcut Combinations ===" << std::endl;

    ShortcutDetector detector;

    // Test various modifier combinations
    std::vector<Shortcut> customShortcuts = {
        Shortcut(VK_F1, true, true, false, false),     // Ctrl+Alt+F1
        Shortcut(VK_F2, true, false, true, false),     // Ctrl+Shift+F2
        Shortcut(VK_F3, false, true, true, false),     // Alt+Shift+F3
        Shortcut(VK_F4, true, true, true, false),      // Ctrl+Alt+Shift+F4
        Shortcut(VK_SPACE, false, false, false, true)  // Win+Space
    };

    const char* customNames[] = {"Ctrl+Alt+F1", "Ctrl+Shift+F2", "Alt+Shift+F3",
                                 "Ctrl+Alt+Shift+F4", "Win+Space"};

    for (size_t i = 0; i < customShortcuts.size(); ++i) {
        auto result = detector.isShortcutCaptured(customShortcuts[i]);

        std::cout << "  " << customNames[i] << ": ";
        std::cout << (result.status == ShortcutStatus::Available
                          ? "✓ Available"
                          : "⚠ Captured");
        std::cout << std::endl;
    }
}

void demonstrateShortcutToString() {
    std::cout << "\n=== Shortcut String Representation ===" << std::endl;

    std::vector<Shortcut> shortcuts = {
        Shortcut(VK_A, true, false, false, false),
        Shortcut(VK_B, false, true, false, false),
        Shortcut(VK_C, false, false, true, false),
        Shortcut(VK_D, false, false, false, true),
        Shortcut(VK_E, true, true, true, true)};

    for (const auto& shortcut : shortcuts) {
        std::cout << "  " << shortcut.toString() << std::endl;
    }
}

int main() {
    std::cout << "Keyboard Shortcut Detection Example" << std::endl;
    std::cout << "====================================" << std::endl;

#ifndef _WIN32
    std::cout << "\n⚠ Warning: This example is designed for Windows."
              << std::endl;
    std::cout << "   Some features may not work on other platforms."
              << std::endl;
#endif

    try {
        demonstrateBasicShortcutCheck();
        demonstrateKeyboardHookDetection();
        demonstrateProcessHookEnumeration();
        demonstrateCustomShortcuts();
        demonstrateShortcutToString();

        std::cout << "\n✓ All shortcut detection demonstrations completed!"
                  << std::endl;
        std::cout << "\nNote: This tool is useful for detecting conflicts with"
                  << std::endl;
        std::cout << "      system shortcuts or other applications."
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
