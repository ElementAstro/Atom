/**
 * @file performance_report.hpp
 * @brief Performance-focused system information report generator
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_REPORTS_PERFORMANCE_REPORT_HPP
#define ATOM_SYSINFO_PRINTER_REPORTS_PERFORMANCE_REPORT_HPP

#include "base_report.hpp"

namespace atom::system {

/**
 * @class PerformanceReport
 * @brief Generator for performance-focused system information reports
 */
class PerformanceReport : public BaseReport {
public:
    PerformanceReport();
    explicit PerformanceReport(const ReportOptions& options);

    auto generate() -> std::string override;

protected:
    [[nodiscard]] auto getDefaultSections() const -> std::vector<ReportSection> override;

private:
    void initializePerformanceReport();
    [[nodiscard]] auto generatePerformanceMetrics() -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_REPORTS_PERFORMANCE_REPORT_HPP
