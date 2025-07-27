/**
 * @file base_report.hpp
 * @brief Base report interface for system information reports
 *
 * This file defines the base interface and common functionality for all
 * system information report generators. It provides a consistent API for
 * generating different types of system reports.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_REPORTS_BASE_REPORT_HPP
#define ATOM_SYSINFO_PRINTER_REPORTS_BASE_REPORT_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../formatters/base_formatter.hpp"
#include "../exporters/base_exporter.hpp"
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @enum ReportType
 * @brief Defines different types of system reports
 */
enum class ReportType {
    FULL,        ///< Complete system information report
    SIMPLE,      ///< Simplified overview report
    PERFORMANCE, ///< Performance-focused report
    SECURITY,    ///< Security-focused report
    HARDWARE,    ///< Hardware-focused report
    SOFTWARE,    ///< Software-focused report
    CUSTOM       ///< Custom user-defined report
};

/**
 * @enum ReportSection
 * @brief Defines different sections that can be included in reports
 */
enum class ReportSection {
    OS,          ///< Operating system information
    CPU,         ///< CPU information
    MEMORY,      ///< Memory information
    DISK,        ///< Disk information
    BATTERY,     ///< Battery information
    NETWORK,     ///< Network information
    BIOS,        ///< BIOS information
    GPU,         ///< GPU information
    LOCALE,      ///< Locale information
    SYSTEM       ///< System/WM information
};

/**
 * @struct ReportOptions
 * @brief Configuration options for report generation
 */
struct ReportOptions {
    ReportType type = ReportType::FULL;
    FormatterStyle style = FormatterStyle::STANDARD;
    OutputFormat format = OutputFormat::TABLE;
    bool includeTimestamp = true;
    bool includeMetadata = true;
    bool colorEnabled = false;
    std::vector<ReportSection> sections;
    std::string title;
    std::string templatePath;
    std::unordered_map<std::string, std::string> customOptions;
} ATOM_ALIGNAS(64);

/**
 * @class BaseReport
 * @brief Abstract base class for all system information report generators
 *
 * This class provides the common interface and functionality that all
 * specific report generators must implement. It handles configuration,
 * section management, and common report operations.
 */
class BaseReport {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~BaseReport() = default;

    /**
     * @brief Generate the report
     * @return Generated report content as string
     */
    virtual auto generate() -> std::string = 0;

    /**
     * @brief Set report options
     * @param options The report options to use
     */
    virtual void setOptions(const ReportOptions& options);

    /**
     * @brief Get current report options
     * @return Current report options
     */
    [[nodiscard]] virtual auto getOptions() const -> const ReportOptions&;

    /**
     * @brief Add a section to the report
     * @param section The section to add
     */
    virtual void addSection(ReportSection section);

    /**
     * @brief Remove a section from the report
     * @param section The section to remove
     */
    virtual void removeSection(ReportSection section);

    /**
     * @brief Check if a section is included in the report
     * @param section The section to check
     * @return true if section is included, false otherwise
     */
    [[nodiscard]] virtual bool hasSection(ReportSection section) const;

    /**
     * @brief Clear all sections
     */
    virtual void clearSections();

    /**
     * @brief Set the report title
     * @param title The report title
     */
    virtual void setTitle(const std::string& title);

    /**
     * @brief Get the report title
     * @return The report title
     */
    [[nodiscard]] virtual auto getTitle() const -> std::string;

    /**
     * @brief Set a custom option
     * @param key The option key
     * @param value The option value
     */
    virtual void setCustomOption(const std::string& key, const std::string& value);

    /**
     * @brief Get a custom option value
     * @param key The option key
     * @param defaultValue Default value if key not found
     * @return The option value or default
     */
    [[nodiscard]] virtual auto getCustomOption(const std::string& key, 
                                              const std::string& defaultValue = "") const -> std::string;

    /**
     * @brief Export the report to a file
     * @param filename The output filename
     * @param format The export format
     * @return true if export was successful, false otherwise
     */
    virtual bool exportToFile(const std::string& filename, ExportFormat format = ExportFormat::HTML);

protected:
    /**
     * @brief Generate content for a specific section
     * @param section The section to generate
     * @return Generated section content
     */
    [[nodiscard]] virtual auto generateSection(ReportSection section) -> std::string;

    /**
     * @brief Get the formatter for a specific section
     * @param section The section
     * @return Shared pointer to the formatter
     */
    [[nodiscard]] virtual auto getFormatter(ReportSection section) -> std::shared_ptr<BaseFormatter>;

    /**
     * @brief Get the exporter for a specific format
     * @param format The export format
     * @return Shared pointer to the exporter
     */
    [[nodiscard]] virtual auto getExporter(ExportFormat format) -> std::shared_ptr<BaseExporter>;

    /**
     * @brief Create the report header
     * @return Report header content
     */
    [[nodiscard]] virtual auto createHeader() -> std::string;

    /**
     * @brief Create the report footer
     * @return Report footer content
     */
    [[nodiscard]] virtual auto createFooter() -> std::string;

    /**
     * @brief Combine all sections into a complete report
     * @param sections Map of section content
     * @return Complete report content
     */
    [[nodiscard]] virtual auto combineSections(const std::unordered_map<ReportSection, std::string>& sections) -> std::string;

    /**
     * @brief Get the default sections for this report type
     * @return Vector of default sections
     */
    [[nodiscard]] virtual auto getDefaultSections() const -> std::vector<ReportSection>;

    /**
     * @brief Convert section enum to string
     * @param section The section enum
     * @return String representation of the section
     */
    [[nodiscard]] virtual auto sectionToString(ReportSection section) const -> std::string;

    /**
     * @brief Apply template to the report content
     * @param content The report content
     * @return Content with template applied
     */
    [[nodiscard]] virtual auto applyTemplate(const std::string& content) -> std::string;

    ReportOptions options_; ///< Current report options

private:
    /**
     * @brief Initialize default sections based on report type
     */
    void initializeDefaultSections();

    /**
     * @brief Create formatter options from report options
     * @return Formatter options
     */
    [[nodiscard]] auto createFormatterOptions() const -> FormatterOptions;

    /**
     * @brief Create exporter options from report options
     * @return Exporter options
     */
    [[nodiscard]] auto createExporterOptions() const -> ExportOptions;
};

/**
 * @brief Factory function to create reports
 * @tparam T The report type to create
 * @param options Initial report options
 * @return Unique pointer to the created report
 */
template<typename T>
[[nodiscard]] auto createReport(const ReportOptions& options = {}) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseReport, T>, "T must derive from BaseReport");
    auto report = std::make_unique<T>();
    report->setOptions(options);
    return report;
}

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_REPORTS_BASE_REPORT_HPP
