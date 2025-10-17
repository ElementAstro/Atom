/**
 * @file color_print_example.cpp
 * @brief Comprehensive examples demonstrating colored console output utilities
 *
 * This example demonstrates all functions available in
 * atom::utils::debug/color_print.hpp:
 * - Basic colored text output with different colors
 * - Text styling (bold, italic, underline, etc.)
 * - Formatted colored output with arguments
 * - Convenience methods for common message types (error, warning, info,
 * success)
 * - Advanced formatting combinations
 * - Cross-platform color support demonstration
 */

#include "atom/utils/debug/color_print.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace atom::utils;

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n";
    ColorPrinter::printColoredLine("==========================================",
                                   ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColoredLine("  " + title, ColorCode::Cyan,
                                   TextStyle::Bold);
    ColorPrinter::printColoredLine("==========================================",
                                   ColorCode::Cyan, TextStyle::Bold);
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n";
    ColorPrinter::printColoredLine("--- " + title + " ---", ColorCode::Yellow,
                                   TextStyle::Bold);
}

// Demonstrate a progress bar with colors
void demonstrateProgressBar() {
    std::cout << "Progress: ";
    for (int i = 0; i <= 20; ++i) {
        if (i < 7) {
            ColorPrinter::printColored("█", ColorCode::Red);
        } else if (i < 14) {
            ColorPrinter::printColored("█", ColorCode::Yellow);
        } else {
            ColorPrinter::printColored("█", ColorCode::Green);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::cout << " Complete!\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  Color Print Utilities Demo\n";
    std::cout << "==========================================\n";

    // ============================
    // Example 1: Basic Color Output
    // ============================
    printSection("1. Basic Color Output");

    printSubsection("Primary Colors");
    ColorPrinter::printColoredLine("This is RED text", ColorCode::Red);
    ColorPrinter::printColoredLine("This is GREEN text", ColorCode::Green);
    ColorPrinter::printColoredLine("This is BLUE text", ColorCode::Blue);
    ColorPrinter::printColoredLine("This is YELLOW text", ColorCode::Yellow);
    ColorPrinter::printColoredLine("This is MAGENTA text", ColorCode::Magenta);
    ColorPrinter::printColoredLine("This is CYAN text", ColorCode::Cyan);
    ColorPrinter::printColoredLine("This is WHITE text", ColorCode::White);

    printSubsection("Bright Colors");
    ColorPrinter::printColoredLine("This is BRIGHT RED text",
                                   ColorCode::BrightRed);
    ColorPrinter::printColoredLine("This is BRIGHT GREEN text",
                                   ColorCode::BrightGreen);
    ColorPrinter::printColoredLine("This is BRIGHT BLUE text",
                                   ColorCode::BrightBlue);
    ColorPrinter::printColoredLine("This is BRIGHT YELLOW text",
                                   ColorCode::BrightYellow);
    ColorPrinter::printColoredLine("This is BRIGHT MAGENTA text",
                                   ColorCode::BrightMagenta);
    ColorPrinter::printColoredLine("This is BRIGHT CYAN text",
                                   ColorCode::BrightCyan);
    ColorPrinter::printColoredLine("This is BRIGHT WHITE text",
                                   ColorCode::BrightWhite);

    // ============================
    // Example 2: Text Styles
    // ============================
    printSection("2. Text Styles");

    printSubsection("Style Variations");
    ColorPrinter::printColoredLine("Normal text", ColorCode::White,
                                   TextStyle::Normal);
    ColorPrinter::printColoredLine("Bold text", ColorCode::White,
                                   TextStyle::Bold);
    ColorPrinter::printColoredLine("Italic text", ColorCode::White,
                                   TextStyle::Italic);
    ColorPrinter::printColoredLine("Underlined text", ColorCode::White,
                                   TextStyle::Underline);
    ColorPrinter::printColoredLine("Strikethrough text", ColorCode::White,
                                   TextStyle::Strikethrough);

    printSubsection("Style Combinations");
    ColorPrinter::printColoredLine("Bold Red Text", ColorCode::Red,
                                   TextStyle::Bold);
    ColorPrinter::printColoredLine("Italic Blue Text", ColorCode::Blue,
                                   TextStyle::Italic);
    ColorPrinter::printColoredLine("Underlined Green Text", ColorCode::Green,
                                   TextStyle::Underline);
    ColorPrinter::printColoredLine("Bold Cyan Text", ColorCode::Cyan,
                                   TextStyle::Bold);

    // ============================
    // Example 3: Formatted Output
    // ============================
    printSection("3. Formatted Output");

    printSubsection("Basic Formatting");
    int number = 42;
    double pi = 3.14159;
    std::string name = "Alice";

    ColorPrinter::printColoredLine(ColorCode::Green, TextStyle::Normal,
                                   "Hello, {}! The answer is {} and π ≈ {:.3f}",
                                   name, number, pi);

    printSubsection("Multiple Arguments");
    std::vector<std::string> items = {"apple", "banana", "cherry"};
    ColorPrinter::printColoredLine(ColorCode::Yellow, TextStyle::Bold,
                                   "Shopping list: {}, {}, {}", items[0],
                                   items[1], items[2]);

    printSubsection("Complex Formatting");
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    ColorPrinter::printColoredLine(ColorCode::Magenta, TextStyle::Italic,
                                   "Current timestamp: {}", time_t);

    // ============================
    // Example 4: Convenience Methods
    // ============================
    printSection("4. Convenience Methods");

    printSubsection("Message Types");
    ColorPrinter::error("This is an error message");
    ColorPrinter::warning("This is a warning message");
    ColorPrinter::info("This is an info message");
    ColorPrinter::success("This is a success message");

    printSubsection("Formatted Convenience Methods");
    int errorCode = 404;
    std::string filename = "config.txt";
    ColorPrinter::error("File '{}' not found (Error: {})", filename, errorCode);

    double progress = 75.5;
    ColorPrinter::info("Download progress: {:.1f}%", progress);

    std::string operation = "Database backup";
    ColorPrinter::success("{} completed successfully!", operation);

    int warningCount = 3;
    ColorPrinter::warning("Found {} potential issues", warningCount);

    // ============================
    // Example 5: Advanced Usage
    // ============================
    printSection("5. Advanced Usage");

    printSubsection("Mixed Color Output");
    std::cout << "Status: ";
    ColorPrinter::printColored("ONLINE", ColorCode::Green, TextStyle::Bold);
    std::cout << " | Errors: ";
    ColorPrinter::printColored("0", ColorCode::Green);
    std::cout << " | Warnings: ";
    ColorPrinter::printColored("2", ColorCode::Yellow);
    std::cout << " | Critical: ";
    ColorPrinter::printColored("0", ColorCode::Red);
    std::cout << "\n";

    printSubsection("Table-like Output");
    ColorPrinter::printColoredLine("┌─────────────┬─────────┬────────┐",
                                   ColorCode::White);
    ColorPrinter::printColored("│ ", ColorCode::White);
    ColorPrinter::printColored("Service", ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColored("     │ ", ColorCode::White);
    ColorPrinter::printColored("Status", ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColored("  │ ", ColorCode::White);
    ColorPrinter::printColored("Uptime", ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColoredLine(" │", ColorCode::White);

    ColorPrinter::printColoredLine("├─────────────┼─────────┼────────┤",
                                   ColorCode::White);

    ColorPrinter::printColored("│ Web Server  │ ", ColorCode::White);
    ColorPrinter::printColored("RUNNING", ColorCode::Green);
    ColorPrinter::printColoredLine(" │ 99.9%  │", ColorCode::White);

    ColorPrinter::printColored("│ Database    │ ", ColorCode::White);
    ColorPrinter::printColored("STOPPED", ColorCode::Red);
    ColorPrinter::printColoredLine(" │ 0.0%   │", ColorCode::White);

    ColorPrinter::printColored("│ Cache       │ ", ColorCode::White);
    ColorPrinter::printColored("WARNING", ColorCode::Yellow);
    ColorPrinter::printColoredLine(" │ 95.2%  │", ColorCode::White);

    ColorPrinter::printColoredLine("└─────────────┴─────────┴────────┘",
                                   ColorCode::White);

    printSubsection("Animated Progress Bar");
    demonstrateProgressBar();

    // ============================
    // Example 6: Log-like Output
    // ============================
    printSection("6. Log-like Output");

    printSubsection("Simulated Log Messages");
    ColorPrinter::printColored("[", ColorCode::White);
    ColorPrinter::printColored("INFO", ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColored("] ", ColorCode::White);
    ColorPrinter::printColoredLine("Application started successfully",
                                   ColorCode::White);

    ColorPrinter::printColored("[", ColorCode::White);
    ColorPrinter::printColored("WARN", ColorCode::Yellow, TextStyle::Bold);
    ColorPrinter::printColored("] ", ColorCode::White);
    ColorPrinter::printColoredLine(
        "Configuration file not found, using defaults", ColorCode::Yellow);

    ColorPrinter::printColored("[", ColorCode::White);
    ColorPrinter::printColored("ERROR", ColorCode::Red, TextStyle::Bold);
    ColorPrinter::printColored("] ", ColorCode::White);
    ColorPrinter::printColoredLine("Failed to connect to database",
                                   ColorCode::Red);

    ColorPrinter::printColored("[", ColorCode::White);
    ColorPrinter::printColored("DEBUG", ColorCode::Magenta, TextStyle::Bold);
    ColorPrinter::printColored("] ", ColorCode::White);
    ColorPrinter::printColoredLine("Processing request ID: 12345",
                                   ColorCode::Magenta);

    std::cout << "\nAll color print examples completed successfully!\n";

    return 0;
}
