#include "os_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <algorithm>

namespace atom::system {

OsFormatter::OsFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto OsFormatter::format(const OperatingSystemInfo& info) const -> std::string {
    return format(info, "Operating System Information");
}

auto OsFormatter::format(const OperatingSystemInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);

    // OS name with icon for verbose style
    if (options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("OS Name", getOsIcon(info.osName) + " " + info.osName);
    } else {
        ss << createTableRow("OS Name", info.osName);
    }

    // OS version
    ss << createTableRow("OS Version", info.osVersion);

    // Kernel version
    ss << createTableRow("Kernel Version", info.kernelVersion);

    // Architecture
    ss << createTableRow("Architecture", info.architecture);

    // Computer name
    ss << createTableRow("Computer Name", info.computerName);

    // Boot time and uptime
    ss << createTableRow("Boot Time", info.bootTime);
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        auto uptime = formatUptime(info.bootTime);
        if (!uptime.empty()) {
            ss << createTableRow("Uptime", uptime);
        }
    }

    // Installation and update information
    if (!info.installDate.empty()) {
        ss << createTableRow("Install Date", info.installDate);
    }

    if (!info.lastUpdate.empty()) {
        ss << createTableRow("Last Update", info.lastUpdate);
    }

    // System configuration
    if (!info.timeZone.empty()) {
        ss << createTableRow("Time Zone", info.timeZone);
    }

    if (!info.charSet.empty()) {
        ss << createTableRow("Character Set", info.charSet);
    }

    // Server status
    ss << createTableRow("Server Edition", formatServerStatus(info.isServer));

    // Compiler information (if available and in verbose mode)
    if (options_.style == FormatterStyle::VERBOSE && !info.compiler.empty()) {
        ss << createTableRow("Compiler", info.compiler);
    }

    // Installed updates (for detailed/verbose styles)
    if ((options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) &&
        !info.installedUpdates.empty()) {
        ss << createTableRow("Installed Updates", std::to_string(info.installedUpdates.size()) + " updates");

        if (options_.style == FormatterStyle::VERBOSE && info.installedUpdates.size() <= 5) {
            for (size_t i = 0; i < std::min(info.installedUpdates.size(), size_t(5)); ++i) {
                ss << createTableRow("  Update " + std::to_string(i + 1), info.installedUpdates[i]);
            }
        }
    }

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto OsFormatter::formatBasic(const OperatingSystemInfo& info) const -> std::string {
    return std::format("{} {} ({})",
                      info.osName,
                      info.osVersion,
                      info.architecture);
}

auto OsFormatter::formatVersion(const OperatingSystemInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("OS Version Information");
    ss << createTableRow("OS Name", info.osName);
    ss << createTableRow("OS Version", info.osVersion);
    ss << createTableRow("Kernel Version", info.kernelVersion);
    ss << createTableRow("Architecture", info.architecture);

    if (!info.compiler.empty()) {
        ss << createTableRow("Compiler", info.compiler);
    }

    ss << createTableFooter();

    return ss.str();
}

auto OsFormatter::formatSystem(const OperatingSystemInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("System Information");
    ss << createTableRow("Computer Name", info.computerName);
    ss << createTableRow("Architecture", info.architecture);
    ss << createTableRow("Server Edition", formatServerStatus(info.isServer));

    if (!info.timeZone.empty()) {
        ss << createTableRow("Time Zone", info.timeZone);
    }

    if (!info.charSet.empty()) {
        ss << createTableRow("Character Set", info.charSet);
    }

    ss << createTableRow("Boot Time", info.bootTime);

    auto uptime = formatUptime(info.bootTime);
    if (!uptime.empty()) {
        ss << createTableRow("Uptime", uptime);
    }

    ss << createTableFooter();

    return ss.str();
}

auto OsFormatter::formatServerStatus(bool isServer) const -> std::string {
    if (isServer) {
        return colorize("Yes", "green");
    } else {
        return colorize("No", "blue");
    }
}

auto OsFormatter::getOsIcon(const std::string& osName) const -> std::string {
    std::string lowerName = osName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("windows") != std::string::npos) {
        return "🪟";
    } else if (lowerName.find("linux") != std::string::npos ||
               lowerName.find("ubuntu") != std::string::npos ||
               lowerName.find("debian") != std::string::npos ||
               lowerName.find("fedora") != std::string::npos ||
               lowerName.find("centos") != std::string::npos ||
               lowerName.find("rhel") != std::string::npos) {
        return "🐧";
    } else if (lowerName.find("macos") != std::string::npos ||
               lowerName.find("darwin") != std::string::npos ||
               lowerName.find("mac") != std::string::npos) {
        return "🍎";
    } else if (lowerName.find("freebsd") != std::string::npos ||
               lowerName.find("openbsd") != std::string::npos ||
               lowerName.find("netbsd") != std::string::npos) {
        return "👹";
    } else {
        return "💻";
    }
}

auto OsFormatter::formatUptime(const std::string& bootTime) const -> std::string {
    // This is a simplified implementation
    // In a real implementation, you would parse the boot time and calculate uptime
    // For now, we'll return an empty string to indicate uptime calculation is not implemented
    return "";
}

} // namespace atom::system
