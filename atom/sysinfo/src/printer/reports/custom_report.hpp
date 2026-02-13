/**
 * @file custom_report.hpp
 * @brief Custom system information report generator
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_REPORTS_CUSTOM_REPORT_HPP
#define ATOM_SYSINFO_PRINTER_REPORTS_CUSTOM_REPORT_HPP

#include "base_report.hpp"

namespace atom::system {

/**
 * @class CustomReport
 * @brief Generator for custom user-defined system information reports
 */
class CustomReport : public BaseReport {
public:
    CustomReport();
    explicit CustomReport(const ReportOptions& options);

    auto generate() -> std::string override;

protected:
    [[nodiscard]] auto getDefaultSections() const -> std::vector<ReportSection> override;

private:
    void initializeCustomReport();
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_REPORTS_CUSTOM_REPORT_HPP
