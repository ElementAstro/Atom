#include "battery_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <cmath>

namespace atom::system {

BatteryFormatter::BatteryFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto BatteryFormatter::format(const BatteryInfo& info) const -> std::string {
    return format(info, "Battery Information");
}

auto BatteryFormatter::format(const BatteryInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);

    // Battery presence
    ss << createTableRow("Battery Present", info.isBatteryPresent ? "Yes" : "No");

    if (!info.isBatteryPresent) {
        ss << createTableFooter();
        return addTimestamp(ss.str());
    }

    // Charging status
    ss << createTableRow("Charging Status", formatChargingStatus(info.isCharging));

    // Battery level
    ss << createTableRow("Battery Level", formatLevelWithColor(info.batteryLifePercent));

    // Battery level visualization for detailed styles
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Level Visualization", createBatteryLevelBar(info.batteryLifePercent, 25));

        // Add battery icon
        if (options_.style == FormatterStyle::VERBOSE) {
            ss << createTableRow("Status Icon", getBatteryIcon(info.batteryLifePercent, info.isCharging));
        }
    }

    // Time remaining
    if (info.batteryLifeTime > 0) {
        ss << createTableRow("Time Remaining", formatTimeDuration(info.batteryLifeTime));
    }

    // Battery health
    float health = info.getBatteryHealth();
    if (health > 0) {
        ss << createTableRow("Battery Health", formatHealthWithColor(health));
    }

    // Temperature
    if (info.temperature > 0) {
        ss << createTableRow("Temperature", formatTemperatureWithColor(info.temperature));
    }

    // Additional details for verbose style
    if (options_.style == FormatterStyle::VERBOSE) {
        if (!info.manufacturer.empty()) {
            ss << createTableRow("Manufacturer", info.manufacturer);
        }
        if (!info.model.empty()) {
            ss << createTableRow("Model", info.model);
        }
        if (!info.serialNumber.empty()) {
            ss << createTableRow("Serial Number", info.serialNumber);
        }
        if (info.cycleCounts > 0) {
            ss << createTableRow("Cycle Count", std::to_string(info.cycleCounts));
        }
    }

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto BatteryFormatter::formatBasic(const BatteryInfo& info) const -> std::string {
    if (!info.isBatteryPresent) {
        return "No battery present";
    }

    std::string status = info.isCharging ? "Charging" : "Discharging";
    return std::format("{}% ({})",
                      static_cast<int>(info.batteryLifePercent),
                      status);
}

auto BatteryFormatter::formatHealth(const BatteryInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Battery Health");

    if (!info.isBatteryPresent) {
        ss << createTableRow("Status", "No battery present");
        ss << createTableFooter();
        return ss.str();
    }

    float health = info.getBatteryHealth();
    ss << createTableRow("Health", formatHealthWithColor(health));

    if (info.cycleCounts > 0) {
        ss << createTableRow("Cycle Count", std::to_string(info.cycleCounts));
    }

    if (info.energyDesign > 0 && info.energyFull > 0) {
        float designCapacity = (info.energyFull / info.energyDesign) * 100.0f;
        ss << createTableRow("Design Capacity", formatPercentage(designCapacity));
    }

    ss << createTableFooter();

    return ss.str();
}

auto BatteryFormatter::formatPower(const BatteryInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Battery Power Information");

    if (!info.isBatteryPresent) {
        ss << createTableRow("Status", "No battery present");
        ss << createTableFooter();
        return ss.str();
    }

    if (info.voltageNow > 0) {
        ss << createTableRow("Voltage", formatWithUnits(info.voltageNow, "V"));
    }

    if (info.currentNow > 0) {
        ss << createTableRow("Current", formatWithUnits(info.currentNow, "A"));
    }

    if (info.energyNow > 0) {
        ss << createTableRow("Current Energy", formatWithUnits(info.energyNow, "Wh"));
    }

    if (info.energyFull > 0) {
        ss << createTableRow("Full Energy", formatWithUnits(info.energyFull, "Wh"));
    }

    if (info.energyDesign > 0) {
        ss << createTableRow("Design Energy", formatWithUnits(info.energyDesign, "Wh"));
    }

    ss << createTableFooter();

    return ss.str();
}

auto BatteryFormatter::formatLevelWithColor(float level) const -> std::string {
    auto levelStr = formatPercentage(level);
    return colorize(levelStr, getBatteryLevelColor(level));
}

auto BatteryFormatter::formatChargingStatus(bool isCharging) const -> std::string {
    if (isCharging) {
        return colorize("Charging ⚡", "green");
    } else {
        return colorize("Discharging", "yellow");
    }
}

auto BatteryFormatter::formatHealthWithColor(float health) const -> std::string {
    auto healthStr = formatPercentage(health);
    return colorize(healthStr, getBatteryHealthColor(health));
}

auto BatteryFormatter::formatTemperatureWithColor(float temperature) const -> std::string {
    auto tempStr = formatTemperature(temperature);
    return colorize(tempStr, getBatteryTemperatureColor(temperature));
}

auto BatteryFormatter::getBatteryLevelColor(float level) const -> std::string {
    if (level > 50) return "green";
    if (level > 20) return "yellow";
    if (level > 10) return "red";
    return "red";
}

auto BatteryFormatter::getBatteryHealthColor(float health) const -> std::string {
    if (health > 80) return "green";
    if (health > 60) return "yellow";
    if (health > 40) return "red";
    return "red";
}

auto BatteryFormatter::getBatteryTemperatureColor(float temperature) const -> std::string {
    if (temperature < 35) return "green";
    if (temperature < 45) return "yellow";
    if (temperature < 55) return "red";
    return "red";
}

auto BatteryFormatter::createBatteryLevelBar(float level, int width) const -> std::string {
    if (!options_.colorEnabled) {
        int filled = static_cast<int>((level / 100.0) * width);
        return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
    }

    int filled = static_cast<int>((level / 100.0) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            if (level > 50) {
                bar += colorize("█", "green");
            } else if (level > 20) {
                bar += colorize("█", "yellow");
            } else {
                bar += colorize("█", "red");
            }
        } else {
            bar += "░";
        }
    }

    bar += "]";
    return bar;
}

auto BatteryFormatter::formatTimeDuration(float minutes) const -> std::string {
    if (minutes <= 0) {
        return "Unknown";
    }

    int totalMinutes = static_cast<int>(std::round(minutes));
    int hours = totalMinutes / 60;
    int mins = totalMinutes % 60;

    if (hours > 0) {
        return std::format("{}h {}m", hours, mins);
    } else {
        return std::format("{}m", mins);
    }
}

auto BatteryFormatter::getBatteryIcon(float level, bool isCharging) const -> std::string {
    if (isCharging) {
        return "🔌";
    }

    if (level > 75) return "🔋";
    if (level > 50) return "🔋";
    if (level > 25) return "🪫";
    if (level > 10) return "🪫";
    return "🪫";
}

} // namespace atom::system
