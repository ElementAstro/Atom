/**
 * @file full_report.hpp
 * @brief Full system information report generator
 *
 * This file contains the FullReport class for generating comprehensive
 * system information reports including all available components.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_REPORTS_FULL_REPORT_HPP
#define ATOM_SYSINFO_PRINTER_REPORTS_FULL_REPORT_HPP

#include "base_report.hpp"

namespace atom::system {

/**
 * @class FullReport
 * @brief Generator for comprehensive system information reports
 *
 * This class provides functionality to generate complete system information
 * reports including all available hardware and software components.
 */
class FullReport : public BaseReport {
public:
    /**
     * @brief Default constructor
     */
    FullReport();

    /**
     * @brief Constructor with options
     * @param options Initial report options
     */
    explicit FullReport(const ReportOptions& options);

    /**
     * @brief Generate the full system report
     * @return Generated report content as string
     */
    auto generate() -> std::string override;

protected:
    /**
     * @brief Get the default sections for full report
     * @return Vector of all available sections
     */
    [[nodiscard]] auto getDefaultSections() const -> std::vector<ReportSection> override;

private:
    /**
     * @brief Initialize full report with all sections
     */
    void initializeFullReport();
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_REPORTS_FULL_REPORT_HPP
