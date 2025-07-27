/**
 * @file system_formatter.hpp
 * @brief System/WM information formatter
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_SYSTEM_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_SYSTEM_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../../wm.hpp"

namespace atom::system {

/**
 * @class SystemFormatter
 * @brief Formatter for system/window manager information
 */
class SystemFormatter : public BaseFormatter {
public:
    SystemFormatter() = default;
    explicit SystemFormatter(const FormatterOptions& options);

    [[nodiscard]] auto format(const SystemInfo& info) const -> std::string;
    [[nodiscard]] auto format(const SystemInfo& info, const std::string& title) const -> std::string;
    [[nodiscard]] auto formatBasic(const SystemInfo& info) const -> std::string;
    [[nodiscard]] auto formatDesktop(const SystemInfo& info) const -> std::string;
    [[nodiscard]] auto formatTheme(const SystemInfo& info) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_SYSTEM_FORMATTER_HPP