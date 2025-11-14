/**
 * @file wm_example.cpp
 * @brief Comprehensive example demonstrating window manager and desktop
 * environment information
 *
 * This example provides a complete demonstration of window manager and desktop
 * environment information gathering capabilities available in the Atom Sysinfo
 * module.
 *
 * Features demonstrated:
 * - Desktop environment detection
 * - Window manager identification
 * - Theme information retrieval
 * - Icon and font settings
 * - Cursor theme information
 * - Cross-platform desktop information gathering
 *
 * Platform support:
 * - Windows (Desktop Window Manager, themes)
 * - Linux (GNOME, KDE, XFCE, i3, etc.)
 * - macOS (Aqua)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive window manager information demonstration
 */

#include <chrono>
#include <iostream>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/info/wm.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
}  // namespace

/**
 * @brief Utility function to create formatted section headers
 */
std::string createSectionHeader(const std::string& title) {
    std::string header = "\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
    header += "=== " + title + " ===\n";
    header += std::string(DISPLAY_WIDTH, '=') + "\n";
    return header;
}

/**
 * @brief Demonstrates window manager and desktop environment information
 */
void demonstrateWindowManagerInformation() {
    std::cout << createSectionHeader(
        "Window Manager and Desktop Environment Information");

    try {
        std::cout << "Gathering window manager and desktop environment "
                     "information...\n\n";

        auto sysInfo = getSystemInfo();

        // Desktop Environment
        std::cout << "Desktop Environment:\n";
        std::cout << "  Desktop Environment:  ";
        if (!sysInfo.desktopEnvironment.empty()) {
            std::cout << sysInfo.desktopEnvironment << "\n";

            // Provide information about the desktop environment
            if (sysInfo.desktopEnvironment.find("GNOME") != std::string::npos) {
                std::cout << "  Type:                 Modern, GTK-based\n";
                std::cout
                    << "  Description:          Popular Linux desktop with "
                       "modern UI\n";
            } else if (sysInfo.desktopEnvironment.find("KDE") !=
                       std::string::npos) {
                std::cout << "  Type:                 Feature-rich, Qt-based\n";
                std::cout
                    << "  Description:          Highly customizable desktop "
                       "environment\n";
            } else if (sysInfo.desktopEnvironment.find("XFCE") !=
                       std::string::npos) {
                std::cout << "  Type:                 Lightweight, GTK-based\n";
                std::cout
                    << "  Description:          Fast and resource-efficient\n";
            } else if (sysInfo.desktopEnvironment.find("Fluent") !=
                       std::string::npos) {
                std::cout << "  Type:                 Windows Desktop\n";
                std::cout
                    << "  Description:          Microsoft's modern design "
                       "language\n";
            } else if (sysInfo.desktopEnvironment.find("Aqua") !=
                       std::string::npos) {
                std::cout << "  Type:                 macOS Desktop\n";
                std::cout
                    << "  Description:          Apple's desktop environment\n";
            }
        } else {
            std::cout << "Not available\n";
        }

        // Window Manager
        std::cout << "\nWindow Manager:\n";
        std::cout << "  Window Manager:       ";
        if (!sysInfo.windowManager.empty()) {
            std::cout << sysInfo.windowManager << "\n";

            // Provide information about the window manager
            if (sysInfo.windowManager.find("i3") != std::string::npos) {
                std::cout << "  Type:                 Tiling window manager\n";
                std::cout
                    << "  Description:          Keyboard-driven, efficient "
                       "workspace management\n";
            } else if (sysInfo.windowManager.find("bspwm") !=
                       std::string::npos) {
                std::cout
                    << "  Type:                 Binary space partitioning "
                       "WM\n";
                std::cout
                    << "  Description:          Tiling window manager with "
                       "scriptable configuration\n";
            } else if (sysInfo.windowManager.find("Desktop Window Manager") !=
                       std::string::npos) {
                std::cout
                    << "  Type:                 Compositing window manager\n";
                std::cout << "  Description:          Windows' built-in window "
                             "manager with Aero effects\n";
            }
        } else {
            std::cout << "Not available\n";
        }

        // Theme Information
        std::cout << "\nTheme and Appearance:\n";
        std::cout << "  WM Theme:             "
                  << (sysInfo.wmTheme.empty() ? "Not available"
                                              : sysInfo.wmTheme)
                  << "\n";
        std::cout << "  Icons:                "
                  << (sysInfo.icons.empty() ? "Not available" : sysInfo.icons)
                  << "\n";
        std::cout << "  Font:                 "
                  << (sysInfo.font.empty() ? "Not available" : sysInfo.font)
                  << "\n";
        std::cout << "  Cursor:               "
                  << (sysInfo.cursor.empty() ? "Not available" : sysInfo.cursor)
                  << "\n";

        // Desktop Environment Information
        std::cout << "\nDesktop Environment Information:\n";
        std::cout << "  Desktop environments provide:\n";
        std::cout << "    • Window management and compositing\n";
        std::cout << "    • Application launchers and menus\n";
        std::cout << "    • System settings and configuration\n";
        std::cout << "    • File managers and system utilities\n";
        std::cout << "    • Theming and visual customization\n";

        std::cout << "\n✓ Window manager information gathering completed "
                     "successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering window manager information: "
                  << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive window manager capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Window Manager and Desktop Environment "
                 "Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "This example demonstrates comprehensive window manager and\n";
    std::cout << "desktop environment information gathering capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateWindowManagerInformation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Window manager information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Desktop environment detection\n";
        std::cout << "  ✓ Window manager identification\n";
        std::cout << "  ✓ Theme and appearance information\n";
        std::cout << "  ✓ Icon, font, and cursor settings\n";
        std::cout << "  ✓ Cross-platform desktop information gathering\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        std::cerr << std::string(DISPLAY_WIDTH, '=') << "\n";
        return 1;
    }

    return 0;
}
