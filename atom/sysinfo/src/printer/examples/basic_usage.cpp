/**
 * @file basic_usage.cpp
 * @brief Basic usage example for the sysinfo_printer module
 *
 * This example demonstrates the basic functionality of the sysinfo_printer
 * module including generating reports and exporting to different formats.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <atom/sysinfo/sysinfo_printer/printer.hpp>

using namespace atom::system;

int main() {
    std::cout << "=== Atom System Information Printer - Basic Usage Example ===\n\n";

    try {
        // Create a printer instance
        SystemInfoPrinter printer;

        // Example 1: Generate a full system report
        std::cout << "1. Generating full system report...\n";
        auto fullReport = printer.generateReport(ReportType::FULL);
        std::cout << "Full report generated (" << fullReport.length() << " characters)\n\n";

        // Example 2: Generate a simple report
        std::cout << "2. Generating simple system report...\n";
        auto simpleReport = printer.generateReport(ReportType::SIMPLE);
        std::cout << "Simple report generated (" << simpleReport.length() << " characters)\n\n";

        // Example 3: Export to HTML
        std::cout << "3. Exporting full report to HTML...\n";
        bool htmlSuccess = printer.exportReport(fullReport, "system_report.html", ExportFormat::HTML);
        std::cout << "HTML export " << (htmlSuccess ? "successful" : "failed") << "\n\n";

        // Example 4: Export to JSON
        std::cout << "4. Exporting full report to JSON...\n";
        bool jsonSuccess = printer.exportReport(fullReport, "system_report.json", ExportFormat::JSON);
        std::cout << "JSON export " << (jsonSuccess ? "successful" : "failed") << "\n\n";

        // Example 5: Using legacy API for backward compatibility
        std::cout << "5. Using legacy API...\n";
        auto legacyReport = SystemInfoPrinter::generateFullReport();
        std::cout << "Legacy report generated (" << legacyReport.length() << " characters)\n";
        
        bool legacyHtmlSuccess = SystemInfoPrinter::exportToHTML("legacy_report.html");
        std::cout << "Legacy HTML export " << (legacyHtmlSuccess ? "successful" : "failed") << "\n\n";

        // Example 6: Display a portion of the report
        std::cout << "6. Sample output (first 500 characters):\n";
        std::cout << "----------------------------------------\n";
        std::cout << fullReport.substr(0, 500);
        if (fullReport.length() > 500) {
            std::cout << "...\n";
        }
        std::cout << "----------------------------------------\n\n";

        std::cout << "Basic usage example completed successfully!\n";
        std::cout << "\nGenerated files:\n";
        std::cout << "- system_report.html (Enhanced HTML report)\n";
        std::cout << "- system_report.json (JSON data export)\n";
        std::cout << "- legacy_report.html (Legacy compatibility)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
