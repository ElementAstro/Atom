#include "table_utils.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace atom::system::utils {

TableBuilder::TableBuilder(const TableOptions& options) : options_(options) {}

auto TableBuilder::setOptions(const TableOptions& options) -> TableBuilder& {
    options_ = options;
    return *this;
}

auto TableBuilder::addColumn(const std::string& header, int width, Alignment alignment) -> TableBuilder& {
    TableColumn column;
    column.header = header;
    column.width = width;
    column.alignment = alignment;
    column.autoSize = (width == 0);
    columns_.push_back(column);
    return *this;
}

auto TableBuilder::addRow(const std::vector<std::string>& cells) -> TableBuilder& {
    rows_.push_back(cells);
    separatorRows_.push_back(false);
    return *this;
}

auto TableBuilder::addSeparator() -> TableBuilder& {
    rows_.emplace_back();
    separatorRows_.push_back(true);
    return *this;
}

auto TableBuilder::build() const -> std::string {
    if (columns_.empty()) {
        return "";
    }
    
    // Make a copy to calculate widths
    auto builder = *this;
    builder.calculateColumnWidths();
    
    std::stringstream ss;
    
    // Top border
    if (options_.showBorders) {
        ss << builder.formatBorder(true) << "\n";
    }
    
    // Header
    if (options_.showHeaders) {
        ss << builder.formatHeader() << "\n";
        if (options_.showBorders) {
            ss << builder.formatBorder(false) << "\n";
        }
    }
    
    // Rows
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (separatorRows_[i]) {
            if (options_.showBorders) {
                ss << builder.formatBorder(false) << "\n";
            }
        } else {
            ss << builder.formatRow(rows_[i]) << "\n";
        }
    }
    
    // Bottom border
    if (options_.showBorders) {
        ss << builder.formatBorder(true) << "\n";
    }
    
    return ss.str();
}

auto TableBuilder::clear() -> TableBuilder& {
    columns_.clear();
    rows_.clear();
    separatorRows_.clear();
    return *this;
}

void TableBuilder::calculateColumnWidths() {
    if (columns_.empty()) return;
    
    // Calculate content widths
    for (auto& column : columns_) {
        if (column.autoSize) {
            // Start with header width
            column.width = static_cast<int>(column.header.length());
            
            // Check all rows for this column
            size_t colIndex = &column - &columns_[0];
            for (const auto& row : rows_) {
                if (colIndex < row.size()) {
                    column.width = std::max(column.width, static_cast<int>(row[colIndex].length()));
                }
            }
            
            // Add padding
            column.width += 2;
        }
    }
    
    // Adjust for total width if needed
    int totalUsed = 0;
    int autoColumns = 0;
    
    for (const auto& column : columns_) {
        totalUsed += column.width;
        if (column.autoSize) {
            autoColumns++;
        }
    }
    
    // Add border characters
    if (options_.showBorders) {
        totalUsed += static_cast<int>(columns_.size()) + 1;
    }
    
    // Adjust if exceeding total width
    if (totalUsed > options_.totalWidth && autoColumns > 0) {
        int excess = totalUsed - options_.totalWidth;
        int reduction = excess / autoColumns;
        
        for (auto& column : columns_) {
            if (column.autoSize && column.width > reduction + 5) {
                column.width -= reduction;
            }
        }
    }
}

auto TableBuilder::formatRow(const std::vector<std::string>& cells, bool isSeparator) const -> std::string {
    if (isSeparator) {
        return formatBorder(false);
    }
    
    std::stringstream ss;
    auto borderChars = getBorderChars();
    
    if (options_.showBorders) {
        ss << borderChars["vertical"];
    }
    
    for (size_t i = 0; i < columns_.size(); ++i) {
        std::string cellContent = (i < cells.size()) ? cells[i] : "";
        std::string formatted = alignText(cellContent, columns_[i].width, columns_[i].alignment);
        
        ss << options_.padding << formatted << options_.padding;
        
        if (options_.showBorders && i < columns_.size() - 1) {
            ss << borderChars["vertical"];
        }
    }
    
    if (options_.showBorders) {
        ss << borderChars["vertical"];
    }
    
    return ss.str();
}

auto TableBuilder::formatHeader() const -> std::string {
    std::vector<std::string> headers;
    for (const auto& column : columns_) {
        headers.push_back(column.header);
    }
    return formatRow(headers);
}

auto TableBuilder::formatBorder(bool isTop) const -> std::string {
    auto borderChars = getBorderChars();
    std::stringstream ss;
    
    // Corner character
    ss << (isTop ? borderChars["top_left"] : borderChars["bottom_left"]);
    
    for (size_t i = 0; i < columns_.size(); ++i) {
        // Horizontal line
        int lineWidth = columns_[i].width + 2 * static_cast<int>(options_.padding.length());
        ss << std::string(lineWidth, borderChars["horizontal"][0]);
        
        // Junction or corner
        if (i < columns_.size() - 1) {
            ss << (isTop ? borderChars["top_junction"] : borderChars["bottom_junction"]);
        } else {
            ss << (isTop ? borderChars["top_right"] : borderChars["bottom_right"]);
        }
    }
    
    return ss.str();
}

auto TableBuilder::alignText(const std::string& text, int width, Alignment alignment) const -> std::string {
    if (static_cast<int>(text.length()) >= width) {
        return text.substr(0, width);
    }
    
    int padding = width - static_cast<int>(text.length());
    
    switch (alignment) {
        case Alignment::LEFT:
            return text + std::string(padding, ' ');
        case Alignment::RIGHT:
            return std::string(padding, ' ') + text;
        case Alignment::CENTER: {
            int leftPad = padding / 2;
            int rightPad = padding - leftPad;
            return std::string(leftPad, ' ') + text + std::string(rightPad, ' ');
        }
        default:
            return text + std::string(padding, ' ');
    }
}

auto TableBuilder::getBorderChars() const -> std::unordered_map<std::string, std::string> {
    switch (options_.style) {
        case TableStyle::UNICODE:
            return {
                {"horizontal", "─"},
                {"vertical", "│"},
                {"top_left", "┌"},
                {"top_right", "┐"},
                {"bottom_left", "└"},
                {"bottom_right", "┘"},
                {"top_junction", "┬"},
                {"bottom_junction", "┴"},
                {"cross", "┼"}
            };
        case TableStyle::ASCII:
        default:
            return {
                {"horizontal", "-"},
                {"vertical", "|"},
                {"top_left", "+"},
                {"top_right", "+"},
                {"bottom_left", "+"},
                {"bottom_right", "+"},
                {"top_junction", "+"},
                {"bottom_junction", "+"},
                {"cross", "+"}
            };
    }
}

auto createSimpleTable(const std::string& title,
                      const std::unordered_map<std::string, std::string>& data,
                      const TableOptions& options) -> std::string {
    TableBuilder builder(options);
    
    if (!title.empty()) {
        builder.addColumn(title, 0, Alignment::LEFT)
               .addColumn("Value", 0, Alignment::LEFT);
    } else {
        builder.addColumn("Property", 0, Alignment::LEFT)
               .addColumn("Value", 0, Alignment::LEFT);
    }
    
    for (const auto& [key, value] : data) {
        builder.addRow({key, value});
    }
    
    return builder.build();
}

auto createTable(const std::vector<std::string>& headers,
                const std::vector<std::vector<std::string>>& rows,
                const TableOptions& options) -> std::string {
    TableBuilder builder(options);
    
    for (const auto& header : headers) {
        builder.addColumn(header);
    }
    
    for (const auto& row : rows) {
        builder.addRow(row);
    }
    
    return builder.build();
}

auto formatText(const std::string& text, int width, Alignment alignment, char padding) -> std::string {
    if (static_cast<int>(text.length()) >= width) {
        return text.substr(0, width);
    }
    
    int padSize = width - static_cast<int>(text.length());
    
    switch (alignment) {
        case Alignment::LEFT:
            return text + std::string(padSize, padding);
        case Alignment::RIGHT:
            return std::string(padSize, padding) + text;
        case Alignment::CENTER: {
            int leftPad = padSize / 2;
            int rightPad = padSize - leftPad;
            return std::string(leftPad, padding) + text + std::string(rightPad, padding);
        }
        default:
            return text + std::string(padSize, padding);
    }
}

auto truncateText(const std::string& text, int width, const std::string& ellipsis) -> std::string {
    if (static_cast<int>(text.length()) <= width) {
        return text;
    }
    
    if (width <= static_cast<int>(ellipsis.length())) {
        return ellipsis.substr(0, width);
    }
    
    return text.substr(0, width - static_cast<int>(ellipsis.length())) + ellipsis;
}

auto wrapText(const std::string& text, int width) -> std::vector<std::string> {
    std::vector<std::string> lines;
    std::istringstream words(text);
    std::string word;
    std::string currentLine;
    
    while (words >> word) {
        if (currentLine.empty()) {
            currentLine = word;
        } else if (static_cast<int>(currentLine.length() + word.length() + 1) <= width) {
            currentLine += " " + word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
        }
    }
    
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }
    
    return lines;
}

} // namespace atom::system::utils
