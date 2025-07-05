#include "detector.hpp"
#include "factory.h"
#include <spdlog/spdlog.h>
#include <iostream>

/**
 * @brief Simple test program for the refactored shortcut detector
 */
int main() {
    // Initialize spdlog with debug level for testing
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Starting shortcut detector test");

    try {
        // Create detector instance
        shortcut_detector::ShortcutDetector detector;

        // Test various shortcuts
        std::vector<std::pair<std::string, shortcut_detector::Shortcut>> testShortcuts = {
            {"Ctrl+C", shortcut_detector::ShortcutFactory::create('C', true, false, false, false)},
            {"Alt+Tab", shortcut_detector::ShortcutFactory::createVK(VK_TAB, false, true, false, false)},
            {"Win+D", shortcut_detector::ShortcutFactory::create('D', false, false, false, true)},
            {"Ctrl+Alt+Del", shortcut_detector::ShortcutFactory::createVK(VK_DELETE, true, true, false, false)},
            {"F1", shortcut_detector::ShortcutFactory::createVK(VK_F1, false, false, false, false)}
        };

        spdlog::info("Testing {} shortcuts", testShortcuts.size());

        for (const auto& [name, shortcut] : testShortcuts) {
            spdlog::info("Testing shortcut: {} ({})", name, shortcut.toString());

            const auto result = detector.isShortcutCaptured(shortcut);

            std::cout << "Shortcut: " << name << " (" << shortcut.toString() << ")\n";
            std::cout << "  Status: ";

            switch (result.status) {
                case shortcut_detector::ShortcutStatus::Available:
                    std::cout << "Available";
                    break;
                case shortcut_detector::ShortcutStatus::CapturedByApp:
                    std::cout << "Captured by Application: " << result.capturingApplication;
                    break;
                case shortcut_detector::ShortcutStatus::CapturedBySystem:
                    std::cout << "Captured by System: " << result.capturingApplication;
                    break;
                case shortcut_detector::ShortcutStatus::Reserved:
                    std::cout << "Reserved by Windows";
                    break;
            }

            std::cout << "\n  Details: " << result.details << "\n\n";
        }

        // Test keyboard hook detection
        spdlog::info("Testing keyboard hook detection");
        const bool hasHooks = detector.hasKeyboardHookInstalled();
        std::cout << "Keyboard hooks detected: " << (hasHooks ? "Yes" : "No") << "\n";

        if (hasHooks) {
            const auto processes = detector.getProcessesWithKeyboardHooks();
            std::cout << "Processes with keyboard hooks:\n";
            for (const auto& process : processes) {
                std::cout << "  - " << process << "\n";
            }
        }

        spdlog::info("Test completed successfully");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Test failed with exception: {}", e.what());
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
