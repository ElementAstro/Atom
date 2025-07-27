/**
 * @file export_examples.cpp
 * @brief Export examples for the sysinfo_printer module
 *
 * This example demonstrates various export formats and options
 * available in the sysinfo_printer module.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <atom/sysinfo/sysinfo_printer/printer.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/html_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/json_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/markdown_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/xml_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/csv_exporter.hpp>

using namespace atom::system;

void demonstrateBasicExports() {
    std::cout << "=== Basic Export Examples ===\n\n";

    try {
        // Create a printer instance
        SystemInfoPrinter printer;
        
        // Generate a full report
        auto report = printer.generateReport(ReportType::FULL);
        
        // Export to different formats
        std::cout << "1. Exporting to HTML...\n";
        bool htmlSuccess = printer.exportReport(report, "basic_export.html", ExportFormat::HTML);
        std::cout << "HTML export " << (htmlSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "2. Exporting to JSON...\n";
        bool jsonSuccess = printer.exportReport(report, "basic_export.json", ExportFormat::JSON);
        std::cout << "JSON export " << (jsonSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "3. Exporting to Markdown...\n";
        bool mdSuccess = printer.exportReport(report, "basic_export.md", ExportFormat::MARKDOWN);
        std::cout << "Markdown export " << (mdSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "4. Exporting to XML...\n";
        bool xmlSuccess = printer.exportReport(report, "basic_export.xml", ExportFormat::XML);
        std::cout << "XML export " << (xmlSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "5. Exporting to CSV...\n";
        bool csvSuccess = printer.exportReport(report, "basic_export.csv", ExportFormat::CSV);
        std::cout << "CSV export " << (csvSuccess ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic exports: " << e.what() << std::endl;
    }
}

void demonstrateCustomExportOptions() {
    std::cout << "=== Custom Export Options Examples ===\n\n";

    try {
        SystemInfoPrinter printer;
        auto report = printer.generateReport(ReportType::PERFORMANCE);
        
        // Configure custom export options
        ExportOptions options;
        options.title = "Custom Performance Report";
        options.description = "A detailed performance analysis of the system";
        options.author = "System Administrator";
        options.includeTimestamp = true;
        options.includeMetadata = true;
        options.prettyPrint = true;
        
        printer.setExportOptions(options);
        
        std::cout << "1. Custom HTML export with metadata...\n";
        bool htmlSuccess = printer.exportReport(report, "custom_performance.html", ExportFormat::HTML);
        std::cout << "Custom HTML export " << (htmlSuccess ? "successful" : "failed") << "\n\n";

        // Change options for JSON
        options.title = "Performance Data Export";
        options.description = "Machine-readable performance data";
        printer.setExportOptions(options);
        
        std::cout << "2. Custom JSON export with different metadata...\n";
        bool jsonSuccess = printer.exportReport(report, "custom_performance.json", ExportFormat::JSON);
        std::cout << "Custom JSON export " << (jsonSuccess ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in custom export options: " << e.what() << std::endl;
    }
}

void demonstrateDirectExporters() {
    std::cout << "=== Direct Exporter Usage Examples ===\n\n";

    try {
        // Generate content
        SystemInfoPrinter printer;
        auto content = printer.generateReport(ReportType::SIMPLE);
        
        // Use exporters directly
        std::cout << "1. Using HTML exporter directly...\n";
        ExportOptions htmlOptions;
        htmlOptions.title = "Direct HTML Export";
        htmlOptions.includeTimestamp = true;
        
        HtmlExporter htmlExporter(htmlOptions);
        bool htmlSuccess = htmlExporter.exportToFile(content, "direct_export.html");
        std::cout << "Direct HTML export " << (htmlSuccess ? "successful" : "failed") << "\n";
        
        // Get HTML as string
        auto htmlString = htmlExporter.exportToString(content);
        std::cout << "HTML string length: " << htmlString.length() << " characters\n\n";

        std::cout << "2. Using JSON exporter directly...\n";
        ExportOptions jsonOptions;
        jsonOptions.title = "Direct JSON Export";
        jsonOptions.prettyPrint = true;
        
        JsonExporter jsonExporter(jsonOptions);
        bool jsonSuccess = jsonExporter.exportToFile(content, "direct_export.json");
        std::cout << "Direct JSON export " << (jsonSuccess ? "successful" : "failed") << "\n";
        
        auto jsonString = jsonExporter.exportToString(content);
        std::cout << "JSON string length: " << jsonString.length() << " characters\n\n";

        std::cout << "3. Using Markdown exporter directly...\n";
        MarkdownExporter mdExporter;
        bool mdSuccess = mdExporter.exportToFile(content, "direct_export.md");
        std::cout << "Direct Markdown export " << (mdSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "4. Using XML exporter directly...\n";
        XmlExporter xmlExporter;
        bool xmlSuccess = xmlExporter.exportToFile(content, "direct_export.xml");
        std::cout << "Direct XML export " << (xmlSuccess ? "successful" : "failed") << "\n\n";

        std::cout << "5. Using CSV exporter directly...\n";
        CsvExporter csvExporter;
        bool csvSuccess = csvExporter.exportToFile(content, "direct_export.csv");
        std::cout << "Direct CSV export " << (csvSuccess ? "successful" : "failed") << "\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in direct exporter usage: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Export Examples ===\n\n";

    try {
        demonstrateBasicExports();
        demonstrateCustomExportOptions();
        demonstrateDirectExporters();

        std::cout << "Export examples completed successfully!\n";
        std::cout << "\nGenerated files:\n";
        std::cout << "- basic_export.html, .json, .md, .xml, .csv\n";
        std::cout << "- custom_performance.html, .json\n";
        std::cout << "- direct_export.html, .json, .md, .xml, .csv\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
