/**
 * @file bios_formatter.hpp
 * @brief BIOS information formatter
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_BIOS_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_BIOS_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../bios.hpp"

namespace atom::system {

class BiosFormatter : public BaseFormatter {
public:
    BiosFormatter() = default;
    explicit BiosFormatter(const FormatterOptions& options);

    [[nodiscard]] auto format(const BiosInfoData& info) const -> std::string;
    [[nodiscard]] auto format(const BiosInfoData& info, const std::string& title) const -> std::string;
    [[nodiscard]] auto formatBasic(const BiosInfoData& info) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_BIOS_FORMATTER_HPP
