/**
 * @file table_utils.hpp
 * @brief Table formatting utilities for system information display
 *
 * This file contains utility functions for creating and formatting tables
 * used in system information display. It provides consistent table styling
 * and formatting across all components.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_TABLE_UTILS_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_TABLE_UTILS_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include "atom/macro.hpp"

namespace atom::system::utils {

/**
 * @enum TableStyle
 * @brief Defines different table styling options
 */
enum class TableStyle {
    ASCII,      ///< ASCII characters for borders
    UNICODE,    ///< Unicode box drawing characters
    MARKDOWN,   ///< Markdown table format
    SIMPLE,     ///< Simple format without borders
    COMPACT     ///< Compact format with minimal spacing
};

/**
 * @enum Alignment
 * @brief Text alignment options for table cells
 */
enum class Alignment {
    LEFT,       ///< Left-aligned text
    CENTER,     ///< Center-aligned text
    RIGHT       ///< Right-aligned text
};

/**
 * @struct TableColumn
 * @brief Configuration for a table column
 */
struct TableColumn {
    std::string header;
    int width = 0;
    Alignment alignment = Alignment::LEFT;
    bool autoSize = true;
} ATOM_ALIGNAS(32);

/**
 * @struct TableOptions
 * @brief Configuration options for table formatting
 */
struct TableOptions {
    TableStyle style = TableStyle::ASCII;
    int totalWidth = 80;
    bool showHeaders = true;
    bool showBorders = true;
    bool alternateRowColors = false;
    std::string padding = " ";
    std::unordered_map<std::string, std::string> customOptions;
} ATOM_ALIGNAS(64);

/**
 * @class TableBuilder
 * @brief Builder class for creating formatted tables
 *
 * This class provides a fluent interface for building tables with
 * customizable styling, alignment, and formatting options.
 */
class TableBuilder {
public:
    /**
     * @brief Default constructor
     */
    TableBuilder() = default;

    /**
     * @brief Constructor with options
     * @param options Table formatting options
     */
    explicit TableBuilder(const TableOptions& options);

    /**
     * @brief Set table options
     * @param options The table options to use
     */
    auto setOptions(const TableOptions& options) -> TableBuilder&;

    /**
     * @brief Add a column to the table
     * @param header Column header text
     * @param width Column width (0 for auto-size)
     * @param alignment Text alignment
     * @return Reference to this builder for chaining
     */
    auto addColumn(const std::string& header, int width = 0, Alignment alignment = Alignment::LEFT) -> TableBuilder&;

    /**
     * @brief Add a row to the table
     * @param cells Vector of cell values
     * @return Reference to this builder for chaining
     */
    auto addRow(const std::vector<std::string>& cells) -> TableBuilder&;

    /**
     * @brief Add a separator row
     * @return Reference to this builder for chaining
     */
    auto addSeparator() -> TableBuilder&;

    /**
     * @brief Build the formatted table
     * @return Formatted table as string
     */
    [[nodiscard]] auto build() const -> std::string;

    /**
     * @brief Clear all data and reset the builder
     * @return Reference to this builder for chaining
     */
    auto clear() -> TableBuilder&;

private:
    TableOptions options_;
    std::vector<TableColumn> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<bool> separatorRows_;

    /**
     * @brief Calculate optimal column widths
     */
    void calculateColumnWidths();

    /**
     * @brief Format a single row
     * @param cells The cell values
     * @param isSeparator Whether this is a separator row
     * @return Formatted row string
     */
    [[nodiscard]] auto formatRow(const std::vector<std::string>& cells, bool isSeparator = false) const -> std::string;

    /**
     * @brief Format table header
     * @return Formatted header string
     */
    [[nodiscard]] auto formatHeader() const -> std::string;

    /**
     * @brief Format table border
     * @param isTop Whether this is the top border
     * @return Formatted border string
     */
    [[nodiscard]] auto formatBorder(bool isTop = true) const -> std::string;

    /**
     * @brief Align text within a cell
     * @param text The text to align
     * @param width The cell width
     * @param alignment The alignment type
     * @return Aligned text
     */
    [[nodiscard]] auto alignText(const std::string& text, int width, Alignment alignment) const -> std::string;

    /**
     * @brief Get border characters for the current style
     * @return Map of border character names to characters
     */
    [[nodiscard]] auto getBorderChars() const -> std::unordered_map<std::string, std::string>;
};

/**
 * @brief Create a simple two-column table
 * @param title Table title
 * @param data Map of label -> value pairs
 * @param options Table formatting options
 * @return Formatted table string
 */
[[nodiscard]] auto createSimpleTable(const std::string& title,
                                    const std::unordered_map<std::string, std::string>& data,
                                    const TableOptions& options = {}) -> std::string;

/**
 * @brief Create a table from structured data
 * @param headers Column headers
 * @param rows Row data
 * @param options Table formatting options
 * @return Formatted table string
 */
[[nodiscard]] auto createTable(const std::vector<std::string>& headers,
                              const std::vector<std::vector<std::string>>& rows,
                              const TableOptions& options = {}) -> std::string;

/**
 * @brief Format text with specified alignment and width
 * @param text The text to format
 * @param width The target width
 * @param alignment The alignment type
 * @param padding The padding character
 * @return Formatted text
 */
[[nodiscard]] auto formatText(const std::string& text, int width, 
                             Alignment alignment = Alignment::LEFT,
                             char padding = ' ') -> std::string;

/**
 * @brief Truncate text to fit within specified width
 * @param text The text to truncate
 * @param width The maximum width
 * @param ellipsis The ellipsis string to append
 * @return Truncated text
 */
[[nodiscard]] auto truncateText(const std::string& text, int width, 
                               const std::string& ellipsis = "...") -> std::string;

/**
 * @brief Wrap text to fit within specified width
 * @param text The text to wrap
 * @param width The maximum width per line
 * @return Vector of wrapped lines
 */
[[nodiscard]] auto wrapText(const std::string& text, int width) -> std::vector<std::string>;

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_TABLE_UTILS_HPP
