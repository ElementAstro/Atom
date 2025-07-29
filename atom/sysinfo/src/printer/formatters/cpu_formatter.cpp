#include "cpu_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

CpuFormatter::CpuFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto CpuFormatter::format(const CpuInfo& info) const -> std::string {
    return format(info, "CPU Information");
}

auto CpuFormatter::format(const CpuInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader(title);
    
    // Basic information
    ss << createTableRow("Model", info.model);
    ss << createTableRow("Vendor", formatVendor(info.vendor));
    ss << createTableRow("Architecture", formatArchitecture(info.architecture));
    
    // Core information
    ss << createTableRow("Physical Cores", std::to_string(info.numPhysicalCores));
    ss << createTableRow("Logical Cores", std::to_string(info.numLogicalCores));
    
    // Frequency information
    ss << createTableRow("Base Frequency", formatFrequency(info.baseFrequency * 1e9));
    if (info.maxFrequency > 0) {
        ss << createTableRow("Max Frequency", formatFrequency(info.maxFrequency * 1e9));
    }
    if (info.minFrequency > 0) {
        ss << createTableRow("Min Frequency", formatFrequency(info.minFrequency * 1e9));
    }
    
    // Performance metrics
    if (info.usage >= 0) {
        ss << createTableRow("Current Usage", formatUsageWithColor(info.usage));
        if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
            ss << createTableRow("Usage Bar", createProgressBar(info.usage));
        }
    }
    
    // Thermal information
    if (info.temperature > 0) {
        ss << createTableRow("Temperature", formatTemperatureWithColor(info.temperature));
    }
    
    // Socket information
    if (!info.socketType.empty()) {
        ss << createTableRow("Socket Type", info.socketType);
    }
    
    // Cache information (detailed styles only)
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        auto cacheInfo = formatCacheSizes(info.caches);
        if (!cacheInfo.empty()) {
            ss << cacheInfo;
        }
    }
    
    ss << createTableFooter();
    
    return addTimestamp(ss.str());
}

auto CpuFormatter::formatBasic(const CpuInfo& info) const -> std::string {
    return std::format("{} ({} cores, {} threads) @ {}", 
                      info.model, 
                      info.numPhysicalCores, 
                      info.numLogicalCores,
                      formatFrequency(info.baseFrequency * 1e9));
}

auto CpuFormatter::formatPerformance(const CpuInfo& info) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader("CPU Performance");
    ss << createTableRow("Model", info.model);
    ss << createTableRow("Base Frequency", formatFrequency(info.baseFrequency * 1e9));
    
    if (info.usage >= 0) {
        ss << createTableRow("Current Usage", formatUsageWithColor(info.usage));
        ss << createTableRow("Usage Visualization", createProgressBar(info.usage, 30));
    }
    
    if (info.temperature > 0) {
        ss << createTableRow("Temperature", formatTemperatureWithColor(info.temperature));
    }
    
    ss << createTableFooter();
    
    return ss.str();
}

auto CpuFormatter::formatThermal(const CpuInfo& info) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader("CPU Thermal Information");
    ss << createTableRow("Model", info.model);
    
    if (info.temperature > 0) {
        ss << createTableRow("Current Temperature", formatTemperatureWithColor(info.temperature));
        
        // Add thermal status based on temperature
        std::string thermalStatus;
        if (info.temperature < 50) {
            thermalStatus = colorize("Cool", "green");
        } else if (info.temperature < 70) {
            thermalStatus = colorize("Normal", "yellow");
        } else if (info.temperature < 85) {
            thermalStatus = colorize("Warm", "red");
        } else {
            thermalStatus = colorize("Hot", "red");
        }
        ss << createTableRow("Thermal Status", thermalStatus);
    } else {
        ss << createTableRow("Temperature", "Not available");
    }
    
    ss << createTableFooter();
    
    return ss.str();
}

auto CpuFormatter::formatCache(const CpuInfo& info) const -> std::string {
    return formatCacheSizes(info.caches);
}

auto CpuFormatter::formatVendor(CpuVendor vendor) const -> std::string {
    switch (vendor) {
        case CpuVendor::INTEL:
            return "Intel";
        case CpuVendor::AMD:
            return "AMD";
        case CpuVendor::ARM:
            return "ARM";
        case CpuVendor::APPLE:
            return "Apple";
        case CpuVendor::QUALCOMM:
            return "Qualcomm";
        case CpuVendor::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto CpuFormatter::formatArchitecture(CpuArchitecture arch) const -> std::string {
    switch (arch) {
        case CpuArchitecture::X86:
            return "x86";
        case CpuArchitecture::X86_64:
            return "x86_64";
        case CpuArchitecture::ARM:
            return "ARM";
        case CpuArchitecture::ARM64:
            return "ARM64";
        case CpuArchitecture::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto CpuFormatter::formatUsageWithColor(float usage) const -> std::string {
    auto usageStr = formatPercentage(usage);
    return colorize(usageStr, getUsageColor(usage));
}

auto CpuFormatter::formatTemperatureWithColor(float temperature) const -> std::string {
    auto tempStr = formatTemperature(temperature);
    return colorize(tempStr, getTemperatureColor(temperature));
}

auto CpuFormatter::getUsageColor(float usage) const -> std::string {
    if (usage < 30) return "green";
    if (usage < 60) return "yellow";
    if (usage < 85) return "red";
    return "red";
}

auto CpuFormatter::getTemperatureColor(float temperature) const -> std::string {
    if (temperature < 50) return "green";
    if (temperature < 70) return "yellow";
    if (temperature < 85) return "red";
    return "red";
}

auto CpuFormatter::formatCacheSizes(const CacheSizes& caches) const -> std::string {
    std::stringstream ss;
    
    bool hasCache = false;
    if (caches.l1d > 0 || caches.l1i > 0 ||
        caches.l2 > 0 || caches.l3 > 0) {
        hasCache = true;
    }

    if (!hasCache) {
        return "";
    }

    ss << createTableHeader("CPU Cache Information");

    if (caches.l1d > 0) {
        ss << createTableRow("L1 Data Cache", formatBytes(caches.l1d));
    }
    if (caches.l1i > 0) {
        ss << createTableRow("L1 Instruction Cache", formatBytes(caches.l1i));
    }
    if (caches.l2 > 0) {
        ss << createTableRow("L2 Cache", formatBytes(caches.l2));
    }
    if (caches.l3 > 0) {
        ss << createTableRow("L3 Cache", formatBytes(caches.l3));
    }
    
    ss << createTableFooter();
    
    return ss.str();
}

auto CpuFormatter::createProgressBar(float percentage, int width) const -> std::string {
    if (!options_.colorEnabled) {
        int filled = static_cast<int>((percentage / 100.0) * width);
        return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
    }
    
    int filled = static_cast<int>((percentage / 100.0) * width);
    std::string bar = "[";
    
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            if (percentage < 30) {
                bar += colorize("█", "green");
            } else if (percentage < 60) {
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

} // namespace atom::system
