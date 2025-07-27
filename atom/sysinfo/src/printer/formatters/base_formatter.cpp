#include "base_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace atom::system {

void BaseFormatter::setStyle(FormatterStyle style) {
    options_.style = style;
}

void BaseFormatter::setOutputFormat(OutputFormat format) {
    options_.format = format;
}

void BaseFormatter::setColorEnabled(bool enabled) {
    options_.colorEnabled = enabled;
    if (enabled) {
        initializeColors();
    }
}

void BaseFormatter::setTimestampEnabled(bool enabled) {
    options_.timestampEnabled = enabled;
}

void BaseFormatter::setOptions(const FormatterOptions& options) {
    options_ = options;
    if (options_.colorEnabled) {
        initializeColors();
    }
}

auto BaseFormatter::getOptions() const -> const FormatterOptions& {
    return options_;
}

void BaseFormatter::setCustomOption(const std::string& key, const std::string& value) {
    options_.customOptions[key] = value;
}

auto BaseFormatter::getCustomOption(const std::string& key, const std::string& defaultValue) const -> std::string {
    auto it = options_.customOptions.find(key);
    return (it != options_.customOptions.end()) ? it->second : defaultValue;
}

auto BaseFormatter::createTableRow(const std::string& label, const std::string& value, int width) const -> std::string {
    int effectiveWidth = (width > 0) ? width : getTableWidth();
    int labelWidth = effectiveWidth * 0.4; // 40% for label
    int valueWidth = effectiveWidth - labelWidth - 5; // Rest for value, minus separators
    
    switch (options_.format) {
        case OutputFormat::TABLE:
            return std::format("| {:<{}} | {:<{}} |\n", label, labelWidth, value, valueWidth);
        case OutputFormat::PLAIN_TEXT:
            return std::format("{}: {}\n", label, value);
        case OutputFormat::JSON:
            return std::format("  \"{}\": \"{}\",\n", label, value);
        case OutputFormat::XML:
            return std::format("  <{}>{}</{}>\n", label, value, label);
        case OutputFormat::MARKDOWN:
            return std::format("| {} | {} |\n", label, value);
        case OutputFormat::HTML:
            return std::format("  <tr><td>{}</td><td>{}</td></tr>\n", label, value);
        default:
            return std::format("| {:<{}} | {:<{}} |\n", label, labelWidth, value, valueWidth);
    }
}

auto BaseFormatter::createTableHeader(const std::string& title, int width) const -> std::string {
    int effectiveWidth = (width > 0) ? width : getTableWidth();
    
    switch (options_.format) {
        case OutputFormat::TABLE: {
            std::stringstream ss;
            ss << "\n" << colorize(title, "bold") << "\n";
            ss << std::string(effectiveWidth, '-') << "\n";
            ss << std::format("| {:<{}} | {:<{}} |\n", "Parameter", effectiveWidth * 0.4, "Value", effectiveWidth * 0.6 - 5);
            ss << std::string(effectiveWidth, '-') << "\n";
            return ss.str();
        }
        case OutputFormat::PLAIN_TEXT:
            return std::format("\n=== {} ===\n", title);
        case OutputFormat::JSON:
            return std::format("  \"{}\": {{\n", title);
        case OutputFormat::XML:
            return std::format("<{}>\n", title);
        case OutputFormat::MARKDOWN:
            return std::format("\n## {}\n\n| Parameter | Value |\n|-----------|-------|\n", title);
        case OutputFormat::HTML:
            return std::format("<h3>{}</h3>\n<table>\n  <tr><th>Parameter</th><th>Value</th></tr>\n", title);
        default:
            return createTableHeader(title, width);
    }
}

auto BaseFormatter::createTableFooter(int width) const -> std::string {
    int effectiveWidth = (width > 0) ? width : getTableWidth();
    
    switch (options_.format) {
        case OutputFormat::TABLE:
            return std::string(effectiveWidth, '-') + "\n\n";
        case OutputFormat::PLAIN_TEXT:
            return "\n";
        case OutputFormat::JSON:
            return "  },\n";
        case OutputFormat::XML:
            return "</>\n";
        case OutputFormat::MARKDOWN:
            return "\n";
        case OutputFormat::HTML:
            return "</table>\n\n";
        default:
            return std::string(effectiveWidth, '-') + "\n\n";
    }
}

auto BaseFormatter::colorize(const std::string& text, const std::string& color) const -> std::string {
    if (!options_.colorEnabled) {
        return text;
    }
    
    auto it = colorCodes_.find(color);
    if (it != colorCodes_.end()) {
        return it->second + text + colorCodes_.at("reset");
    }
    
    return text;
}

auto BaseFormatter::formatWithUnits(double value, const std::string& unit, int precision) const -> std::string {
    if (!options_.unitsEnabled) {
        return std::format("{:.{}f}", value, precision);
    }
    return std::format("{:.{}f} {}", value, precision, unit);
}

auto BaseFormatter::formatBytes(uint64_t bytes, int precision) const -> std::string {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    const size_t numUnits = sizeof(units) / sizeof(units[0]);
    
    double value = static_cast<double>(bytes);
    size_t unitIndex = 0;
    
    while (value >= 1024.0 && unitIndex < numUnits - 1) {
        value /= 1024.0;
        ++unitIndex;
    }
    
    return formatWithUnits(value, units[unitIndex], precision);
}

auto BaseFormatter::formatPercentage(double percentage, int precision) const -> std::string {
    return formatWithUnits(percentage, "%", precision);
}

auto BaseFormatter::formatTemperature(double celsius, int precision) const -> std::string {
    return formatWithUnits(celsius, "°C", precision);
}

auto BaseFormatter::formatFrequency(double hertz, int precision) const -> std::string {
    if (hertz >= 1e9) {
        return formatWithUnits(hertz / 1e9, "GHz", precision);
    } else if (hertz >= 1e6) {
        return formatWithUnits(hertz / 1e6, "MHz", precision);
    } else if (hertz >= 1e3) {
        return formatWithUnits(hertz / 1e3, "kHz", precision);
    } else {
        return formatWithUnits(hertz, "Hz", precision);
    }
}

auto BaseFormatter::addTimestamp(const std::string& content) const -> std::string {
    if (!options_.timestampEnabled) {
        return content;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    return std::format("Generated at: {}\n\n{}", ss.str(), content);
}

auto BaseFormatter::getTableWidth() const -> int {
    return options_.tableWidth;
}

void BaseFormatter::initializeColors() {
    colorCodes_ = {
        {"reset", "\033[0m"},
        {"bold", "\033[1m"},
        {"dim", "\033[2m"},
        {"underline", "\033[4m"},
        {"black", "\033[30m"},
        {"red", "\033[31m"},
        {"green", "\033[32m"},
        {"yellow", "\033[33m"},
        {"blue", "\033[34m"},
        {"magenta", "\033[35m"},
        {"cyan", "\033[36m"},
        {"white", "\033[37m"},
        {"bg_black", "\033[40m"},
        {"bg_red", "\033[41m"},
        {"bg_green", "\033[42m"},
        {"bg_yellow", "\033[43m"},
        {"bg_blue", "\033[44m"},
        {"bg_magenta", "\033[45m"},
        {"bg_cyan", "\033[46m"},
        {"bg_white", "\033[47m"}
    };
}

} // namespace atom::system
