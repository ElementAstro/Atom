#include "format_utils.hpp"
#include <format>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace atom::system::utils {

auto formatBytes(uint64_t bytes) -> std::string {
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    const size_t numUnits = sizeof(units) / sizeof(units[0]);

    double value = static_cast<double>(bytes);
    size_t unitIndex = 0;

    while (value >= 1024.0 && unitIndex < numUnits - 1) {
        value /= 1024.0;
        ++unitIndex;
    }

    return std::format("{:.2f} {}", value, units[unitIndex]);
}

auto formatPercentage(double percentage, int precision) -> std::string {
    return std::format("{:.{}f}%", percentage, precision);
}

auto formatTemperature(double celsius, int precision) -> std::string {
    return std::format("{:.{}f}°C", celsius, precision);
}

auto formatFrequency(double hertz, int precision) -> std::string {
    if (hertz >= 1e9) {
        return std::format("{:.{}f} GHz", hertz / 1e9, precision);
    } else if (hertz >= 1e6) {
        return std::format("{:.{}f} MHz", hertz / 1e6, precision);
    } else if (hertz >= 1e3) {
        return std::format("{:.{}f} kHz", hertz / 1e3, precision);
    } else {
        return std::format("{:.{}f} Hz", hertz, precision);
    }
}

auto formatDuration(uint64_t seconds) -> std::string {
    if (seconds < 60) {
        return std::format("{} seconds", seconds);
    } else if (seconds < 3600) {
        return std::format("{} minutes, {} seconds", seconds / 60, seconds % 60);
    } else if (seconds < 86400) {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        return std::format("{} hours, {} minutes", hours, minutes);
    } else {
        uint64_t days = seconds / 86400;
        uint64_t hours = (seconds % 86400) / 3600;
        return std::format("{} days, {} hours", days, hours);
    }
}

auto formatWithUnits(double value, const std::string& unit, int precision) -> std::string {
    return std::format("{:.{}f} {}", value, precision, unit);
}

auto formatTimestamp() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

auto formatUptime(uint64_t uptimeSeconds) -> std::string {
    uint64_t days = uptimeSeconds / 86400;
    uint64_t hours = (uptimeSeconds % 86400) / 3600;
    uint64_t minutes = (uptimeSeconds % 3600) / 60;
    uint64_t seconds = uptimeSeconds % 60;

    std::stringstream ss;
    bool first = true;

    if (days > 0) {
        ss << days << (days == 1 ? " day" : " days");
        first = false;
    }

    if (hours > 0) {
        if (!first) ss << ", ";
        ss << hours << (hours == 1 ? " hour" : " hours");
        first = false;
    }

    if (minutes > 0) {
        if (!first) ss << ", ";
        ss << minutes << (minutes == 1 ? " minute" : " minutes");
        first = false;
    }

    if (seconds > 0 || first) {
        if (!first) ss << ", ";
        ss << seconds << (seconds == 1 ? " second" : " seconds");
    }

    return ss.str();
}

auto formatNetworkSpeed(double bytesPerSecond) -> std::string {
    double bitsPerSecond = bytesPerSecond * 8;

    if (bitsPerSecond >= 1e9) {
        return std::format("{:.2f} Gbps", bitsPerSecond / 1e9);
    } else if (bitsPerSecond >= 1e6) {
        return std::format("{:.2f} Mbps", bitsPerSecond / 1e6);
    } else if (bitsPerSecond >= 1e3) {
        return std::format("{:.2f} kbps", bitsPerSecond / 1e3);
    } else {
        return std::format("{:.2f} bps", bitsPerSecond);
    }
}

auto formatMemorySize(uint64_t bytes) -> std::string {
    return formatBytes(bytes);
}

auto formatDiskSpace(uint64_t bytes) -> std::string {
    return formatBytes(bytes);
}

auto formatBoolean(bool value) -> std::string {
    return value ? "Yes" : "No";
}

auto formatNumber(uint64_t number) -> std::string {
    std::string result = std::to_string(number);

    // Add thousands separators
    int insertPosition = result.length() - 3;
    while (insertPosition > 0) {
        result.insert(insertPosition, ",");
        insertPosition -= 3;
    }

    return result;
}

auto formatNumber(double number, int precision) -> std::string {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(precision) << number;
    std::string result = ss.str();

    // Find decimal point
    size_t decimalPos = result.find('.');
    if (decimalPos == std::string::npos) {
        decimalPos = result.length();
    }

    // Add thousands separators to integer part
    int insertPosition = decimalPos - 3;
    while (insertPosition > 0) {
        result.insert(insertPosition, ",");
        insertPosition -= 3;
        decimalPos += 1; // Adjust for inserted comma
    }

    return result;
}

} // namespace atom::system::utils