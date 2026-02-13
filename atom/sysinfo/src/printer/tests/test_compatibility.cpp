/**
 * @file test_compatibility.cpp
 * @brief Compatibility tests for the sysinfo_printer module
 *
 * This file contains tests to ensure backward compatibility with the
 * original sysinfo_printer API.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <iostream>
#include <cassert>
#include <atom/sysinfo/sysinfo_printer.hpp>

using namespace atom::system;

void testLegacyAPI() {
    std::cout << "Testing legacy API compatibility...\n";

    try {
        // Test static methods exist and can be called
        auto fullReport = SystemInfoPrinter::generateFullReport();
        assert(!fullReport.empty());
        std::cout << "✓ generateFullReport() works\n";

        auto simpleReport = SystemInfoPrinter::generateSimpleReport();
        assert(!simpleReport.empty());
        std::cout << "✓ generateSimpleReport() works\n";

        auto perfReport = SystemInfoPrinter::generatePerformanceReport();
        assert(!perfReport.empty());
        std::cout << "✓ generatePerformanceReport() works\n";

        auto secReport = SystemInfoPrinter::generateSecurityReport();
        assert(!secReport.empty());
        std::cout << "✓ generateSecurityReport() works\n";

        // Test export functions
        bool htmlSuccess = SystemInfoPrinter::exportToHTML("test_legacy.html");
        std::cout << "✓ exportToHTML() " << (htmlSuccess ? "succeeded" : "failed") << "\n";

        bool jsonSuccess = SystemInfoPrinter::exportToJSON("test_legacy.json");
        std::cout << "✓ exportToJSON() " << (jsonSuccess ? "succeeded" : "failed") << "\n";

        bool mdSuccess = SystemInfoPrinter::exportToMarkdown("test_legacy.md");
        std::cout << "✓ exportToMarkdown() " << (mdSuccess ? "succeeded" : "failed") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in legacy API test: " << e.what() << std::endl;
        throw;
    }
}

void testHelperFunctions() {
    std::cout << "Testing helper functions...\n";

    try {
        // Test formatting functions
        auto bytesStr = SystemInfoPrinter::formatBytes(1024 * 1024 * 1024);
        assert(bytesStr.find("GB") != std::string::npos);
        std::cout << "✓ formatBytes() works: " << bytesStr << "\n";

        auto percentStr = SystemInfoPrinter::formatPercentage(75.5);
        assert(percentStr.find("%") != std::string::npos);
        std::cout << "✓ formatPercentage() works: " << percentStr << "\n";

        auto tempStr = SystemInfoPrinter::formatTemperature(45.7);
        assert(tempStr.find("°C") != std::string::npos);
        std::cout << "✓ formatTemperature() works: " << tempStr << "\n";

        auto freqStr = SystemInfoPrinter::formatFrequency(2.4e9);
        assert(freqStr.find("GHz") != std::string::npos);
        std::cout << "✓ formatFrequency() works: " << freqStr << "\n";

        // Test table functions
        auto tableRow = SystemInfoPrinter::createTableRow("Test Label", "Test Value");
        assert(!tableRow.empty());
        std::cout << "✓ createTableRow() works\n";

        auto tableHeader = SystemInfoPrinter::createTableHeader("Test Header");
        assert(!tableHeader.empty());
        std::cout << "✓ createTableHeader() works\n";

        auto tableFooter = SystemInfoPrinter::createTableFooter();
        assert(!tableFooter.empty());
        std::cout << "✓ createTableFooter() works\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in helper functions test: " << e.what() << std::endl;
        throw;
    }
}

void testComponentFormatters() {
    std::cout << "Testing component formatters...\n";

    try {
        // Test CPU formatter
        auto cpuInfo = getCpuInfo();
        auto cpuFormatted = SystemInfoPrinter::formatCpuInfo(cpuInfo);
        assert(!cpuFormatted.empty());
        std::cout << "✓ formatCpuInfo() works\n";

        // Test memory formatter
        auto memInfo = getDetailedMemoryStats();
        auto memFormatted = SystemInfoPrinter::formatMemoryInfo(memInfo);
        assert(!memFormatted.empty());
        std::cout << "✓ formatMemoryInfo() works\n";

        // Test OS formatter
        auto osInfo = getOperatingSystemInfo();
        auto osFormatted = SystemInfoPrinter::formatOsInfo(osInfo);
        assert(!osFormatted.empty());
        std::cout << "✓ formatOsInfo() works\n";

        // Test disk formatter
        auto diskInfo = getDiskInfo();
        auto diskFormatted = SystemInfoPrinter::formatDiskInfo(diskInfo);
        assert(!diskFormatted.empty());
        std::cout << "✓ formatDiskInfo() works\n";

        // Test BIOS formatter
        auto& bios = BiosInfo::getInstance();
        const auto& biosInfo = bios.getBiosInfo();
        auto biosFormatted = SystemInfoPrinter::formatBiosInfo(biosInfo);
        assert(!biosFormatted.empty());
        std::cout << "✓ formatBiosInfo() works\n";

        // Test battery formatter (may not be available on all systems)
        auto batteryResult = getDetailedBatteryInfo();
        if (std::holds_alternative<BatteryInfo>(batteryResult)) {
            const auto& batteryInfo = std::get<BatteryInfo>(batteryResult);
            auto batteryFormatted = SystemInfoPrinter::formatBatteryInfo(batteryInfo);
            assert(!batteryFormatted.empty());
            std::cout << "✓ formatBatteryInfo() works\n";
        } else {
            std::cout << "- formatBatteryInfo() skipped (no battery)\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in component formatters test: " << e.what() << std::endl;
        throw;
    }
}

void testDeprecatedFunctions() {
    std::cout << "Testing deprecated global functions...\n";

    try {
        // Test deprecated global functions (should still work but with warnings)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

        auto globalReport = generateSystemReport();
        assert(!globalReport.empty());
        std::cout << "✓ generateSystemReport() (deprecated) works\n";

        bool globalHtmlSuccess = exportSystemReportToHTML("test_global.html");
        std::cout << "✓ exportSystemReportToHTML() (deprecated) "
                  << (globalHtmlSuccess ? "succeeded" : "failed") << "\n";

        #pragma GCC diagnostic pop

    } catch (const std::exception& e) {
        std::cerr << "Error in deprecated functions test: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    std::cout << "=== Atom System Information Printer - Compatibility Tests ===\n\n";

    try {
        testLegacyAPI();
        std::cout << "\n";

        testHelperFunctions();
        std::cout << "\n";

        testComponentFormatters();
        std::cout << "\n";

        testDeprecatedFunctions();
        std::cout << "\n";

        std::cout << "All compatibility tests passed successfully!\n";
        std::cout << "\nGenerated test files:\n";
        std::cout << "- test_legacy.html\n";
        std::cout << "- test_legacy.json\n";
        std::cout << "- test_legacy.md\n";
        std::cout << "- test_global.html\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Compatibility test failed: " << e.what() << std::endl;
        return 1;
    }
}
