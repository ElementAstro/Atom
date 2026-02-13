/**
 * @file cpu_formatter.hpp
 * @brief CPU information formatter
 *
 * This file contains the CpuFormatter class for formatting CPU information
 * into human-readable text with various styling and output format options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_CPU_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_CPU_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../cpu.hpp"

namespace atom::system {

/**
 * @class CpuFormatter
 * @brief Formatter for CPU information
 *
 * This class provides specialized formatting for CPU information including
 * model, vendor, architecture, cores, frequency, temperature, and usage.
 */
class CpuFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    CpuFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit CpuFormatter(const FormatterOptions& options);

    /**
     * @brief Format CPU information as a string
     * @param info The CPU information to format
     * @return A formatted string containing CPU details
     */
    [[nodiscard]] auto format(const CpuInfo& info) const -> std::string;

    /**
     * @brief Format CPU information with custom title
     * @param info The CPU information to format
     * @param title Custom title for the section
     * @return A formatted string containing CPU details
     */
    [[nodiscard]] auto format(const CpuInfo& info, const std::string& title) const -> std::string;

    /**
     * @brief Format only basic CPU information
     * @param info The CPU information to format
     * @return A formatted string with basic CPU details
     */
    [[nodiscard]] auto formatBasic(const CpuInfo& info) const -> std::string;

    /**
     * @brief Format CPU performance metrics
     * @param info The CPU information to format
     * @return A formatted string with performance details
     */
    [[nodiscard]] auto formatPerformance(const CpuInfo& info) const -> std::string;

    /**
     * @brief Format CPU thermal information
     * @param info The CPU information to format
     * @return A formatted string with thermal details
     */
    [[nodiscard]] auto formatThermal(const CpuInfo& info) const -> std::string;

    /**
     * @brief Format CPU cache information
     * @param info The CPU information to format
     * @return A formatted string with cache details
     */
    [[nodiscard]] auto formatCache(const CpuInfo& info) const -> std::string;

private:
    /**
     * @brief Format CPU vendor information
     * @param vendor The CPU vendor enum
     * @return Human-readable vendor string
     */
    [[nodiscard]] auto formatVendor(CpuVendor vendor) const -> std::string;

    /**
     * @brief Format CPU architecture information
     * @param arch The CPU architecture enum
     * @return Human-readable architecture string
     */
    [[nodiscard]] auto formatArchitecture(CpuArchitecture arch) const -> std::string;

    /**
     * @brief Format CPU usage with color coding
     * @param usage CPU usage percentage
     * @return Formatted usage string with appropriate color
     */
    [[nodiscard]] auto formatUsageWithColor(float usage) const -> std::string;

    /**
     * @brief Format CPU temperature with color coding
     * @param temperature Temperature in Celsius
     * @return Formatted temperature string with appropriate color
     */
    [[nodiscard]] auto formatTemperatureWithColor(float temperature) const -> std::string;

    /**
     * @brief Get the appropriate color for CPU usage
     * @param usage CPU usage percentage
     * @return Color name for the usage level
     */
    [[nodiscard]] auto getUsageColor(float usage) const -> std::string;

    /**
     * @brief Get the appropriate color for CPU temperature
     * @param temperature Temperature in Celsius
     * @return Color name for the temperature level
     */
    [[nodiscard]] auto getTemperatureColor(float temperature) const -> std::string;

    /**
     * @brief Format cache size information
     * @param caches The cache size information
     * @return Formatted cache information string
     */
    [[nodiscard]] auto formatCacheSizes(const CacheSizes& caches) const -> std::string;

    /**
     * @brief Create a progress bar for usage visualization
     * @param percentage The percentage value (0-100)
     * @param width The width of the progress bar
     * @return ASCII progress bar string
     */
    [[nodiscard]] auto createProgressBar(float percentage, int width = 20) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_CPU_FORMATTER_HPP
