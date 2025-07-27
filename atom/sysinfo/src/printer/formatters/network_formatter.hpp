/**
 * @file network_formatter.hpp
 * @brief Network information formatter
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_NETWORK_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_NETWORK_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../../wifi.hpp"

namespace atom::system {

/**
 * @class NetworkFormatter
 * @brief Formatter for network information
 */
class NetworkFormatter : public BaseFormatter {
public:
    NetworkFormatter() = default;
    explicit NetworkFormatter(const FormatterOptions& options);

    [[nodiscard]] auto format(const NetworkStats& stats) const -> std::string;
    [[nodiscard]] auto format(const NetworkStats& stats, const std::string& title) const -> std::string;
    [[nodiscard]] auto formatBasic(const NetworkStats& stats) const -> std::string;
    [[nodiscard]] auto formatInterfaces() const -> std::string;
    [[nodiscard]] auto formatConnections() const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_NETWORK_FORMATTER_HPP