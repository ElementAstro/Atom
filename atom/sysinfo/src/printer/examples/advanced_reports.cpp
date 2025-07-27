/**
 * @file advanced_reports.cpp
 * @brief Advanced reporting examples for the sysinfo_printer module
 *
 * This example demonstrates advanced reporting features including
 * custom reports, performance monitoring, and specialized formatters.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <atom/sysinfo/sysinfo_printer/printer.hpp>
#include <atom/sysinfo/sysinfo_printer/reports/custom_report.hpp>
#include <atom/sysinfo/sysinfo_printer/reports/performance_report.hpp>
#include <atom/sysinfo/sysinfo_printer/utils/performance_monitor.hpp>
#include <atom/sysinfo/sysinfo_printer/utils/cache.hpp>

using namespace atom::system;
using namespace atom::system::utils;

void demonstrateCustomReports() {
    std::cout << "=== Custom Report Examples ===\n\n";

    try {
        // Create a custom report with specific sections
        ReportOptions options;
        options.type = ReportType::CUSTOM;
        options.title = "Hardware Analysis Report";
        options.description = "Focused analysis of hardware components";
        options.style = FormatterStyle::DETAILED;
        options.sections = {
            ReportSection::CPU,
            ReportSection::MEMORY,
            ReportSection::GPU,
            ReportSection::DISK
        };

        CustomReport customReport(options);
        auto content = customReport.generate();
        
        std::cout << "1. Custom hardware report generated (" << content.length() << " characters)\n";
        
        // Export the custom report
        SystemInfoPrinter printer;
        bool success = printer.exportReport(content, "hardware_analysis.html", ExportFormat::HTML);
        std::cout << "Custom report export " << (success ? "successful" : "failed") << "\n\n";

        // Create another custom report with different focus
        options.title = "System Environment Report";
        options.description = "Analysis of system environment and configuration";
        options.sections = {
            ReportSection::OS,
            ReportSection::SYSTEM,
            ReportSection::LOCALE,
            ReportSection::NETWORK
        };

        CustomReport envReport(options);
        auto envContent = envReport.generate();
        
        std::cout << "2. Custom environment report generated (" << envContent.length() << " characters)\n";
        
        success = printer.exportReport(envContent, "environment_analysis.md", ExportFormat::MARKDOWN);
        std::cout << "Environment report export " << (success ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in custom reports: " << e.what() << std::endl;
    }
}

void demonstratePerformanceReports() {
    std::cout << "=== Performance Report Examples ===\n\n";

    try {
        // Enable performance monitoring
        auto& monitor = PerformanceMonitor::getInstance();
        monitor.setEnabled(true);
        
        {
            PERF_TIMER_CATEGORY("report_generation", "reporting");
            
            // Create a performance-focused report
            PerformanceReport perfReport;
            auto content = perfReport.generate();
            
            std::cout << "1. Performance report generated (" << content.length() << " characters)\n";
            
            // Export with timing
            SystemInfoPrinter printer;
            bool success = printer.exportReport(content, "performance_analysis.html", ExportFormat::HTML);
            std::cout << "Performance report export " << (success ? "successful" : "failed") << "\n\n";
        }
        
        // Show performance metrics
        auto summary = monitor.getSummary();
        std::cout << "2. Performance monitoring results:\n";
        std::cout << summary << "\n";
        
        monitor.clear();
        monitor.setEnabled(false);

    } catch (const std::exception& e) {
        std::cerr << "Error in performance reports: " << e.what() << std::endl;
    }
}

void demonstrateCaching() {
    std::cout << "=== Caching Examples ===\n\n";

    try {
        auto& cache = SystemInfoCache::getInstance();
        
        // Demonstrate string caching
        std::cout << "1. Testing string cache...\n";
        
        auto getValue = []() -> std::string {
            std::cout << "   Computing expensive operation...\n";
            return "Cached system information";
        };
        
        // First call - should compute
        auto value1 = cache.stringCache.getOrCompute("test_key", getValue);
        std::cout << "   First call result: " << value1 << "\n";
        
        // Second call - should use cache
        auto value2 = cache.stringCache.getOrCompute("test_key", getValue);
        std::cout << "   Second call result: " << value2 << "\n";
        
        std::cout << "   Cache size: " << cache.stringCache.size() << "\n\n";

        // Demonstrate numeric caching
        std::cout << "2. Testing numeric cache...\n";
        
        auto getNumeric = []() -> double {
            std::cout << "   Computing numeric value...\n";
            return 42.5;
        };
        
        auto num1 = cache.numericCache.getOrCompute("numeric_key", getNumeric);
        auto num2 = cache.numericCache.getOrCompute("numeric_key", getNumeric);
        
        std::cout << "   Numeric values: " << num1 << ", " << num2 << "\n";
        std::cout << "   Cache size: " << cache.numericCache.size() << "\n\n";

        // Clear caches
        cache.stringCache.clear();
        cache.numericCache.clear();
        std::cout << "3. Caches cleared\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in caching demo: " << e.what() << std::endl;
    }
}

void demonstrateAdvancedFormatting() {
    std::cout << "=== Advanced Formatting Examples ===\n\n";

    try {
        SystemInfoPrinter printer;
        
        // Configure advanced formatting options
        FormatterOptions fmtOptions;
        fmtOptions.style = FormatterStyle::VERBOSE;
        fmtOptions.colorEnabled = true;
        fmtOptions.timestampEnabled = true;
        fmtOptions.tableWidth = 120;
        fmtOptions.indentation = "  ";
        
        printer.setFormatterOptions(fmtOptions);
        
        std::cout << "1. Generating report with verbose formatting...\n";
        auto verboseReport = printer.generateReport(ReportType::FULL);
        std::cout << "Verbose report generated (" << verboseReport.length() << " characters)\n\n";
        
        // Change to compact formatting
        fmtOptions.style = FormatterStyle::COMPACT;
        fmtOptions.colorEnabled = false;
        fmtOptions.tableWidth = 80;
        
        printer.setFormatterOptions(fmtOptions);
        
        std::cout << "2. Generating report with compact formatting...\n";
        auto compactReport = printer.generateReport(ReportType::SIMPLE);
        std::cout << "Compact report generated (" << compactReport.length() << " characters)\n\n";
        
        // Export both with different options
        ExportOptions exportOptions;
        exportOptions.title = "Advanced Formatting Demo";
        exportOptions.includeTimestamp = true;
        
        printer.setExportOptions(exportOptions);
        
        bool verboseSuccess = printer.exportReport(verboseReport, "verbose_format.html", ExportFormat::HTML);
        bool compactSuccess = printer.exportReport(compactReport, "compact_format.html", ExportFormat::HTML);
        
        std::cout << "3. Export results:\n";
        std::cout << "   Verbose: " << (verboseSuccess ? "successful" : "failed") << "\n";
        std::cout << "   Compact: " << (compactSuccess ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in advanced formatting: " << e.what() << std::endl;
    }
}

void demonstrateReportComparison() {
    std::cout << "=== Report Comparison Examples ===\n\n";

    try {
        SystemInfoPrinter printer;
        
        // Generate different types of reports
        auto fullReport = printer.generateReport(ReportType::FULL);
        auto simpleReport = printer.generateReport(ReportType::SIMPLE);
        auto perfReport = printer.generateReport(ReportType::PERFORMANCE);
        
        std::cout << "Report size comparison:\n";
        std::cout << "  Full Report:        " << fullReport.length() << " characters\n";
        std::cout << "  Simple Report:      " << simpleReport.length() << " characters\n";
        std::cout << "  Performance Report: " << perfReport.length() << " characters\n\n";
        
        // Export all for comparison
        bool fullSuccess = printer.exportReport(fullReport, "comparison_full.json", ExportFormat::JSON);
        bool simpleSuccess = printer.exportReport(simpleReport, "comparison_simple.json", ExportFormat::JSON);
        bool perfSuccess = printer.exportReport(perfReport, "comparison_performance.json", ExportFormat::JSON);
        
        std::cout << "Comparison exports:\n";
        std::cout << "  Full:        " << (fullSuccess ? "successful" : "failed") << "\n";
        std::cout << "  Simple:      " << (simpleSuccess ? "successful" : "failed") << "\n";
        std::cout << "  Performance: " << (perfSuccess ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in report comparison: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Advanced Reports ===\n\n";

    try {
        demonstrateCustomReports();
        demonstratePerformanceReports();
        demonstrateCaching();
        demonstrateAdvancedFormatting();
        demonstrateReportComparison();

        std::cout << "Advanced reporting examples completed successfully!\n";
        std::cout << "\nGenerated files:\n";
        std::cout << "- hardware_analysis.html\n";
        std::cout << "- environment_analysis.md\n";
        std::cout << "- performance_analysis.html\n";
        std::cout << "- verbose_format.html\n";
        std::cout << "- compact_format.html\n";
        std::cout << "- comparison_*.json (3 files)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
