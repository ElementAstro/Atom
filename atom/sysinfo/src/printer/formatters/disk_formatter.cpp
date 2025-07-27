#include "disk_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>

namespace atom::system {

DiskFormatter::DiskFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto DiskFormatter::format(const std::vector<DiskInfo>& disks) const -> std::string {
    return format(disks, "Disk Information");
}

auto DiskFormatter::format(const std::vector<DiskInfo>& disks, const std::string& title) const -> std::string {
    std::stringstream ss;
    
    ss << createTableHeader(title);
    
    for (size_t i = 0; i < disks.size(); ++i) {
        const auto& disk = disks[i];
        ss << formatSingle(disk, static_cast<int>(i + 1));
    }
    
    ss << createTableFooter();
    
    return addTimestamp(ss.str());
}

auto DiskFormatter::formatSingle(const DiskInfo& disk, int index) const -> std::string {
    std::stringstream ss;
    
    std::string prefix = (index > 0) ? "Disk " + std::to_string(index) + " " : "";
    
    ss << createTableRow(prefix + "Model", disk.model);
    ss << createTableRow(prefix + "Type", disk.fsType.empty() ? "Unknown" : disk.fsType);
    ss << createTableRow(prefix + "Size", formatBytes(disk.totalSpace));
    ss << createTableRow(prefix + "Free Space", formatBytes(disk.freeSpace));
    
    if (disk.totalSpace > 0) {
        double usagePercent = ((disk.totalSpace - disk.freeSpace) * 100.0) / disk.totalSpace;
        ss << createTableRow(prefix + "Usage", formatPercentage(usagePercent));
    }
    
    return ss.str();
}

} // namespace atom::system
