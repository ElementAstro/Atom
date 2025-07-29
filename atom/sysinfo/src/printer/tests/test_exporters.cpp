/**
 * @file test_exporters.cpp
 * @brief Tests for exporter classes
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <cassert>
#include <fstream>
#include <atom/sysinfo/sysinfo_printer/exporters/html_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/json_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/markdown_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/xml_exporter.hpp>
#include <atom/sysinfo/sysinfo_printer/exporters/csv_exporter.hpp>

using namespace atom::system;

const std::string TEST_CONTENT = R"(
=== System Information ===
| Component | Value |
| CPU | Test CPU Model |
| Memory | 16 GB |
| Disk | 500 GB SSD |
========================
)";

bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

void testHtmlExporter() {
    std::cout << "Testing HTML exporter...\n";

    try {
        // Test basic export
        HtmlExporter exporter;

        // Test file extension and MIME type
        assert(exporter.getFileExtension() == ".html");
        assert(exporter.getMimeType() == "text/html");
        std::cout << "✓ HTML exporter metadata correct\n";

        // Test string export
        auto htmlString = exporter.exportToString(TEST_CONTENT);
        assert(!htmlString.empty());
        assert(htmlString.find("<!DOCTYPE html>") != std::string::npos);
        std::cout << "✓ HTML string export works\n";

        // Test file export
        bool success = exporter.exportToFile(TEST_CONTENT, "test_export.html");
        assert(success);
        assert(fileExists("test_export.html"));
        std::cout << "✓ HTML file export works\n";

        // Test with custom options
        ExportOptions options;
        options.title = "Test Report";
        options.includeTimestamp = true;

        HtmlExporter customExporter(options);
        auto customHtml = customExporter.exportToString(TEST_CONTENT);
        assert(customHtml.find("Test Report") != std::string::npos);
        std::cout << "✓ HTML custom options work\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in HTML exporter test: " << e.what() << std::endl;
        throw;
    }
}

void testJsonExporter() {
    std::cout << "Testing JSON exporter...\n";

    try {
        JsonExporter exporter;

        // Test metadata
        assert(exporter.getFileExtension() == ".json");
        assert(exporter.getMimeType() == "application/json");
        std::cout << "✓ JSON exporter metadata correct\n";

        // Test string export
        auto jsonString = exporter.exportToString(TEST_CONTENT);
        assert(!jsonString.empty());
        assert(jsonString.find("{") != std::string::npos);
        std::cout << "✓ JSON string export works\n";

        // Test file export
        bool success = exporter.exportToFile(TEST_CONTENT, "test_export.json");
        assert(success);
        assert(fileExists("test_export.json"));
        std::cout << "✓ JSON file export works\n";

        // Test pretty print option
        ExportOptions options;
        options.prettyPrint = true;

        JsonExporter prettyExporter(options);
        auto prettyJson = prettyExporter.exportToString(TEST_CONTENT);
        assert(!prettyJson.empty());
        std::cout << "✓ JSON pretty print works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in JSON exporter test: " << e.what() << std::endl;
        throw;
    }
}

void testMarkdownExporter() {
    std::cout << "Testing Markdown exporter...\n";

    try {
        MarkdownExporter exporter;

        // Test metadata
        assert(exporter.getFileExtension() == ".md");
        assert(exporter.getMimeType() == "text/markdown");
        std::cout << "✓ Markdown exporter metadata correct\n";

        // Test string export
        auto mdString = exporter.exportToString(TEST_CONTENT);
        assert(!mdString.empty());
        assert(mdString.find("#") != std::string::npos); // Should have headers
        std::cout << "✓ Markdown string export works\n";

        // Test file export
        bool success = exporter.exportToFile(TEST_CONTENT, "test_export.md");
        assert(success);
        assert(fileExists("test_export.md"));
        std::cout << "✓ Markdown file export works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in Markdown exporter test: " << e.what() << std::endl;
        throw;
    }
}

void testXmlExporter() {
    std::cout << "Testing XML exporter...\n";

    try {
        XmlExporter exporter;

        // Test metadata
        assert(exporter.getFileExtension() == ".xml");
        assert(exporter.getMimeType() == "application/xml");
        std::cout << "✓ XML exporter metadata correct\n";

        // Test string export
        auto xmlString = exporter.exportToString(TEST_CONTENT);
        assert(!xmlString.empty());
        assert(xmlString.find("<?xml") != std::string::npos);
        assert(xmlString.find("<system_information>") != std::string::npos);
        std::cout << "✓ XML string export works\n";

        // Test file export
        bool success = exporter.exportToFile(TEST_CONTENT, "test_export.xml");
        assert(success);
        assert(fileExists("test_export.xml"));
        std::cout << "✓ XML file export works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in XML exporter test: " << e.what() << std::endl;
        throw;
    }
}

void testCsvExporter() {
    std::cout << "Testing CSV exporter...\n";

    try {
        CsvExporter exporter;

        // Test metadata
        assert(exporter.getFileExtension() == ".csv");
        assert(exporter.getMimeType() == "text/csv");
        std::cout << "✓ CSV exporter metadata correct\n";

        // Test string export
        auto csvString = exporter.exportToString(TEST_CONTENT);
        assert(!csvString.empty());
        assert(csvString.find("Component,Property,Value") != std::string::npos);
        std::cout << "✓ CSV string export works\n";

        // Test file export
        bool success = exporter.exportToFile(TEST_CONTENT, "test_export.csv");
        assert(success);
        assert(fileExists("test_export.csv"));
        std::cout << "✓ CSV file export works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in CSV exporter test: " << e.what() << std::endl;
        throw;
    }
}

void testExportOptions() {
    std::cout << "Testing export options...\n";

    try {
        ExportOptions options;
        options.title = "Test Title";
        options.description = "Test Description";
        options.author = "Test Author";
        options.includeTimestamp = true;
        options.includeMetadata = true;

        // Test with HTML exporter
        HtmlExporter htmlExporter(options);
        auto htmlOutput = htmlExporter.exportToString(TEST_CONTENT);
        assert(htmlOutput.find("Test Title") != std::string::npos);
        std::cout << "✓ Export options work with HTML\n";

        // Test with JSON exporter
        JsonExporter jsonExporter(options);
        auto jsonOutput = jsonExporter.exportToString(TEST_CONTENT);
        assert(jsonOutput.find("Test Title") != std::string::npos);
        std::cout << "✓ Export options work with JSON\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in export options test: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Exporter Tests ===\n\n";

    try {
        testHtmlExporter();
        std::cout << "\n";

        testJsonExporter();
        std::cout << "\n";

        testMarkdownExporter();
        std::cout << "\n";

        testXmlExporter();
        std::cout << "\n";

        testCsvExporter();
        std::cout << "\n";

        testExportOptions();
        std::cout << "\n";

        std::cout << "All exporter tests passed successfully!\n";
        std::cout << "\nGenerated test files:\n";
        std::cout << "- test_export.html\n";
        std::cout << "- test_export.json\n";
        std::cout << "- test_export.md\n";
        std::cout << "- test_export.xml\n";
        std::cout << "- test_export.csv\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exporter test failed: " << e.what() << std::endl;
        return 1;
    }
}
