#include "gpu_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

GpuFormatter::GpuFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto GpuFormatter::format(const std::vector<GpuInfo>& gpus) const -> std::string {
    return format(gpus, "GPU Information");
}

auto GpuFormatter::format(const std::vector<GpuInfo>& gpus, const std::string& title) const -> std::string {
    if (gpus.empty()) {
        std::stringstream ss;
        ss << createTableHeader(title);
        ss << createTableRow("Status", "No GPU information available");
        ss << createTableFooter();
        return ss.str();
    }

    std::stringstream ss;
    ss << createTableHeader(title);

    for (size_t i = 0; i < gpus.size(); ++i) {
        ss << formatSingle(gpus[i], static_cast<int>(i + 1));
        if (i < gpus.size() - 1) {
            ss << "\n"; // Add spacing between GPUs
        }
    }

    ss << createTableFooter();
    return ss.str();
}

auto GpuFormatter::formatSingle(const GpuInfo& gpu, int index) const -> std::string {
    std::stringstream ss;

    std::string prefix = (index > 0) ? "GPU " + std::to_string(index) + " " : "";

    // Basic information
    ss << createTableRow(prefix + "Name", gpu.name);
    ss << createTableRow(prefix + "Vendor", gpu.vendor);
    ss << createTableRow(prefix + "Type", gpu.isIntegrated ? "Integrated" : "Discrete");

    if (!gpu.driverVersion.empty()) {
        ss << createTableRow(prefix + "Driver Version", gpu.driverVersion);
    }

    // Memory information
    if (gpu.memoryTotal > 0) {
        ss << createTableRow(prefix + "Total Memory", formatBytes(gpu.memoryTotal));

        if (gpu.memoryUsed > 0) {
            ss << createTableRow(prefix + "Used Memory", formatMemoryUsage(gpu.memoryUsed, gpu.memoryTotal));

            // Memory usage visualization for detailed styles
            if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
                float memoryUsagePercent = (static_cast<float>(gpu.memoryUsed) / gpu.memoryTotal) * 100.0f;
                ss << createTableRow(prefix + "Memory Usage Bar", createUsageBar(memoryUsagePercent, 25));
            }
        }
    }

    // Performance metrics
    if (gpu.usage >= 0) {
        ss << createTableRow(prefix + "GPU Usage", formatUsageWithColor(gpu.usage));

        if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
            ss << createTableRow(prefix + "Usage Bar", createUsageBar(gpu.usage, 25));
        }
    }

    // Temperature
    if (gpu.temperature > 0) {
        ss << createTableRow(prefix + "Temperature", formatTemperatureWithColor(gpu.temperature));
    }

    // Clock speeds
    if (gpu.baseClock > 0) {
        ss << createTableRow(prefix + "Base Clock", formatFrequency(gpu.baseClock * 1e6)); // Assuming MHz input
    }

    if (gpu.memoryClock > 0) {
        ss << createTableRow(prefix + "Memory Clock", formatFrequency(gpu.memoryClock * 1e6)); // Assuming MHz input
    }

    // Core count
    if (gpu.coreCount > 0) {
        ss << createTableRow(prefix + "Cores", std::to_string(gpu.coreCount));
    }

    // API support (verbose mode only)
    if (options_.style == FormatterStyle::VERBOSE && !gpu.apiSupport.empty()) {
        ss << createTableRow(prefix + "API Support", gpu.apiSupport);
    }

    return ss.str();
}

auto GpuFormatter::formatPerformance(const GpuInfo& gpu) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("GPU Performance");
    ss << createTableRow("GPU Name", gpu.name);

    if (gpu.usage >= 0) {
        ss << createTableRow("Current Usage", formatUsageWithColor(gpu.usage));
        ss << createTableRow("Usage Visualization", createUsageBar(gpu.usage, 30));
    }

    if (gpu.memoryTotal > 0 && gpu.memoryUsed > 0) {
        float memoryUsagePercent = (static_cast<float>(gpu.memoryUsed) / gpu.memoryTotal) * 100.0f;
        ss << createTableRow("Memory Usage", formatPercentage(memoryUsagePercent));
        ss << createTableRow("Memory Visualization", createUsageBar(memoryUsagePercent, 30));
    }

    if (gpu.temperature > 0) {
        ss << createTableRow("Temperature", formatTemperatureWithColor(gpu.temperature));
    }

    if (gpu.baseClock > 0) {
        ss << createTableRow("Base Clock", formatFrequency(gpu.baseClock * 1e6));
    }

    ss << createTableFooter();

    return ss.str();
}

auto GpuFormatter::formatUsageWithColor(float usage) const -> std::string {
    auto usageStr = formatPercentage(usage);
    return colorize(usageStr, getUsageColor(usage));
}

auto GpuFormatter::formatTemperatureWithColor(float temperature) const -> std::string {
    auto tempStr = formatTemperature(temperature);
    return colorize(tempStr, getTemperatureColor(temperature));
}

auto GpuFormatter::getUsageColor(float usage) const -> std::string {
    if (usage < 30) return "green";
    if (usage < 60) return "yellow";
    if (usage < 85) return "red";
    return "red";
}

auto GpuFormatter::getTemperatureColor(float temperature) const -> std::string {
    if (temperature < 60) return "green";
    if (temperature < 75) return "yellow";
    if (temperature < 85) return "red";
    return "red";
}

auto GpuFormatter::createUsageBar(float usage, int width) const -> std::string {
    if (!options_.colorEnabled) {
        int filled = static_cast<int>((usage / 100.0) * width);
        return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
    }

    int filled = static_cast<int>((usage / 100.0) * width);
    std::string bar = "[";

    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            if (usage < 30) {
                bar += colorize("█", "green");
            } else if (usage < 60) {
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

auto GpuFormatter::formatMemoryUsage(uint64_t used, uint64_t total) const -> std::string {
    float usagePercent = (static_cast<float>(used) / total) * 100.0f;
    return std::format("{} / {} ({:.1f}%)",
                      formatBytes(used),
                      formatBytes(total),
                      usagePercent);
}

} // namespace atom::system