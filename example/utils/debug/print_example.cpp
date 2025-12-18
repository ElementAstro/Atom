/**
 * @file print_example.cpp
 * @brief Examples for atom::utils print utilities
 */

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "atom/utils/debug/print.hpp"

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateBasicLogging() {
    printSection("1. Basic Logging");

    std::cout << "--- Log Levels ---" << std::endl;
    log(std::cout, LogLevel::DEBUG_LEVEL, "Debug message: value = {}", 42);
    log(std::cout, LogLevel::INFO_LEVEL, "Info message: status = {}", "OK");
    log(std::cout, LogLevel::WARNING_LEVEL, "Warning: memory at {}%", 85);
    log(std::cout, LogLevel::ERROR_LEVEL, "Error: file {} not found",
        "config.json");
}

void demonstratePrintToStream() {
    printSection("2. Print to Stream");

    std::cout << "--- printToStream ---" << std::endl;
    printToStream(std::cout, "Hello, {}!\n", "World");
    printToStream(std::cout, "Numbers: {}, {}, {}\n", 1, 2, 3);
    printToStream(std::cout, "Mixed: {} is {} years old\n", "Alice", 30);

    std::cout << "\n--- To string stream ---" << std::endl;
    std::stringstream ss;
    printToStream(ss, "Formatted: {} + {} = {}", 2, 3, 5);
    std::cout << "Result: " << ss.str() << std::endl;
}

void demonstrateFormatToStream() {
    printSection("3. Format to Stream");

    std::cout << "--- formatToStream ---" << std::endl;
    formatToStream(std::cout, "Simple text\n");
    formatToStream(std::cout, "With placeholder: {}\n", "value");
    formatToStream(std::cout, "Multiple: {}, {}, {}\n", "a", "b", "c");

    std::cout << "\n--- Building complex output ---" << std::endl;
    std::stringstream report;
    formatToStream(report, "=== Report ===\n");
    formatToStream(report, "Items processed: {}\n", 1000);
    formatToStream(report, "Success rate: {}%\n", 99.5);
    formatToStream(report, "Duration: {} seconds\n", 12.34);
    std::cout << report.str();
}

void demonstrateProgressBar() {
    printSection("4. Progress Bar Styles");

    std::cout << "--- Basic Style ---" << std::endl;
    for (int i = 0; i <= 100; i += 25) {
        std::cout << "  " << i << "%: ";
        int filled = i / 2;
        std::cout << "[";
        for (int j = 0; j < 50; ++j) {
            if (j < filled)
                std::cout << "=";
            else if (j == filled)
                std::cout << ">";
            else
                std::cout << " ";
        }
        std::cout << "]" << std::endl;
    }

    std::cout << "\n--- Block Style ---" << std::endl;
    for (int i = 0; i <= 100; i += 25) {
        std::cout << "  " << i << "%: ";
        int filled = i / 2;
        std::cout << "[";
        for (int j = 0; j < 50; ++j) {
            if (j < filled)
                std::cout << "\u2588";
            else
                std::cout << " ";
        }
        std::cout << "]" << std::endl;
    }
}

void demonstrateContainerPrinting() {
    printSection("5. Container Printing");

    std::vector<int> numbers = {1, 2, 3, 4, 5};
    std::vector<std::string> words = {"hello", "world", "test"};

    std::cout << "--- Vector of integers ---" << std::endl;
    std::cout << "  [";
    for (size_t i = 0; i < numbers.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << numbers[i];
    }
    std::cout << "]" << std::endl;

    std::cout << "\n--- Vector of strings ---" << std::endl;
    std::cout << "  [";
    for (size_t i = 0; i < words.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << "\"" << words[i] << "\"";
    }
    std::cout << "]" << std::endl;
}

void demonstrateFileLogging() {
    printSection("6. File Logging Example");

    std::stringstream fileSimulator;

    log(fileSimulator, LogLevel::INFO_LEVEL, "Application started");
    log(fileSimulator, LogLevel::INFO_LEVEL, "Loading configuration from {}",
        "config.yaml");
    log(fileSimulator, LogLevel::WARNING_LEVEL, "Deprecated API used in {}",
        "legacy.cpp");
    log(fileSimulator, LogLevel::INFO_LEVEL, "Server listening on port {}",
        8080);
    log(fileSimulator, LogLevel::ERROR_LEVEL,
        "Failed to connect to database: {}", "timeout");
    log(fileSimulator, LogLevel::INFO_LEVEL, "Retrying connection...");
    log(fileSimulator, LogLevel::INFO_LEVEL, "Database connected successfully");

    std::cout << "--- Simulated Log File ---" << std::endl;
    std::cout << fileSimulator.str();
}

void demonstrateBuildOutput() {
    printSection("7. Build System Output");

    std::cout << "--- Compilation Progress ---" << std::endl;

    std::vector<std::pair<std::string, bool>> files = {{"main.cpp", true},
                                                       {"utils.cpp", true},
                                                       {"network.cpp", true},
                                                       {"database.cpp", false},
                                                       {"cache.cpp", true}};

    for (const auto& [file, success] : files) {
        std::cout << "  Compiling " << file << "... ";
        if (success) {
            std::cout << "OK" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
        }
    }

    std::cout << "\n--- Build Summary ---" << std::endl;
    int succeeded = 0, failed = 0;
    for (const auto& [file, success] : files) {
        if (success)
            ++succeeded;
        else
            ++failed;
    }
    printToStream(std::cout, "  Succeeded: {}\n", succeeded);
    printToStream(std::cout, "  Failed: {}\n", failed);
    printToStream(std::cout, "  Total: {}\n", files.size());
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Print Utilities Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicLogging();
        demonstratePrintToStream();
        demonstrateFormatToStream();
        demonstrateProgressBar();
        demonstrateContainerPrinting();
        demonstrateFileLogging();
        demonstrateBuildOutput();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All print examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
