/**
 * @file color_print_example.cpp
 * @brief Examples for atom::utils ColorPrinter
 */

#include "atom/utils/debug/color_print.hpp"
#include <iostream>
#include <string>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicColors() {
    printSection("1. Basic Colors");

    std::cout << "--- Available Colors ---" << std::endl;
    ColorPrinter::printColoredLine("Red text", ColorCode::Red);
    ColorPrinter::printColoredLine("Green text", ColorCode::Green);
    ColorPrinter::printColoredLine("Yellow text", ColorCode::Yellow);
    ColorPrinter::printColoredLine("Blue text", ColorCode::Blue);
    ColorPrinter::printColoredLine("Magenta text", ColorCode::Magenta);
    ColorPrinter::printColoredLine("Cyan text", ColorCode::Cyan);
    ColorPrinter::printColoredLine("White text", ColorCode::White);
}

void demonstrateTextStyles() {
    printSection("2. Text Styles");

    std::cout << "--- Text Styles ---" << std::endl;
    ColorPrinter::printColoredLine("Bold text", ColorCode::White, TextStyle::Bold);
    ColorPrinter::printColoredLine("Underlined text", ColorCode::White, TextStyle::Underline);
    ColorPrinter::printColoredLine("Bold + Colored", ColorCode::Cyan, TextStyle::Bold);
    ColorPrinter::printColoredLine("Underlined + Colored", ColorCode::Green, TextStyle::Underline);
}

void demonstrateLogLevels() {
    printSection("3. Log Level Messages");

    std::cout << "--- Standard Log Levels ---" << std::endl;
    ColorPrinter::info("This is an info message");
    ColorPrinter::success("This is a success message");
    ColorPrinter::warning("This is a warning message");
    ColorPrinter::error("This is an error message");

    std::cout << "\n--- With Format Arguments ---" << std::endl;
    ColorPrinter::info("Processing {} items...", 42);
    ColorPrinter::success("Completed in {:.2f} seconds", 1.234);
    ColorPrinter::warning("Memory usage at {}%", 85);
    ColorPrinter::error("Failed to open file: {}", "config.json");
}

void demonstrateInlineColoring() {
    printSection("4. Inline Coloring");

    std::cout << "--- Inline colored text ---" << std::endl;
    std::cout << "Status: ";
    ColorPrinter::printColored("ONLINE", ColorCode::Green, TextStyle::Bold);
    std::cout << std::endl;

    std::cout << "Temperature: ";
    ColorPrinter::printColored("75°C", ColorCode::Yellow, TextStyle::Bold);
    std::cout << " (Warning threshold)" << std::endl;

    std::cout << "Error count: ";
    ColorPrinter::printColored("3", ColorCode::Red, TextStyle::Bold);
    std::cout << " errors found" << std::endl;
}

void demonstrateStatusDisplay() {
    printSection("5. Status Display Example");

    struct ServiceStatus {
        std::string name;
        bool running;
        int connections;
    };

    std::vector<ServiceStatus> services = std::vector<ServiceStatus>{
        {"Web Server", true, 150},
        {"Database", true, 45},
        {"Cache", false, 0},
        {"Queue", true, 1200}
    };

    std::cout << "--- Service Status Dashboard ---" << std::endl;
    for (const auto& svc : services) {
        std::cout << "  " << svc.name << ": ";
        if (svc.running) {
            ColorPrinter::printColored("RUNNING", ColorCode::Green, TextStyle::Bold);
            std::cout << " (" << svc.connections << " connections)" << std::endl;
        } else {
            ColorPrinter::printColored("STOPPED", ColorCode::Red, TextStyle::Bold);
            std::cout << std::endl;
        }
    }
}

void demonstrateBuildOutput() {
    printSection("6. Build Output Example");

    std::cout << "--- Simulated Build Output ---" << std::endl;

    ColorPrinter::info("Starting build...");
    ColorPrinter::info("Compiling source files...");

    std::cout << "  [";
    ColorPrinter::printColored("OK", ColorCode::Green);
    std::cout << "] main.cpp" << std::endl;

    std::cout << "  [";
    ColorPrinter::printColored("OK", ColorCode::Green);
    std::cout << "] utils.cpp" << std::endl;

    std::cout << "  [";
    ColorPrinter::printColored("WARN", ColorCode::Yellow);
    std::cout << "] legacy.cpp (deprecated API)" << std::endl;

    ColorPrinter::info("Linking...");
    ColorPrinter::success("Build completed successfully!");

    std::cout << "\n--- Build Summary ---" << std::endl;
    std::cout << "  Files compiled: ";
    ColorPrinter::printColored("3", ColorCode::Cyan, TextStyle::Bold);
    std::cout << std::endl;

    std::cout << "  Warnings: ";
    ColorPrinter::printColored("1", ColorCode::Yellow, TextStyle::Bold);
    std::cout << std::endl;

    std::cout << "  Errors: ";
    ColorPrinter::printColored("0", ColorCode::Green, TextStyle::Bold);
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ColorPrinter Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicColors();
        demonstrateTextStyles();
        demonstrateLogLevels();
        demonstrateInlineColoring();
        demonstrateStatusDisplay();
        demonstrateBuildOutput();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All ColorPrinter examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
