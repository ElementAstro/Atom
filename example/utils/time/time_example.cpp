/**
 * @file time_example.cpp
 * @brief Examples for atom::utils time utilities
 */

#include "atom/utils/time/time.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

void demonstrateTimestampStrings() {
    printSection("1. Timestamp Strings");

    std::cout << "--- getChinaTimestampString ---" << std::endl;
    std::string chinaTs = getChinaTimestampString();
    std::cout << "  China timestamp: " << chinaTs << std::endl;

    std::cout << "\n--- getTimestampString ---" << std::endl;
    std::string localTs = getTimestampString();
    std::cout << "  Local timestamp: " << localTs << std::endl;
}

void demonstrateTimeFormatting() {
    printSection("2. Time Formatting");

    auto now = std::chrono::system_clock::now();
    std::cout << "Current time in various formats:" << std::endl;
    std::cout << "  ISO: " << formatTime(now, "%Y-%m-%dT%H:%M:%S") << std::endl;
    std::cout << "  Date: " << formatTime(now, "%Y-%m-%d") << std::endl;
    std::cout << "  Time: " << formatTime(now, "%H:%M:%S") << std::endl;
}

void demonstrateTimeDifference() {
    printSection("3. Time Difference");

    auto start = std::chrono::system_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto end = std::chrono::system_clock::now();

    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Elapsed: " << diff.count() << " ms" << std::endl;
}

void demonstrateLogging() {
    printSection("4. Logging with Timestamps");

    auto logEntry = [](const std::string& level, const std::string& msg) {
        std::cout << "[" << getTimestampString() << "] [" << level << "] " << msg << std::endl;
    };

    logEntry("INFO", "Application started");
    logEntry("DEBUG", "Loading configuration");
    logEntry("INFO", "Ready");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Time Utilities Examples" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateTimestampStrings();
        demonstrateTimeFormatting();
        demonstrateTimeDifference();
        demonstrateLogging();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All time examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
