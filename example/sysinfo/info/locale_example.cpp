/**
 * @file locale_example.cpp
 * @brief Comprehensive example demonstrating locale and language information
 *
 * This example provides a complete demonstration of system locale, language,
 * and encoding information gathering capabilities available in the Atom Sysinfo
 * module.
 *
 * Features demonstrated:
 * - System language detection
 * - System encoding detection
 * - Locale information retrieval
 * - Cross-platform locale information gathering
 *
 * Platform support:
 * - Windows (with Windows API)
 * - Linux (with locale and environment variables)
 * - macOS (with CoreFoundation)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive locale information demonstration
 */

#include <chrono>
#include <iostream>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/info/locale.hpp"

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
 * @brief Demonstrates locale and language information
 */
void demonstrateLocaleInformation() {
    std::cout << createSectionHeader("Locale and Language Information");

    try {
        std::cout << "Gathering locale and language information...\n\n";

        // System Language
        std::cout << "Language Settings:\n";
        try {
            auto language = getSystemLanguage();
            std::cout << "  System Language:      " << language << "\n";

            // Analyze language code
            if (language.find("en") != std::string::npos) {
                std::cout << "  Language Family:      English\n";
            } else if (language.find("zh") != std::string::npos) {
                std::cout << "  Language Family:      Chinese\n";
            } else if (language.find("es") != std::string::npos) {
                std::cout << "  Language Family:      Spanish\n";
            } else if (language.find("fr") != std::string::npos) {
                std::cout << "  Language Family:      French\n";
            } else if (language.find("de") != std::string::npos) {
                std::cout << "  Language Family:      German\n";
            } else if (language.find("ja") != std::string::npos) {
                std::cout << "  Language Family:      Japanese\n";
            } else if (language.find("ko") != std::string::npos) {
                std::cout << "  Language Family:      Korean\n";
            } else if (language.find("ru") != std::string::npos) {
                std::cout << "  Language Family:      Russian\n";
            } else {
                std::cout << "  Language Family:      Other\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "  System Language:      ✗ Error: " << e.what()
                      << "\n";
        }

        // System Encoding
        std::cout << "\nEncoding Settings:\n";
        try {
            auto encoding = getSystemEncoding();
            std::cout << "  System Encoding:      " << encoding << "\n";

            // Analyze encoding
            if (encoding.find("UTF-8") != std::string::npos ||
                encoding.find("utf-8") != std::string::npos) {
                std::cout << "  Encoding Type:        Unicode (UTF-8)\n";
                std::cout << "  Unicode Support:      ✓ Full Unicode support\n";
            } else if (encoding.find("UTF-16") != std::string::npos ||
                       encoding.find("utf-16") != std::string::npos) {
                std::cout << "  Encoding Type:        Unicode (UTF-16)\n";
                std::cout << "  Unicode Support:      ✓ Full Unicode support\n";
            } else if (encoding.find("ASCII") != std::string::npos ||
                       encoding.find("ascii") != std::string::npos) {
                std::cout << "  Encoding Type:        ASCII\n";
                std::cout
                    << "  Unicode Support:      ✗ Limited character set\n";
            } else {
                std::cout << "  Encoding Type:        Legacy/Other\n";
                std::cout << "  Unicode Support:      ⚠ May have limitations\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "  System Encoding:      ✗ Error: " << e.what()
                      << "\n";
        }

        // Locale Analysis
        std::cout << "\nLocale Analysis:\n";
        std::cout << "  Locale information provides:\n";
        std::cout << "    • Language preferences for the system\n";
        std::cout << "    • Character encoding for text display\n";
        std::cout << "    • Regional settings for date/time/number formats\n";
        std::cout << "    • Collation rules for text sorting\n";

        std::cout
            << "\n✓ Locale information gathering completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error gathering locale information: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive locale capabilities
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Locale and Language Information Example "
                 "===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "This example demonstrates comprehensive locale and language\n";
    std::cout << "information gathering capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateLocaleInformation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Locale information gathering completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ System language detection\n";
        std::cout << "  ✓ System encoding detection\n";
        std::cout << "  ✓ Locale analysis and interpretation\n";
        std::cout << "  ✓ Cross-platform locale information gathering\n";

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
