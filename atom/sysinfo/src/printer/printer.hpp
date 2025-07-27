/**
 * @file printer.hpp
 * @brief Main system information printer class
 *
 * This file contains the SystemInfoPrinter class which provides the main
 * interface for formatting and exporting system information. It serves as
 * the central hub for all formatting, reporting, and export functionality.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_PRINTER_HPP
#define ATOM_SYSINFO_PRINTER_PRINTER_HPP

#include <string>
#include <memory>
#include <unordered_map>
#include "formatters/base_formatter.hpp"
#include "exporters/base_exporter.hpp"
#include "reports/base_report.hpp"
#include "atom/macro.hpp"

// Include all system info headers for compatibility
#include "../battery.hpp"
#include "../bios.hpp"
#include "../cpu.hpp"
#include "../disk.hpp"
#include "../locale.hpp"
#include "../memory.hpp"
#include "../os.hpp"
#include "../wm.hpp"
#include "../wifi.hpp"

namespace atom::system {

/**
 * @class SystemInfoPrinter
 * @brief Main system information printer and formatter
 *
 * This class provides a comprehensive interface for formatting system information,
 * generating reports, and exporting data to various formats. It maintains backward
 * compatibility with the original API while providing enhanced functionality.
 */
class SystemInfoPrinter {
public:
    /**
     * @brief Default constructor
     */
    SystemInfoPrinter();

    /**
     * @brief Constructor with formatter options
     * @param options Default formatter options
     */
    explicit SystemInfoPrinter(const FormatterOptions& options);

    /**
     * @brief Destructor
     */
    ~SystemInfoPrinter() = default;

    // ========== Legacy API (for backward compatibility) ==========

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

    // ========== Enhanced API ==========

    /**
     * @brief Set default formatter options
     * @param options The formatter options to use
     */
    void setFormatterOptions(const FormatterOptions& options);

    /**
     * @brief Get current formatter options
     * @return Current formatter options
     */
    [[nodiscard]] auto getFormatterOptions() const -> const FormatterOptions&;

    /**
     * @brief Set default export options
     * @param options The export options to use
     */
    void setExportOptions(const ExportOptions& options);

    /**
     * @brief Get current export options
     * @return Current export options
     */
    [[nodiscard]] auto getExportOptions() const -> const ExportOptions&;

    /**
     * @brief Generate a report of specified type
     * @param type The type of report to generate
     * @param options Optional report options
     * @return Generated report content
     */
    auto generateReport(ReportType type, const ReportOptions& options = {}) -> std::string;

    /**
     * @brief Export a report to file with specified format
     * @param content The content to export
     * @param filename The output filename
     * @param format The export format
     * @return true if export was successful, false otherwise
     */
    bool exportReport(const std::string& content, const std::string& filename, ExportFormat format);

    /**
     * @brief Create a custom formatter for a specific component
     * @tparam T The formatter type
     * @param options Optional formatter options
     * @return Unique pointer to the formatter
     */
    template<typename T>
    auto createFormatter(const FormatterOptions& options = {}) -> std::unique_ptr<T>;

    /**
     * @brief Create a custom exporter for a specific format
     * @tparam T The exporter type
     * @param options Optional exporter options
     * @return Unique pointer to the exporter
     */
    template<typename T>
    auto createExporter(const ExportOptions& options = {}) -> std::unique_ptr<T>;

    /**
     * @brief Create a custom report generator
     * @tparam T The report type
     * @param options Optional report options
     * @return Unique pointer to the report
     */
    template<typename T>
    auto createReport(const ReportOptions& options = {}) -> std::unique_ptr<T>;

private:
    FormatterOptions defaultFormatterOptions_;
    ExportOptions defaultExportOptions_;

    // Static instances for legacy API
    static std::unique_ptr<SystemInfoPrinter> legacyInstance_;
    static auto getLegacyInstance() -> SystemInfoPrinter&;

    // Helper methods for legacy API
    static auto createLegacyFormatterOptions() -> FormatterOptions;
    static auto createLegacyExportOptions() -> ExportOptions;
};

// Template implementations
template<typename T>
auto SystemInfoPrinter::createFormatter(const FormatterOptions& options) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseFormatter, T>, "T must derive from BaseFormatter");
    auto formatter = std::make_unique<T>();
    auto effectiveOptions = options.style != FormatterStyle::STANDARD ? options : defaultFormatterOptions_;
    formatter->setOptions(effectiveOptions);
    return formatter;
}

template<typename T>
auto SystemInfoPrinter::createExporter(const ExportOptions& options) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseExporter, T>, "T must derive from BaseExporter");
    auto exporter = std::make_unique<T>();
    auto effectiveOptions = !options.title.empty() ? options : defaultExportOptions_;
    exporter->setOptions(effectiveOptions);
    return exporter;
}

template<typename T>
auto SystemInfoPrinter::createReport(const ReportOptions& options) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseReport, T>, "T must derive from BaseReport");
    auto report = std::make_unique<T>();
    report->setOptions(options);
    return report;
}

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_PRINTER_HPP
