#include "memory_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

MemoryFormatter::MemoryFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto MemoryFormatter::format(const MemoryInfo& info) const -> std::string {
    return format(info, "Memory Information");
}

auto MemoryFormatter::format(const MemoryInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader(title);
    
    // Basic memory information
    ss << createTableRow("Total Physical Memory", formatBytes(info.totalPhysicalMemory));
    ss << createTableRow("Available Physical Memory", formatBytes(info.availablePhysicalMemory));
    
    // Calculate and display used memory
    uint64_t usedMemory = calculateUsedMemory(info.totalPhysicalMemory, info.availablePhysicalMemory);
    ss << createTableRow("Used Physical Memory", formatBytes(usedMemory));
    
    // Memory usage percentage
    ss << createTableRow("Memory Usage", formatUsageWithColor(info.memoryLoadPercentage));
    
    // Usage visualization for detailed styles
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Usage Visualization", createMemoryUsageBar(info.memoryLoadPercentage, 30));
    }
    
    // Virtual memory information
    if (info.virtualMemoryMax > 0) {
        ss << createTableRow("Total Virtual Memory", formatBytes(info.virtualMemoryMax));
    }
    
    // Additional details for verbose style
    if (options_.style == FormatterStyle::VERBOSE) {
        // Memory efficiency
        double efficiency = (static_cast<double>(usedMemory) / info.totalPhysicalMemory) * 100.0;
        ss << createTableRow("Memory Efficiency", formatPercentage(efficiency));
        
        // Available memory ratio
        double availableRatio = (static_cast<double>(info.availablePhysicalMemory) / info.totalPhysicalMemory) * 100.0;
        ss << createTableRow("Available Memory Ratio", formatPercentage(availableRatio));
    }
    
    ss << createTableFooter();
    
    // Add memory slots information if available and in detailed mode
    if ((options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) && 
        !info.memorySlots.empty()) {
        ss << formatMemorySlots(info);
    }
    
    return addTimestamp(ss.str());
}

auto MemoryFormatter::formatBasic(const MemoryInfo& info) const -> std::string {
    uint64_t usedMemory = calculateUsedMemory(info.totalPhysicalMemory, info.availablePhysicalMemory);
    
    return std::format("{} / {} ({:.1f}% used)", 
                      formatBytes(usedMemory),
                      formatBytes(info.totalPhysicalMemory),
                      info.memoryLoadPercentage);
}

auto MemoryFormatter::formatUsageSummary(const MemoryInfo& info) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader("Memory Usage Summary");
    
    uint64_t usedMemory = calculateUsedMemory(info.totalPhysicalMemory, info.availablePhysicalMemory);
    
    ss << createTableRow("Total", formatBytes(info.totalPhysicalMemory));
    ss << createTableRow("Used", formatBytes(usedMemory));
    ss << createTableRow("Available", formatBytes(info.availablePhysicalMemory));
    ss << createTableRow("Usage", formatUsageWithColor(info.memoryLoadPercentage));
    ss << createTableRow("Usage Bar", createMemoryUsageBar(info.memoryLoadPercentage, 25));
    
    ss << createTableFooter();
    
    return ss.str();
}

auto MemoryFormatter::formatVirtualMemory(const MemoryInfo& info) const -> std::string {
    if (info.virtualMemoryMax == 0) {
        return "";
    }
    
    std::stringstream ss;
    
    ss << createTableHeader("Virtual Memory Information");
    ss << createTableRow("Total Virtual Memory", formatBytes(info.virtualMemoryMax));
    
    // Calculate virtual memory usage if we have the information
    if (info.virtualMemoryUsed > 0) {
        ss << createTableRow("Used Virtual Memory", formatBytes(info.virtualMemoryUsed));
        double virtualUsagePercent = (static_cast<double>(info.virtualMemoryUsed) / info.virtualMemoryMax) * 100.0;
        ss << createTableRow("Virtual Memory Usage", formatUsageWithColor(virtualUsagePercent));
    }
    
    ss << createTableFooter();
    
    return ss.str();
}

auto MemoryFormatter::formatMemorySlots(const MemoryInfo& info) const -> std::string {
    if (info.memorySlots.empty()) {
        return "";
    }
    
    std::stringstream ss;
    
    ss << createTableHeader("Memory Slots Information");
    
    for (size_t i = 0; i < info.memorySlots.size(); ++i) {
        const auto& slot = info.memorySlots[i];
        ss << formatMemorySlot(slot, static_cast<int>(i + 1));
    }
    
    ss << createTableFooter();
    
    return ss.str();
}

auto MemoryFormatter::formatUsageWithColor(float usagePercent) const -> std::string {
    auto usageStr = formatPercentage(usagePercent);
    return colorize(usageStr, getUsageColor(usagePercent));
}

auto MemoryFormatter::getUsageColor(float usagePercent) const -> std::string {
    if (usagePercent < 50) return "green";
    if (usagePercent < 75) return "yellow";
    if (usagePercent < 90) return "red";
    return "red";
}

auto MemoryFormatter::calculateUsedMemory(uint64_t total, uint64_t available) const -> uint64_t {
    return (total > available) ? (total - available) : 0;
}

auto MemoryFormatter::createMemoryUsageBar(float usagePercent, int width) const -> std::string {
    if (!options_.colorEnabled) {
        int filled = static_cast<int>((usagePercent / 100.0) * width);
        return "[" + std::string(filled, '#') + std::string(width - filled, '-') + "]";
    }
    
    int filled = static_cast<int>((usagePercent / 100.0) * width);
    std::string bar = "[";
    
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            if (usagePercent < 50) {
                bar += colorize("█", "green");
            } else if (usagePercent < 75) {
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

auto MemoryFormatter::formatMemorySlot(const MemoryInfo::MemorySlot& slot, int slotIndex) const -> std::string {
    std::stringstream ss;
    
    std::string slotName = std::format("Slot {}", slotIndex);
    
    if (!slot.capacity.empty()) {
        ss << createTableRow(slotName + " Capacity", slot.capacity);
    }
    
    if (!slot.clockSpeed.empty()) {
        ss << createTableRow(slotName + " Speed", slot.clockSpeed);
    }
    
    if (!slot.type.empty()) {
        ss << createTableRow(slotName + " Type", slot.type);
    }
    
    return ss.str();
}

} // namespace atom::system
