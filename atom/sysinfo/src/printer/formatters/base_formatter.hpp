/**
 * @file base_formatter.hpp
 * @brief Base formatter interface for system information components
 *
 * This file defines the base interface and common functionality for all
 * system information formatters. It provides a consistent API for formatting
 * different types of system data into human-readable strings.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_BASE_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_BASE_FORMATTER_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include "atom/macro.hpp"

namespace atom::system {

/**
 * @enum FormatterStyle
 * @brief Defines different formatting styles for output
 */
enum class FormatterStyle {
    MINIMAL,    ///< Minimal output with essential information only
    COMPACT,    ///< Compact format with key information
    STANDARD,   ///< Standard detailed format
    DETAILED,   ///< Detailed format with all available information
    VERBOSE     ///< Verbose format with explanations and context
};

/**
 * @enum OutputFormat
 * @brief Defines the output format type
 */
enum class OutputFormat {
    PLAIN_TEXT, ///< Plain text format
    TABLE,      ///< Formatted table
    JSON,       ///< JSON format
    XML,        ///< XML format
    MARKDOWN,   ///< Markdown format
    HTML        ///< HTML format
};

/**
 * @struct FormatterOptions
 * @brief Configuration options for formatters
 */
struct FormatterOptions {
    FormatterStyle style = FormatterStyle::STANDARD;
    OutputFormat format = OutputFormat::TABLE;
    bool colorEnabled = false;
    bool timestampEnabled = false;
    bool unitsEnabled = true;
    int tableWidth = 80;
    std::string indentation = "  ";
    std::unordered_map<std::string, std::string> customOptions;
} ATOM_ALIGNAS(64);

/**
 * @class BaseFormatter
 * @brief Abstract base class for all system information formatters
 *
 * This class provides the common interface and functionality that all
 * specific formatters must implement. It handles configuration, styling,
 * and common formatting operations.
 */
class BaseFormatter {
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~BaseFormatter() = default;

    /**
     * @brief Set the formatting style
     * @param style The formatting style to use
     */
    virtual void setStyle(FormatterStyle style);

    /**
     * @brief Set the output format
     * @param format The output format to use
     */
    virtual void setOutputFormat(OutputFormat format);

    /**
     * @brief Enable or disable color output
     * @param enabled Whether to enable color output
     */
    virtual void setColorEnabled(bool enabled);

    /**
     * @brief Enable or disable timestamp in output
     * @param enabled Whether to include timestamps
     */
    virtual void setTimestampEnabled(bool enabled);

    /**
     * @brief Set custom formatting options
     * @param options The formatting options to use
     */
    virtual void setOptions(const FormatterOptions& options);

    /**
     * @brief Get current formatting options
     * @return Current formatting options
     */
    [[nodiscard]] virtual auto getOptions() const -> const FormatterOptions&;

    /**
     * @brief Set a custom option
     * @param key The option key
     * @param value The option value
     */
    virtual void setCustomOption(const std::string& key, const std::string& value);

    /**
     * @brief Get a custom option value
     * @param key The option key
     * @param defaultValue Default value if key not found
     * @return The option value or default
     */
    [[nodiscard]] virtual auto getCustomOption(const std::string& key,
                                              const std::string& defaultValue = "") const -> std::string;

protected:
    /**
     * @brief Create a formatted table row
     * @param label The label for the row
     * @param value The value for the row
     * @param width The total width of the row
     * @return Formatted table row string
     */
    [[nodiscard]] virtual auto createTableRow(const std::string& label,
                                             const std::string& value,
                                             int width = 0) const -> std::string;

    /**
     * @brief Create a formatted table header
     * @param title The title of the table
     * @param width The total width of the table
     * @return Formatted table header string
     */
    [[nodiscard]] virtual auto createTableHeader(const std::string& title,
                                                int width = 0) const -> std::string;

    /**
     * @brief Create a formatted table footer
     * @param width The total width of the table
     * @return Formatted table footer string
     */
    [[nodiscard]] virtual auto createTableFooter(int width = 0) const -> std::string;

    /**
     * @brief Apply color formatting to text
     * @param text The text to colorize
     * @param color The color code or name
     * @return Colorized text if color is enabled, otherwise original text
     */
    [[nodiscard]] virtual auto colorize(const std::string& text,
                                       const std::string& color) const -> std::string;

    /**
     * @brief Format a value with units
     * @param value The numeric value
     * @param unit The unit string
     * @param precision Number of decimal places
     * @return Formatted value with units
     */
    [[nodiscard]] virtual auto formatWithUnits(double value,
                                              const std::string& unit,
                                              int precision = 2) const -> std::string;

    /**
     * @brief Format bytes into human-readable format
     * @param bytes The number of bytes
     * @param precision Number of decimal places
     * @return Human-readable byte format (e.g., "1.5 GB")
     */
    [[nodiscard]] virtual auto formatBytes(uint64_t bytes, int precision = 2) const -> std::string;

    /**
     * @brief Format percentage value
     * @param percentage The percentage value (0-100)
     * @param precision Number of decimal places
     * @return Formatted percentage string
     */
    [[nodiscard]] virtual auto formatPercentage(double percentage, int precision = 1) const -> std::string;

    /**
     * @brief Format temperature value
     * @param celsius Temperature in Celsius
     * @param precision Number of decimal places
     * @return Formatted temperature string
     */
    [[nodiscard]] virtual auto formatTemperature(double celsius, int precision = 1) const -> std::string;

    /**
     * @brief Format frequency value
     * @param hertz Frequency in Hz
     * @param precision Number of decimal places
     * @return Formatted frequency string
     */
    [[nodiscard]] virtual auto formatFrequency(double hertz, int precision = 2) const -> std::string;

    /**
     * @brief Add timestamp to output if enabled
     * @param content The content to add timestamp to
     * @return Content with timestamp if enabled
     */
    [[nodiscard]] virtual auto addTimestamp(const std::string& content) const -> std::string;

    /**
     * @brief Get the effective table width
     * @return The table width to use for formatting
     */
    [[nodiscard]] virtual auto getTableWidth() const -> int;

    FormatterOptions options_; ///< Current formatting options

private:
    /**
     * @brief Initialize default color codes
     */
    void initializeColors();

    std::unordered_map<std::string, std::string> colorCodes_; ///< Color code mappings
};

/**
 * @brief Factory function to create formatters
 * @tparam T The formatter type to create
 * @param options Initial formatting options
 * @return Unique pointer to the created formatter
 */
template<typename T>
[[nodiscard]] auto createFormatter(const FormatterOptions& options = {}) -> std::unique_ptr<T> {
    static_assert(std::is_base_of_v<BaseFormatter, T>, "T must derive from BaseFormatter");
    auto formatter = std::make_unique<T>();
    formatter->setOptions(options);
    return formatter;
}

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_BASE_FORMATTER_HPP
