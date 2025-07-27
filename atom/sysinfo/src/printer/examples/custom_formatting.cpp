/**
 * @file custom_formatting.cpp
 * @brief Custom formatting example for the sysinfo_printer module
 *
 * This example demonstrates how to use custom formatting options,
 * create specialized formatters, and customize the output appearance.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <atom/sysinfo/sysinfo_printer/printer.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/cpu_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/memory_formatter.hpp>
#include <atom/sysinfo/sysinfo_printer/formatters/battery_formatter.hpp>

using namespace atom::system;

void demonstrateFormatterStyles() {
    std::cout << "=== Formatter Styles Demonstration ===\n\n";

    try {
        // Get CPU information
        auto cpuInfo = getCpuInfo();

        // Create formatters with different styles
        std::vector<FormatterStyle> styles = {
            FormatterStyle::MINIMAL,
            FormatterStyle::COMPACT,
            FormatterStyle::STANDARD,
            FormatterStyle::DETAILED,
            FormatterStyle::VERBOSE
        };

        std::vector<std::string> styleNames = {
            "MINIMAL", "COMPACT", "STANDARD", "DETAILED", "VERBOSE"
        };

        for (size_t i = 0; i < styles.size(); ++i) {
            std::cout << "--- " << styleNames[i] << " Style ---\n";
            
            FormatterOptions options;
            options.style = styles[i];
            options.colorEnabled = false; // Disable colors for console output
            
            CpuFormatter formatter(options);
            auto formatted = formatter.format(cpuInfo, "CPU Information (" + styleNames[i] + ")");
            
            std::cout << formatted << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in formatter styles demo: " << e.what() << std::endl;
    }
}

void demonstrateOutputFormats() {
    std::cout << "=== Output Formats Demonstration ===\n\n";

    try {
        // Get memory information
        auto memInfo = getDetailedMemoryStats();

        // Create formatters with different output formats
        std::vector<OutputFormat> formats = {
            OutputFormat::TABLE,
            OutputFormat::PLAIN_TEXT,
            OutputFormat::JSON,
            OutputFormat::MARKDOWN
        };

        std::vector<std::string> formatNames = {
            "TABLE", "PLAIN_TEXT", "JSON", "MARKDOWN"
        };

        for (size_t i = 0; i < formats.size(); ++i) {
            std::cout << "--- " << formatNames[i] << " Format ---\n";
            
            FormatterOptions options;
            options.format = formats[i];
            options.style = FormatterStyle::STANDARD;
            
            MemoryFormatter formatter(options);
            auto formatted = formatter.format(memInfo, "Memory Information (" + formatNames[i] + ")");
            
            std::cout << formatted << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in output formats demo: " << e.what() << std::endl;
    }
}

void demonstrateCustomOptions() {
    std::cout << "=== Custom Options Demonstration ===\n\n";

    try {
        // Get battery information
        auto batteryResult = getDetailedBatteryInfo();
        if (!std::holds_alternative<BatteryInfo>(batteryResult)) {
            std::cout << "Battery not available for custom options demo\n\n";
            return;
        }

        const auto& batteryInfo = std::get<BatteryInfo>(batteryResult);

        // Create formatter with custom options
        FormatterOptions options;
        options.style = FormatterStyle::DETAILED;
        options.colorEnabled = true; // Enable colors
        options.timestampEnabled = true;
        options.tableWidth = 100;
        options.indentation = "    "; // 4-space indentation

        BatteryFormatter formatter(options);
        
        // Set custom options
        formatter.setCustomOption("show_icons", "true");
        formatter.setCustomOption("progress_bar_width", "25");
        formatter.setCustomOption("temperature_unit", "celsius");

        auto formatted = formatter.format(batteryInfo, "Battery Information (Custom)");
        std::cout << formatted << "\n";

        // Demonstrate different table widths
        std::cout << "--- Different Table Widths ---\n";
        std::vector<int> widths = {60, 80, 120};
        
        for (int width : widths) {
            options.tableWidth = width;
            BatteryFormatter widthFormatter(options);
            auto widthFormatted = widthFormatter.formatBasic(batteryInfo);
            std::cout << "Width " << width << ": " << widthFormatted << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in custom options demo: " << e.what() << std::endl;
    }
}

void demonstrateReportCustomization() {
    std::cout << "=== Report Customization Demonstration ===\n\n";

    try {
        // Create a custom report with specific sections
        ReportOptions reportOptions;
        reportOptions.type = ReportType::CUSTOM;
        reportOptions.style = FormatterStyle::DETAILED;
        reportOptions.format = OutputFormat::TABLE;
        reportOptions.title = "Custom System Report";
        reportOptions.includeTimestamp = true;
        reportOptions.colorEnabled = false;

        // Add only specific sections
        reportOptions.sections = {
            ReportSection::OS,
            ReportSection::CPU,
            ReportSection::MEMORY
        };

        SystemInfoPrinter printer;
        auto customReport = printer.generateReport(ReportType::CUSTOM, reportOptions);
        
        std::cout << "Custom report with selected sections:\n";
        std::cout << customReport.substr(0, 1000); // Show first 1000 characters
        if (customReport.length() > 1000) {
            std::cout << "...\n";
        }
        std::cout << "\n";

        // Export with custom options
        ExportOptions exportOptions;
        exportOptions.title = "Custom Formatted Report";
        exportOptions.description = "A customized system information report";
        exportOptions.author = "System Administrator";
        exportOptions.includeTimestamp = true;
        exportOptions.prettyPrint = true;

        printer.setExportOptions(exportOptions);
        bool success = printer.exportReport(customReport, "custom_report.html", ExportFormat::HTML);
        std::cout << "Custom HTML export " << (success ? "successful" : "failed") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in report customization demo: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Custom Formatting Example ===\n\n";

    try {
        demonstrateFormatterStyles();
        demonstrateOutputFormats();
        demonstrateCustomOptions();
        demonstrateReportCustomization();

        std::cout << "Custom formatting example completed successfully!\n";
        std::cout << "\nGenerated files:\n";
        std::cout << "- custom_report.html (Customized HTML report)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
