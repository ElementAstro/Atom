#include "system_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

SystemFormatter::SystemFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto SystemFormatter::format(const SystemInfo& info) const -> std::string {
    return format(info, "System Information");
}

auto SystemFormatter::format(const SystemInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);

    // Desktop environment
    ss << createTableRow("Desktop Environment", info.desktopEnvironment);
    ss << createTableRow("Window Manager", info.windowManager);

    // Theme information
    if (!info.wmTheme.empty()) {
        ss << createTableRow("WM Theme", info.wmTheme);
    }
    if (!info.icons.empty()) {
        ss << createTableRow("Icon Theme", info.icons);
    }
    if (!info.font.empty()) {
        ss << createTableRow("Font", info.font);
    }
    if (!info.cursor.empty()) {
        ss << createTableRow("Cursor Theme", info.cursor);
    }

    // Additional details for verbose mode
    if (options_.style == FormatterStyle::VERBOSE) {
        // All available fields are already shown above
        // This section can be used for future extensions
    }

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto SystemFormatter::formatBasic(const SystemInfo& info) const -> std::string {
    return std::format("{} ({})", info.desktopEnvironment, info.windowManager);
}

auto SystemFormatter::formatDesktop(const SystemInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Desktop Environment");
    ss << createTableRow("Desktop Environment", info.desktopEnvironment);
    ss << createTableRow("Window Manager", info.windowManager);

    ss << createTableFooter();

    return ss.str();
}

auto SystemFormatter::formatTheme(const SystemInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Theme Information");

    if (!info.wmTheme.empty()) {
        ss << createTableRow("WM Theme", info.wmTheme);
    }
    if (!info.icons.empty()) {
        ss << createTableRow("Icon Theme", info.icons);
    }
    if (!info.font.empty()) {
        ss << createTableRow("Font", info.font);
    }
    if (!info.cursor.empty()) {
        ss << createTableRow("Cursor Theme", info.cursor);
    }

    ss << createTableFooter();

    return ss.str();
}

} // namespace atom::system