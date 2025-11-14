/**
 * @file crash_example.cpp
 * @brief Example demonstrating crash logging and system information collection
 *
 * This example shows how to use the crash reporting functionality to:
 * - Save crash logs with detailed information
 * - Collect comprehensive system information
 * - Handle crash scenarios gracefully
 */

#include "atom/system/crash.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace atom::system;

void demonstrateSystemInfo() {
    std::cout << "\n=== System Information Collection ===" << std::endl;

    try {
        std::string sysInfo = getSystemInfo();
        std::cout << "System Information:" << std::endl;
        std::cout << sysInfo << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to get system info: " << e.what() << std::endl;
    }
}

void demonstrateCrashLog() {
    std::cout << "\n=== Crash Log Creation ===" << std::endl;

    try {
        // Simulate a crash scenario
        std::string errorMsg =
            "Example crash: Simulated error for demonstration purposes\n"
            "Error Code: 0x12345678\n"
            "Location: example/system/debug/crash_example.cpp:35\n"
            "Description: This is a test crash log entry";

        saveCrashLog(errorMsg);
        std::cout << "✓ Crash log saved successfully" << std::endl;
        std::cout << "  Check the crash log directory for the generated file"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to save crash log: " << e.what() << std::endl;
    }
}

void demonstrateExceptionHandling() {
    std::cout << "\n=== Exception Handling with Crash Logging ===" << std::endl;

    try {
        // Simulate an exception
        throw std::runtime_error("Simulated runtime error");
    } catch (const std::exception& e) {
        std::cout << "✓ Caught exception: " << e.what() << std::endl;

        // Log the crash
        std::string crashMsg = std::string("Exception caught: ") + e.what() +
                               "\n" + "Type: std::runtime_error\n" +
                               "Handler: demonstrateExceptionHandling()";

        try {
            saveCrashLog(crashMsg);
            std::cout << "✓ Exception logged to crash file" << std::endl;
        } catch (const std::exception& logError) {
            std::cerr << "✗ Failed to log exception: " << logError.what()
                      << std::endl;
        }
    }
}

void demonstrateDetailedCrashReport() {
    std::cout << "\n=== Detailed Crash Report ===" << std::endl;

    try {
        // Create a detailed crash report
        std::string detailedReport =
            "=== CRASH REPORT ===\n"
            "Timestamp: " +
            std::string(__DATE__) + " " + std::string(__TIME__) +
            "\n"
            "Application: Atom System Example\n"
            "Module: crash_example\n"
            "Severity: CRITICAL\n"
            "\n"
            "Error Details:\n"
            "  - Type: Segmentation Fault (simulated)\n"
            "  - Address: 0x00000000\n"
            "  - Instruction: MOV [NULL], EAX\n"
            "\n"
            "Stack Trace:\n"
            "  #0 demonstrateDetailedCrashReport() at crash_example.cpp:75\n"
            "  #1 main() at crash_example.cpp:120\n"
            "\n"
            "Additional Context:\n"
            "  - User Action: Running crash example\n"
            "  - System State: Normal operation\n"
            "  - Memory Usage: Within normal limits\n";

        saveCrashLog(detailedReport);
        std::cout << "✓ Detailed crash report saved" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to save detailed report: " << e.what()
                  << std::endl;
    }
}

void demonstrateMultipleCrashLogs() {
    std::cout << "\n=== Multiple Crash Logs ===" << std::endl;

    const char* scenarios[] = {"Null pointer dereference",
                               "Out of bounds access", "Stack overflow",
                               "Heap corruption"};

    for (const auto& scenario : scenarios) {
        try {
            std::string msg = std::string("Crash Scenario: ") + scenario;
            saveCrashLog(msg);
            std::cout << "✓ Logged: " << scenario << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "✗ Failed to log " << scenario << ": " << e.what()
                      << std::endl;
        }
    }
}

int main() {
    std::cout << "Crash Logging and System Information Example" << std::endl;
    std::cout << "=============================================" << std::endl;

    try {
        demonstrateSystemInfo();
        demonstrateCrashLog();
        demonstrateExceptionHandling();
        demonstrateDetailedCrashReport();
        demonstrateMultipleCrashLogs();

        std::cout << "\n✓ All crash logging operations completed!" << std::endl;
        std::cout
            << "\nNote: Check your crash log directory for generated files."
            << std::endl;
        std::cout
            << "      These logs include system information, stack traces,"
            << std::endl;
        std::cout << "      and environment details useful for debugging."
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
