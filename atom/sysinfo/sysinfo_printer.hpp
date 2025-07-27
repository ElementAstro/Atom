/**
 * @file sysinfo_printer.hpp
 * @brief Compatibility header for the original sysinfo_printer API
 *
 * This file provides backward compatibility with the original sysinfo_printer
 * interface while internally using the new modular implementation. All original
 * functions and classes are preserved to ensure existing code continues to work.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_SYSINFO_PRINTER_HPP
#define ATOM_SYSINFO_SYSINFO_PRINTER_HPP

// Include all system info headers for compatibility
#include "battery.hpp"
#include "bios.hpp"
#include "cpu.hpp"
#include "disk.hpp"
#include "locale.hpp"
#include "memory.hpp"
#include "os.hpp"
#include "wm.hpp"
#include "wifi.hpp"

#include <string>
#include <vector>
#include <format>

namespace atom::system {

/**
 * @class SystemInfoPrinter
 * @brief Legacy compatibility class for system information printing
 *
 * This class maintains the exact same interface as the original SystemInfoPrinter
 * while internally delegating to the new modular implementation. This ensures
 * complete backward compatibility for existing code.
 *
 * @note This is a compatibility wrapper. For new code, consider using the
 *       enhanced API in sysinfo_printer/printer.hpp directly.
 */
class SystemInfoPrinter {
public:
    // ========== Original Static Methods (Preserved for Compatibility) ==========

    /**
     * @brief Format battery information as a string
     * @param info The battery information to format
     * @return A formatted string containing battery details
     */
    static auto formatBatteryInfo(const BatteryInfo& info) -> std::string;

    /**
     * @brief Format BIOS information as a string
     * @param info The BIOS information to format
     * @return A formatted string containing BIOS details
     */
    static auto formatBiosInfo(const BiosInfoData& info) -> std::string;

    /**
     * @brief Format CPU information as a string
     * @param info The CPU information to format
     * @return A formatted string containing CPU details
     */
    static auto formatCpuInfo(const CpuInfo& info) -> std::string;

    /**
     * @brief Format disk information as a string
     * @param info Vector of disk information objects to format
     * @return A formatted string containing disk details for all drives
     */
    static auto formatDiskInfo(const std::vector<DiskInfo>& info) -> std::string;

    /**
     * @brief Format GPU information as a string
     * @return A formatted string containing GPU details
     */
    static auto formatGpuInfo() -> std::string;

    /**
     * @brief Format locale information as a string
     * @param info The locale information to format
     * @return A formatted string containing locale settings
     */
    static auto formatLocaleInfo(const LocaleInfo& info) -> std::string;

    /**
     * @brief Format memory information as a string
     * @param info The memory information to format
     * @return A formatted string containing memory details
     */
    static auto formatMemoryInfo(const MemoryInfo& info) -> std::string;

    /**
     * @brief Format operating system information as a string
     * @param info The OS information to format
     * @return A formatted string containing OS details
     */
    static auto formatOsInfo(const OperatingSystemInfo& info) -> std::string;

    /**
     * @brief Format comprehensive system information as a string
     * @param info The system information structure to format
     * @return A formatted string containing system details
     */
    static auto formatSystemInfo(const SystemInfo& info) -> std::string;

    /**
     * @brief Generate a comprehensive report of all system components
     * @return A string containing the full system report
     */
    static auto generateFullReport() -> std::string;

    /**
     * @brief Generate a simplified overview of key system information
     * @return A string containing the simplified system report
     */
    static auto generateSimpleReport() -> std::string;

    /**
     * @brief Generate a report focused on system performance metrics
     * @return A string containing the performance-focused report
     */
    static auto generatePerformanceReport() -> std::string;

    /**
     * @brief Generate a report focused on system security features
     * @return A string containing the security-focused report
     */
    static auto generateSecurityReport() -> std::string;

    /**
     * @brief Export system information to HTML format
     * @param filename The path where the HTML file will be saved
     * @return true if export was successful, false otherwise
     */
    static bool exportToHTML(const std::string& filename);

    /**
     * @brief Export system information to JSON format
     * @param filename The path where the JSON file will be saved
     * @return true if export was successful, false otherwise
     */
    static bool exportToJSON(const std::string& filename);

    /**
     * @brief Export system information to Markdown format
     * @param filename The path where the Markdown file will be saved
     * @return true if export was successful, false otherwise
     */
    static bool exportToMarkdown(const std::string& filename);

    // ========== Original Helper Functions (Preserved for Compatibility) ==========

    /**
     * @brief Create a formatted table row
     * @param label The label for the row
     * @param value The value for the row
     * @param width The total width of the row
     * @return Formatted table row string
     */
    static auto createTableRow(const std::string& label, const std::string& value, int width = 80) -> std::string;

    /**
     * @brief Create a formatted table header
     * @param title The title of the table
     * @param width The total width of the table
     * @return Formatted table header string
     */
    static auto createTableHeader(const std::string& title, int width = 80) -> std::string;

    /**
     * @brief Create a formatted table footer
     * @param width The total width of the table
     * @return Formatted table footer string
     */
    static auto createTableFooter(int width = 80) -> std::string;

    /**
     * @brief Format bytes into human-readable format
     * @param bytes The number of bytes
     * @return Human-readable byte format (e.g., "1.5 GB")
     */
    static auto formatBytes(uint64_t bytes) -> std::string;

    /**
     * @brief Format percentage value
     * @param percentage The percentage value (0-100)
     * @return Formatted percentage string
     */
    static auto formatPercentage(double percentage) -> std::string;

    /**
     * @brief Format temperature value
     * @param celsius Temperature in Celsius
     * @return Formatted temperature string
     */
    static auto formatTemperature(double celsius) -> std::string;

    /**
     * @brief Format frequency value
     * @param hertz Frequency in Hz
     * @return Formatted frequency string
     */
    static auto formatFrequency(double hertz) -> std::string;
};

} // namespace atom::system

// ========== Global Compatibility Functions ==========

/**
 * @brief Global function for backward compatibility
 * @deprecated Use atom::system::SystemInfoPrinter::generateFullReport() instead
 */
[[deprecated("Use atom::system::SystemInfoPrinter::generateFullReport() instead")]]
inline auto generateSystemReport() -> std::string {
    return atom::system::SystemInfoPrinter::generateFullReport();
}

/**
 * @brief Global function for backward compatibility
 * @deprecated Use atom::system::SystemInfoPrinter::exportToHTML() instead
 */
[[deprecated("Use atom::system::SystemInfoPrinter::exportToHTML() instead")]]
inline bool exportSystemReportToHTML(const std::string& filename) {
    return atom::system::SystemInfoPrinter::exportToHTML(filename);
}

#endif // ATOM_SYSINFO_SYSINFO_PRINTER_HPP
