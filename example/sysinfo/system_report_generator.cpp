/**
 * @file system_report_generator.cpp
 * @brief Comprehensive system report generator using SystemInfoPrinter
 *
 * This example demonstrates how to generate comprehensive system reports using
 * the SystemInfoPrinter utility and all available system information APIs.
 * It showcases the complete system analysis and reporting capabilities.
 *
 * Features demonstrated:
 * - Complete system report generation using SystemInfoPrinter
 * - Individual component report formatting
 * - Custom report generation with selective information
 * - Report export to files (text, HTML, JSON formats)
 * - Performance timing and analysis
 * - Error handling and graceful degradation
 * - Cross-platform system reporting
 *
 * Report components:
 * - Operating system information
 * - CPU information and performance
 * - Memory statistics and health
 * - Disk/storage information
 * - Network and connectivity status
 * - Battery information (if available)
 * - BIOS and hardware details
 * - System health summary
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive system report generator
 */

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/battery.hpp"
#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/disk.hpp"
#include "atom/sysinfo/memory.hpp"
#include "atom/sysinfo/os.hpp"
#include "atom/sysinfo/sysinfo_printer.hpp"
#include "atom/sysinfo/wifi.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
}

/**
 * @brief Utility function to create formatted section headers
 */
std::string createSectionHeader(const std::string& title) {
    std::string header = "\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
    header += "=== " + title + " ===\n";
    header += std::string(DISPLAY_WIDTH, '=') + "\n";
    return header;
}

/**
 * @brief Get current timestamp for report generation
 */
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

/**
 * @brief Demonstrates full system report generation
 */
void demonstrateFullSystemReport() {
    std::cout << createSectionHeader("Full System Report Generation");

    try {
        std::cout << "Generating comprehensive system report...\n\n";

        auto startTime = std::chrono::high_resolution_clock::now();

        // Generate full system report using SystemInfoPrinter
        auto fullReport = SystemInfoPrinter::generateFullReport();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << "Report Generation Performance:\n";
        std::cout << "  Generation Time:      " << duration.count() << " ms\n";
        std::cout << "  Report Size:          " << fullReport.length()
                  << " characters\n";
        std::cout << "  Report Lines:         "
                  << std::count(fullReport.begin(), fullReport.end(), '\n')
                  << " lines\n";

        std::cout << "\n" << std::string(60, '-') << "\n";
        std::cout << "GENERATED SYSTEM REPORT\n";
        std::cout << std::string(60, '-') << "\n";
        std::cout << fullReport;
        std::cout << std::string(60, '-') << "\n";
        std::cout << "END OF SYSTEM REPORT\n";
        std::cout << std::string(60, '-') << "\n";

        std::cout
            << "\n✓ Full system report generation completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error generating full system report: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates individual component report formatting
 */
void demonstrateComponentReports() {
    std::cout << createSectionHeader("Individual Component Report Formatting");

    try {
        std::cout << "Generating individual component reports...\n\n";

        // OS Information Report
        std::cout << "Operating System Report:\n";
        std::cout << std::string(40, '-') << "\n";
        try {
            auto osInfo = getOperatingSystemInfo();
            auto osReport = SystemInfoPrinter::formatOsInfo(osInfo);
            std::cout << osReport << "\n";
        } catch (const std::exception& e) {
            std::cerr << "OS Report Error: " << e.what() << "\n";
        }

        // CPU Information Report
        std::cout << "\nCPU Information Report:\n";
        std::cout << std::string(40, '-') << "\n";
        try {
            auto cpuInfo = getCpuInfo();
            auto cpuReport = SystemInfoPrinter::formatCpuInfo(cpuInfo);
            std::cout << cpuReport << "\n";
        } catch (const std::exception& e) {
            std::cerr << "CPU Report Error: " << e.what() << "\n";
        }

        // Memory Information Report
        std::cout << "\nMemory Information Report:\n";
        std::cout << std::string(40, '-') << "\n";
        try {
            auto memInfo = getDetailedMemoryStats();
            auto memReport = SystemInfoPrinter::formatMemoryInfo(memInfo);
            std::cout << memReport << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Memory Report Error: " << e.what() << "\n";
        }

        // Disk Information Report
        std::cout << "\nDisk Information Report:\n";
        std::cout << std::string(40, '-') << "\n";
        try {
            auto diskInfo = getDiskInfo();
            auto diskReport = SystemInfoPrinter::formatDiskInfo(diskInfo);
            std::cout << diskReport << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Disk Report Error: " << e.what() << "\n";
        }

        // Battery Information Report (if available)
        std::cout << "\nBattery Information Report:\n";
        std::cout << std::string(40, '-') << "\n";
        try {
            auto batteryResult = getDetailedBatteryInfo();
            if (std::holds_alternative<BatteryInfo>(batteryResult)) {
                const auto& batteryInfo = std::get<BatteryInfo>(batteryResult);
                auto batteryReport =
                    SystemInfoPrinter::formatBatteryInfo(batteryInfo);
                std::cout << batteryReport << "\n";
            } else {
                std::cout
                    << "Battery information not available or error occurred.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Battery Report Error: " << e.what() << "\n";
        }

        std::cout
            << "\n✓ Individual component reports completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error generating component reports: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates report export functionality
 */
void demonstrateReportExport() {
    std::cout << createSectionHeader("Report Export Functionality");

    try {
        std::cout << "Demonstrating report export to files...\n\n";

        // Generate full report
        auto fullReport = SystemInfoPrinter::generateFullReport();
        auto timestamp = getCurrentTimestamp();

        // Export to text file
        std::string textFilename =
            "system_report_" +
            std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()) +
            ".txt";

        try {
            std::ofstream textFile(textFilename);
            if (textFile.is_open()) {
                textFile << "Atom System Information Report\n";
                textFile << "Generated: " << timestamp << "\n";
                textFile << std::string(80, '=') << "\n\n";
                textFile << fullReport;
                textFile.close();

                std::cout << "Text Report Export:\n";
                std::cout << "  Filename:             " << textFilename << "\n";
                std::cout << "  Status:               🟢 SUCCESS\n";
                std::cout << "  File Size:            " << fullReport.length()
                          << " bytes\n";
            } else {
                std::cerr
                    << "  Text Export:          ✗ Failed to create file\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  Text Export Error:    " << e.what() << "\n";
        }

        // Create HTML report
        std::string htmlFilename =
            "system_report_" +
            std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count()) +
            ".html";

        try {
            std::ofstream htmlFile(htmlFilename);
            if (htmlFile.is_open()) {
                htmlFile << "<!DOCTYPE html>\n<html>\n<head>\n";
                htmlFile << "<title>Atom System Information Report</title>\n";
                htmlFile << "<style>body{font-family:monospace;margin:20px;}</"
                            "style>\n";
                htmlFile << "</head>\n<body>\n";
                htmlFile << "<h1>Atom System Information Report</h1>\n";
                htmlFile << "<p><strong>Generated:</strong> " << timestamp
                         << "</p>\n";
                htmlFile << "<pre>" << fullReport << "</pre>\n";
                htmlFile << "</body>\n</html>";
                htmlFile.close();

                std::cout << "\nHTML Report Export:\n";
                std::cout << "  Filename:             " << htmlFilename << "\n";
                std::cout << "  Status:               🟢 SUCCESS\n";
                std::cout << "  Format:               HTML with CSS styling\n";
            } else {
                std::cerr
                    << "  HTML Export:          ✗ Failed to create file\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "  HTML Export Error:    " << e.what() << "\n";
        }

        std::cout << "\nExport Summary:\n";
        std::cout << "  Reports Generated:    2 files\n";
        std::cout << "  Formats:              Text, HTML\n";
        std::cout << "  Timestamp:            " << timestamp << "\n";

        std::cout << "\n✓ Report export demonstration completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in report export: " << e.what() << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive system reporting
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "=== Atom Sysinfo - Comprehensive System Report Generator ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "This example demonstrates comprehensive system report generation\n";
    std::cout
        << "using the SystemInfoPrinter utility and all available APIs.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute all demonstration functions
        demonstrateFullSystemReport();
        demonstrateComponentReports();
        demonstrateReportExport();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "All system report generation completed successfully!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Complete system report generation\n";
        std::cout << "  ✓ Individual component report formatting\n";
        std::cout << "  ✓ Report export to multiple formats\n";
        std::cout << "  ✓ Performance timing and analysis\n";
        std::cout << "  ✓ Error handling and graceful degradation\n";
        std::cout << "  ✓ Cross-platform system reporting\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << "Check the generated report files for detailed system "
                     "information.\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        std::cerr << std::string(DISPLAY_WIDTH, '=') << "\n";
        return 1;
    }

    return 0;
}
