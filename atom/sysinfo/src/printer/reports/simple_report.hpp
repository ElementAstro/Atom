/**
 * @file simple_report.hpp
 * @brief Simple system information report generator
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_REPORTS_SIMPLE_REPORT_HPP
#define ATOM_SYSINFO_PRINTER_REPORTS_SIMPLE_REPORT_HPP

#include "base_report.hpp"

namespace atom::system {

/**
 * @class SimpleReport
 * @brief Generator for simplified system information reports
 */
class SimpleReport : public BaseReport {
public:
    SimpleReport();
    explicit SimpleReport(const ReportOptions& options);

    auto generate() -> std::string override;

protected:
    [[nodiscard]] auto getDefaultSections() const -> std::vector<ReportSection> override;

private:
    void initializeSimpleReport();
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_REPORTS_SIMPLE_REPORT_HPP
