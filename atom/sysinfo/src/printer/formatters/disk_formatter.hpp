/**
 * @file disk_formatter.hpp
 * @brief Disk information formatter
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_DISK_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_DISK_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../disk.hpp"

namespace atom::system {

class DiskFormatter : public BaseFormatter {
public:
    DiskFormatter() = default;
    explicit DiskFormatter(const FormatterOptions& options);

    [[nodiscard]] auto format(const std::vector<DiskInfo>& disks) const -> std::string;
    [[nodiscard]] auto format(const std::vector<DiskInfo>& disks, const std::string& title) const -> std::string;
    [[nodiscard]] auto formatSingle(const DiskInfo& disk, int index = 0) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_DISK_FORMATTER_HPP
