/**
 * @file format_utils.hpp
 * @brief Formatting utility functions
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_FORMAT_UTILS_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_FORMAT_UTILS_HPP

#include <string>
#include <cstdint>

namespace atom::system::utils {

/**
 * @brief Format bytes to human-readable string
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 GB")
 */
[[nodiscard]] auto formatBytes(uint64_t bytes) -> std::string;

/**
 * @brief Format percentage value
 * @param percentage Percentage value (0-100)
 * @param precision Number of decimal places
 * @return Formatted percentage string
 */
[[nodiscard]] auto formatPercentage(double percentage, int precision = 1) -> std::string;

/**
 * @brief Format temperature value
 * @param celsius Temperature in Celsius
 * @param precision Number of decimal places
 * @return Formatted temperature string
 */
[[nodiscard]] auto formatTemperature(double celsius, int precision = 1) -> std::string;

/**
 * @brief Format frequency value
 * @param hertz Frequency in Hz
 * @param precision Number of decimal places
 * @return Formatted frequency string
 */
[[nodiscard]] auto formatFrequency(double hertz, int precision = 2) -> std::string;

/**
 * @brief Format duration in seconds to human-readable string
 * @param seconds Duration in seconds
 * @return Formatted duration string
 */
[[nodiscard]] auto formatDuration(uint64_t seconds) -> std::string;

/**
 * @brief Format a value with units
 * @param value The numeric value
 * @param unit The unit string
 * @param precision Number of decimal places
 * @return Formatted string with value and unit
 */
[[nodiscard]] auto formatWithUnits(double value, const std::string& unit, int precision = 2) -> std::string;

/**
 * @brief Format timestamp to ISO 8601 string
 * @return Current timestamp as ISO 8601 string
 */
[[nodiscard]] auto formatTimestamp() -> std::string;

/**
 * @brief Format uptime in seconds to human-readable string
 * @param uptimeSeconds Uptime in seconds
 * @return Formatted uptime string (e.g., "2 days, 3 hours, 45 minutes")
 */
[[nodiscard]] auto formatUptime(uint64_t uptimeSeconds) -> std::string;

/**
 * @brief Format network speed
 * @param bytesPerSecond Speed in bytes per second
 * @return Formatted speed string (e.g., "100 Mbps")
 */
[[nodiscard]] auto formatNetworkSpeed(double bytesPerSecond) -> std::string;

/**
 * @brief Format memory size with appropriate units
 * @param bytes Memory size in bytes
 * @return Formatted memory string
 */
[[nodiscard]] auto formatMemorySize(uint64_t bytes) -> std::string;

/**
 * @brief Format disk space with appropriate units
 * @param bytes Disk space in bytes
 * @return Formatted disk space string
 */
[[nodiscard]] auto formatDiskSpace(uint64_t bytes) -> std::string;

/**
 * @brief Format boolean value to Yes/No string
 * @param value Boolean value
 * @return "Yes" or "No"
 */
[[nodiscard]] auto formatBoolean(bool value) -> std::string;

/**
 * @brief Format a number with thousands separators
 * @param number The number to format
 * @return Formatted number string with separators
 */
[[nodiscard]] auto formatNumber(uint64_t number) -> std::string;

/**
 * @brief Format a floating-point number with thousands separators
 * @param number The number to format
 * @param precision Number of decimal places
 * @return Formatted number string with separators
 */
[[nodiscard]] auto formatNumber(double number, int precision = 2) -> std::string;

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_FORMAT_UTILS_HPP
