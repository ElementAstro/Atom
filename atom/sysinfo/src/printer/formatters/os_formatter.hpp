/**
 * @file os_formatter.hpp
 * @brief Operating System information formatter
 *
 * This file contains the OsFormatter class for formatting operating system
 * information into human-readable text with various styling options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_OS_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_OS_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../os.hpp"

namespace atom::system {

/**
 * @class OsFormatter
 * @brief Formatter for operating system information
 *
 * This class provides specialized formatting for OS information including
 * name, version, kernel, architecture, boot time, and system configuration.
 */
class OsFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    OsFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit OsFormatter(const FormatterOptions& options);

    /**
     * @brief Format OS information as a string
     * @param info The OS information to format
     * @return A formatted string containing OS details
     */
    [[nodiscard]] auto format(const OperatingSystemInfo& info) const -> std::string;

    /**
     * @brief Format OS information with custom title
     * @param info The OS information to format
     * @param title Custom title for the section
     * @return A formatted string containing OS details
     */
    [[nodiscard]] auto format(const OperatingSystemInfo& info, const std::string& title) const -> std::string;

    /**
     * @brief Format only basic OS information
     * @param info The OS information to format
     * @return A formatted string with basic OS details
     */
    [[nodiscard]] auto formatBasic(const OperatingSystemInfo& info) const -> std::string;

    /**
     * @brief Format OS version information
     * @param info The OS information to format
     * @return A formatted string with version details
     */
    [[nodiscard]] auto formatVersion(const OperatingSystemInfo& info) const -> std::string;

    /**
     * @brief Format OS system information
     * @param info The OS information to format
     * @return A formatted string with system details
     */
    [[nodiscard]] auto formatSystem(const OperatingSystemInfo& info) const -> std::string;

private:
    /**
     * @brief Format server edition status with appropriate styling
     * @param isServer Whether the OS is server edition
     * @return Formatted server status string
     */
    [[nodiscard]] auto formatServerStatus(bool isServer) const -> std::string;

    /**
     * @brief Get OS icon based on OS name
     * @param osName The operating system name
     * @return Unicode icon for the OS
     */
    [[nodiscard]] auto getOsIcon(const std::string& osName) const -> std::string;

    /**
     * @brief Format uptime from boot time
     * @param bootTime Boot time string
     * @return Formatted uptime string
     */
    [[nodiscard]] auto formatUptime(const std::string& bootTime) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_OS_FORMATTER_HPP
