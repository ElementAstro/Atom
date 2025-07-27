/**
 * @file battery_formatter.hpp
 * @brief Battery information formatter
 *
 * This file contains the BatteryFormatter class for formatting battery information
 * into human-readable text with various styling and output format options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_BATTERY_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_BATTERY_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../battery.hpp"

namespace atom::system {

/**
 * @class BatteryFormatter
 * @brief Formatter for battery information
 *
 * This class provides specialized formatting for battery information including
 * charge level, charging status, health, temperature, and time estimates.
 */
class BatteryFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    BatteryFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit BatteryFormatter(const FormatterOptions& options);

    /**
     * @brief Format battery information as a string
     * @param info The battery information to format
     * @return A formatted string containing battery details
     */
    [[nodiscard]] auto format(const BatteryInfo& info) const -> std::string;

    /**
     * @brief Format battery information with custom title
     * @param info The battery information to format
     * @param title Custom title for the section
     * @return A formatted string containing battery details
     */
    [[nodiscard]] auto format(const BatteryInfo& info, const std::string& title) const -> std::string;

    /**
     * @brief Format only basic battery information
     * @param info The battery information to format
     * @return A formatted string with basic battery details
     */
    [[nodiscard]] auto formatBasic(const BatteryInfo& info) const -> std::string;

    /**
     * @brief Format battery health information
     * @param info The battery information to format
     * @return A formatted string with battery health details
     */
    [[nodiscard]] auto formatHealth(const BatteryInfo& info) const -> std::string;

    /**
     * @brief Format battery power information
     * @param info The battery information to format
     * @return A formatted string with power-related details
     */
    [[nodiscard]] auto formatPower(const BatteryInfo& info) const -> std::string;

private:
    /**
     * @brief Format battery level with color coding
     * @param level Battery level percentage
     * @return Formatted level string with appropriate color
     */
    [[nodiscard]] auto formatLevelWithColor(float level) const -> std::string;

    /**
     * @brief Format charging status with appropriate icon/color
     * @param isCharging Whether the battery is charging
     * @return Formatted charging status string
     */
    [[nodiscard]] auto formatChargingStatus(bool isCharging) const -> std::string;

    /**
     * @brief Format battery health with color coding
     * @param health Battery health percentage
     * @return Formatted health string with appropriate color
     */
    [[nodiscard]] auto formatHealthWithColor(float health) const -> std::string;

    /**
     * @brief Format battery temperature with color coding
     * @param temperature Temperature in Celsius
     * @return Formatted temperature string with appropriate color
     */
    [[nodiscard]] auto formatTemperatureWithColor(float temperature) const -> std::string;

    /**
     * @brief Get the appropriate color for battery level
     * @param level Battery level percentage
     * @return Color name for the battery level
     */
    [[nodiscard]] auto getBatteryLevelColor(float level) const -> std::string;

    /**
     * @brief Get the appropriate color for battery health
     * @param health Battery health percentage
     * @return Color name for the health level
     */
    [[nodiscard]] auto getBatteryHealthColor(float health) const -> std::string;

    /**
     * @brief Get the appropriate color for battery temperature
     * @param temperature Temperature in Celsius
     * @return Color name for the temperature level
     */
    [[nodiscard]] auto getBatteryTemperatureColor(float temperature) const -> std::string;

    /**
     * @brief Create a battery level visualization bar
     * @param level The battery level percentage (0-100)
     * @param width The width of the bar
     * @return ASCII battery level bar string
     */
    [[nodiscard]] auto createBatteryLevelBar(float level, int width = 20) const -> std::string;

    /**
     * @brief Format time duration in human-readable format
     * @param minutes Time in minutes
     * @return Formatted time string (e.g., "2h 30m")
     */
    [[nodiscard]] auto formatTimeDuration(float minutes) const -> std::string;

    /**
     * @brief Get battery status icon based on level and charging state
     * @param level Battery level percentage
     * @param isCharging Whether the battery is charging
     * @return Unicode battery icon
     */
    [[nodiscard]] auto getBatteryIcon(float level, bool isCharging) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_BATTERY_FORMATTER_HPP
