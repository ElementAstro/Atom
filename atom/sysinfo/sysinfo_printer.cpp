#include "sysinfo_printer.hpp"
#include <format>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include "wifi.hpp"
#include "bios.hpp"

/**
 * 辅助函数：将磁盘文件系统类型字符串标准化输出
 */
static std::string diskTypeToString(const std::string& fsType) {
    if (fsType.empty()) return "Unknown";
    // 可根据需要做大小写转换或映射
    return fsType;
}
namespace atom::system {

// Format helper functions
auto SystemInfoPrinter::createTableRow(const std::string& label,
                                       const std::string& value)
    -> std::string {
    return std::format("| {:<30} | {:<40} |\n", label, value);
}

auto SystemInfoPrinter::createTableHeader(const std::string& title)
    -> std::string {
    std::stringstream ss;
    ss << "\n" << title << "\n";
    ss << "|--------------------------------|----------------------------------"
          "------|\n";
    ss << "| Parameter                      | Value                            "
          "      |\n";
    ss << "|--------------------------------|----------------------------------"
          "------|\n";
    return ss.str();
}

auto SystemInfoPrinter::createTableFooter() -> std::string {
    return "|--------------------------------|---------------------------------"
           "-------|\n\n";
}

// Format component information
auto SystemInfoPrinter::formatBatteryInfo(const BatteryInfo& info)
    -> std::string {
    std::stringstream ss;
    ss << createTableHeader("Battery Information");
    ss << createTableRow("Battery Present",
                         info.isBatteryPresent ? "Yes" : "No");
    ss << createTableRow("Charging Status",
                         info.isCharging ? "Charging" : "Not Charging");
    ss << createTableRow("Battery Level",
                         std::format("{}%", info.batteryLifePercent));
    ss << createTableRow("Time Remaining",
                         std::format("{:.1f} minutes", info.batteryLifeTime));
    ss << createTableRow("Battery Health",
                         std::format("{:.1f}%", info.getBatteryHealth()));
    ss << createTableRow("Temperature",
                         std::format("{:.1f}°C", info.temperature));
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatMemoryInfo(const MemoryInfo& info)
    -> std::string {
    std::stringstream ss;
    ss << createTableHeader("Memory Information");
    ss << createTableRow("Total Physical Memory",
                         std::format("{:.2f} GB", info.totalPhysicalMemory /
                                                      (1024.0 * 1024 * 1024)));
    ss << createTableRow("Available Physical Memory",
                         std::format("{:.2f} GB", info.availablePhysicalMemory /
                                                      (1024.0 * 1024 * 1024)));
    ss << createTableRow("Memory Usage",
                         std::format("{:.1f}%", info.memoryLoadPercentage));
    ss << createTableRow("Total Virtual Memory",
                         std::format("{:.2f} GB", info.virtualMemoryMax /
                                                      (1024.0 * 1024 * 1024)));
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatCpuInfo(const CpuInfo& info) -> std::string {
    std::stringstream ss;
    ss << createTableHeader("CPU Information");
    ss << createTableRow("Model", info.model);
    ss << createTableRow("Vendor", cpuVendorToString(info.vendor));
    ss << createTableRow("Architecture",
                         cpuArchitectureToString(info.architecture));
    ss << createTableRow("Physical Cores",
                         std::to_string(info.numPhysicalCores));
    ss << createTableRow("Logical Cores", std::to_string(info.numLogicalCores));
    ss << createTableRow("Base Frequency",
                         std::format("{:.2f} GHz", info.baseFrequency));
    ss << createTableRow("Current Temperature",
                         std::format("{:.1f}°C", info.temperature));
    ss << createTableRow("Current Usage", std::format("{:.1f}%", info.usage));
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatBiosInfo(const BiosInfoData& info) -> std::string {
    std::stringstream ss;
    ss << createTableHeader("BIOS Information");
    ss << createTableRow("Vendor", info.manufacturer);
    ss << createTableRow("Version", info.version);
    ss << createTableRow("Release Date", info.releaseDate);
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatDiskInfo(const std::vector<DiskInfo>& disks) -> std::string {
    std::stringstream ss;
    ss << createTableHeader("Disk Information");
    
    for (size_t i = 0; i < disks.size(); ++i) {
        const auto& disk = disks[i];
        ss << createTableRow("Disk " + std::to_string(i + 1) + " Model", disk.model);
        ss << createTableRow("Disk " + std::to_string(i + 1) + " Type", 
                            diskTypeToString(disk.fsType));
        ss << createTableRow("Disk " + std::to_string(i + 1) + " Size", 
                            std::format("{:.2f} GB", disk.totalSpace / (1024.0 * 1024 * 1024)));
        ss << createTableRow("Disk " + std::to_string(i + 1) + " Free Space", 
                            std::format("{:.2f} GB", disk.freeSpace / (1024.0 * 1024 * 1024)));
    }
    
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatLocaleInfo(const LocaleInfo& info) -> std::string {
    std::stringstream ss;
    ss << createTableHeader("Locale Information");
    ss << createTableRow("Language", info.languageDisplayName + " (" + info.languageCode + ")");
    ss << createTableRow("Country", info.countryDisplayName + " (" + info.countryCode + ")");
    ss << createTableRow("Encoding", info.characterEncoding);
    ss << createTableRow("Time Format", info.timeFormat);
    ss << createTableRow("Date Format", info.dateFormat);
    ss << createTableFooter();
    return ss.str();
}

auto SystemInfoPrinter::formatOsInfo(const OperatingSystemInfo& info) -> std::string {
    std::stringstream ss;
    ss << createTableHeader("Operating System Information");
    ss << createTableRow("OS Name", info.osName);
    ss << createTableRow("OS Version", info.osVersion);
    ss << createTableRow("Kernel Version", info.kernelVersion);
    ss << createTableRow("Architecture", info.architecture);
    ss << createTableRow("Computer Name", info.computerName);
    ss << createTableRow("Boot Time", info.bootTime);
    ss << createTableRow("Install Date", info.installDate);
    ss << createTableRow("Last Update", info.lastUpdate);
    ss << createTableRow("Time Zone", info.timeZone);
    ss << createTableRow("Character Set", info.charSet);
    ss << createTableRow("Is Server", info.isServer ? "Yes" : "No");
    ss << createTableFooter();
    return ss.str();
}

/* GPU 信息格式化功能未实现，相关接口和结构体缺失，已注释处理 */
// auto SystemInfoPrinter::formatGpuInfo() -> std::string {
//     std::stringstream ss;
//     ss << createTableHeader("GPU Information");
//     ss << createTableRow("Error", "GPU information not implemented.");
//     ss << createTableFooter();
//     return ss.str();
// }

auto SystemInfoPrinter::formatSystemInfo(const SystemInfo& info) -> std::string {
    std::stringstream ss;
    ss << "=== System Desktop/WM Information ===\n\n";
    ss << createTableHeader("Desktop/WM Information");
    ss << createTableRow("Desktop Environment", info.desktopEnvironment);
    ss << createTableRow("Window Manager", info.windowManager);
    ss << createTableRow("WM Theme", info.wmTheme);
    ss << createTableRow("Icons", info.icons);
    ss << createTableRow("Font", info.font);
    ss << createTableRow("Cursor", info.cursor);
    ss << createTableFooter();
    return ss.str();
}

// Generate comprehensive reports
auto SystemInfoPrinter::generateFullReport() -> std::string {
    spdlog::info("Generating full system report");
    std::stringstream ss;

    ss << "=== Complete System Information Report ===\n";
    ss << "Generated at: "
       << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now())
       << "\n\n";

    try {
        // Operating system information
        auto osInfo = getOperatingSystemInfo();
        ss << formatOsInfo(osInfo);

        // CPU information
        auto cpuInfo = getCpuInfo();
        ss << formatCpuInfo(cpuInfo);

        // Memory information
        auto memInfo = getDetailedMemoryStats();
        ss << formatMemoryInfo(memInfo);

        // Battery information
        auto batteryResult = getDetailedBatteryInfo();
        if (std::holds_alternative<BatteryInfo>(batteryResult)) {
            const auto& batteryInfo = std::get<BatteryInfo>(batteryResult);
            ss << formatBatteryInfo(batteryInfo);
        } else {
            ss << createTableHeader("Battery Information");
            ss << createTableRow("Error", "Battery information unavailable or error occurred.");
            ss << createTableFooter();
        }

        // Network information
        auto netStats = getNetworkStats();
        ss << createTableHeader("Network Information");
        ss << createTableRow(
            "Download Speed",
            std::format("{:.2f} MB/s", netStats.downloadSpeed));
        ss << createTableRow("Upload Speed",
                             std::format("{:.2f} MB/s", netStats.uploadSpeed));
        ss << createTableRow("Latency",
                             std::format("{:.1f} ms", netStats.latency));
        ss << createTableRow(
            "Signal Strength",
            std::format("{:.1f} dBm", netStats.signalStrength));
        ss << createTableFooter();

    } catch (const std::exception& e) {
        spdlog::error("Error generating report: {}", e.what());
        ss << "\nError generating some parts of the report: " << e.what()
           << "\n";
    }

    return ss.str();
}

auto SystemInfoPrinter::generateSimpleReport() -> std::string {
    spdlog::info("Generating simple system report");
    std::stringstream ss;

    ss << "=== System Summary ===\n";
    ss << "Generated at: "
       << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now())
       << "\n\n";

    try {
        auto osInfo = getOperatingSystemInfo();
        auto cpuInfo = getCpuInfo();
        auto memInfo = getDetailedMemoryStats();

        ss << "OS: " << osInfo.osName << " " << osInfo.osVersion << "\n";
        ss << "CPU: " << cpuInfo.model << " (" << cpuInfo.numPhysicalCores 
           << " cores, " << cpuInfo.numLogicalCores << " threads)\n";
        ss << "Memory: " << std::format("{:.2f} GB / {:.2f} GB ({:.1f}% used)\n", 
                                      (memInfo.totalPhysicalMemory - memInfo.availablePhysicalMemory) / (1024.0 * 1024 * 1024),
                                      memInfo.totalPhysicalMemory / (1024.0 * 1024 * 1024),
                                      memInfo.memoryLoadPercentage);
        
        auto batteryResult = getDetailedBatteryInfo();
        if (std::holds_alternative<BatteryInfo>(batteryResult)) {
            const auto& batteryInfo = std::get<BatteryInfo>(batteryResult);
            if (batteryInfo.isBatteryPresent) {
                ss << "Battery: " << batteryInfo.batteryLifePercent << "% "
                   << (batteryInfo.isCharging ? "(Charging)" : "(Discharging)") << "\n";
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error generating simple report: {}", e.what());
        ss << "Error generating report: " << e.what() << "\n";
    }

    return ss.str();
}

auto SystemInfoPrinter::generatePerformanceReport() -> std::string {
    spdlog::info("Generating performance report");
    std::stringstream ss;

    ss << "=== System Performance Report ===\n";
    ss << "Generated at: "
       << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now())
       << "\n\n";

    try {
        auto cpuInfo = getCpuInfo();
        ss << createTableHeader("CPU Performance");
        ss << createTableRow("Model", cpuInfo.model);
        ss << createTableRow("Base Frequency", std::format("{:.2f} GHz", cpuInfo.baseFrequency));
        ss << createTableRow("Current Usage", std::format("{:.1f}%", cpuInfo.usage));
        ss << createTableRow("Temperature", std::format("{:.1f}°C", cpuInfo.temperature));
        ss << createTableFooter();

        auto memInfo = getDetailedMemoryStats();
        ss << createTableHeader("Memory Performance");
        ss << createTableRow("Total RAM", std::format("{:.2f} GB", memInfo.totalPhysicalMemory / (1024.0 * 1024 * 1024)));
        ss << createTableRow("Available RAM", std::format("{:.2f} GB", memInfo.availablePhysicalMemory / (1024.0 * 1024 * 1024)));
        ss << createTableRow("Memory Usage", std::format("{:.1f}%", memInfo.memoryLoadPercentage));
        ss << createTableFooter();

        auto netStats = getNetworkStats();
        ss << createTableHeader("Network Performance");
        ss << createTableRow("Download Speed", std::format("{:.2f} MB/s", netStats.downloadSpeed));
        ss << createTableRow("Upload Speed", std::format("{:.2f} MB/s", netStats.uploadSpeed));
        ss << createTableRow("Latency", std::format("{:.1f} ms", netStats.latency));
        ss << createTableFooter();

        auto disks = getDiskInfo();
        ss << createTableHeader("Disk Performance");
        for (size_t i = 0; i < disks.size(); ++i) {
            const auto& disk = disks[i];
            // DiskInfo结构体无readSpeed/writeSpeed成员，以下两行已注释或移除
            // ss << createTableRow("Disk " + std::to_string(i + 1) + " Read Speed",
            //                     std::format("{:.1f} MB/s", disk.readSpeed));
            // ss << createTableRow("Disk " + std::to_string(i + 1) + " Write Speed",
            //                     std::format("{:.1f} MB/s", disk.writeSpeed));
        }
        ss << createTableFooter();

    } catch (const std::exception& e) {
        spdlog::error("Error generating performance report: {}", e.what());
        ss << "\nError generating performance report: " << e.what() << "\n";
    }

    return ss.str();
}

auto SystemInfoPrinter::generateSecurityReport() -> std::string {
    spdlog::info("Generating security report");
    std::stringstream ss;

    ss << "=== System Security Report ===\n";
    ss << "Generated at: "
       << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now())
       << "\n\n";

    try {
        auto osInfo = getOperatingSystemInfo();
        ss << createTableHeader("OS Security");
        ss << createTableRow("Operating System", osInfo.osName + " " + osInfo.osVersion);
        ss << createTableRow("Kernel Version", osInfo.kernelVersion);
        ss << createTableRow("Computer Name", osInfo.computerName);
        ss << createTableRow("Boot Time", osInfo.bootTime);
        ss << createTableRow("Install Date", osInfo.installDate);
        ss << createTableRow("Last Update", osInfo.lastUpdate);
        ss << createTableRow("Time Zone", osInfo.timeZone);
        ss << createTableRow("Character Set", osInfo.charSet);
        ss << createTableRow("Is Server", osInfo.isServer ? "Yes" : "No");
        ss << createTableFooter();

        #include "bios.hpp"
        auto& bios = BiosInfo::getInstance();
        const auto& biosInfo = bios.getBiosInfo();
        ss << createTableHeader("Firmware Security");
        ss << createTableRow("BIOS Vendor", biosInfo.manufacturer);
        ss << createTableRow("BIOS Version", biosInfo.version);
        ss << createTableFooter();

    } catch (const std::exception& e) {
        spdlog::error("Error generating security report: {}", e.what());
        ss << "\nError generating security report: " << e.what() << "\n";
    }

    return ss.str();
}

bool SystemInfoPrinter::exportToHTML(const std::string& filename) {
    spdlog::info("Exporting system information to HTML: {}", filename);
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filename);
            return false;
        }

        std::string report = generateFullReport();
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>System Information Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; margin-bottom: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
        h2 { margin-top: 20px; color: #333; }
    </style>
</head>
<body>
<h1>System Information Report</h1>
<p>Generated at: )" + 
    std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now()) + 
R"(</p>
)";

        // Convert ASCII tables to HTML tables
        std::string currentLine;
        std::istringstream reportStream(report);
        bool inTable = false;
        
        while (std::getline(reportStream, currentLine)) {
            if (currentLine.find("===") != std::string::npos) {
                html += "<h2>" + currentLine + "</h2>\n";
            }
            else if (currentLine.find("|--") != std::string::npos) {
                // Table border line, ignore
                if (!inTable) {
                    html += "<table>\n<tr><th>Parameter</th><th>Value</th></tr>\n";
                    inTable = true;
                }
            }
            else if (currentLine.find("|") == 0) {
                // Table row
                size_t middlePipe = currentLine.find("|", 1);
                if (middlePipe != std::string::npos) {
                    std::string param = currentLine.substr(1, middlePipe - 1);
                    std::string value = currentLine.substr(middlePipe + 1);
                    
                    // Remove trailing pipe and trim
                    if (!value.empty() && value.back() == '|') {
                        value.pop_back();
                    }
                    
                    // Trim spaces
                    param.erase(0, param.find_first_not_of(" "));
                    param.erase(param.find_last_not_of(" ") + 1);
                    value.erase(0, value.find_first_not_of(" "));
                    value.erase(value.find_last_not_of(" ") + 1);
                    
                    html += "<tr><td>" + param + "</td><td>" + value + "</td></tr>\n";
                }
            }
            else if (inTable && currentLine.empty()) {
                html += "</table>\n";
                inTable = false;
            }
            else if (!currentLine.empty()) {
                html += "<p>" + currentLine + "</p>\n";
            }
        }
        
        if (inTable) {
            html += "</table>\n";
        }

        html += "</body></html>";
        file << html;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error exporting to HTML: {}", e.what());
        return false;
    }
}

bool SystemInfoPrinter::exportToJSON(const std::string& filename) {
    spdlog::info("Exporting system information to JSON: {}", filename);
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filename);
            return false;
        }

        // Create a JSON structure with system information
        file << "{\n";
        file << "  \"timestamp\": \"" << 
            std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now()) << "\",\n";
        
        // OS information
        try {
            auto osInfo = getOperatingSystemInfo();
            file << "  \"os\": {\n";
            file << "    \"osName\": \"" << osInfo.osName << "\",\n";
            file << "    \"osVersion\": \"" << osInfo.osVersion << "\",\n";
            file << "    \"kernelVersion\": \"" << osInfo.kernelVersion << "\",\n";
            file << "    \"architecture\": \"" << osInfo.architecture << "\",\n";
            file << "    \"computerName\": \"" << osInfo.computerName << "\",\n";
            file << "    \"bootTime\": \"" << osInfo.bootTime << "\",\n";
            file << "    \"installDate\": \"" << osInfo.installDate << "\",\n";
            file << "    \"lastUpdate\": \"" << osInfo.lastUpdate << "\",\n";
            file << "    \"timeZone\": \"" << osInfo.timeZone << "\",\n";
            file << "    \"charSet\": \"" << osInfo.charSet << "\",\n";
            file << "    \"isServer\": " << (osInfo.isServer ? "true" : "false") << "\n";
            file << "  },\n";
        } catch (const std::exception& e) {
            spdlog::error("Error getting OS info for JSON export: {}", e.what());
            file << "  \"os\": { \"error\": \"" << e.what() << "\" },\n";
        }
        
        // CPU information
        try {
            auto cpuInfo = getCpuInfo();
            file << "  \"cpu\": {\n";
            file << "    \"model\": \"" << cpuInfo.model << "\",\n";
            file << "    \"vendor\": \"" << cpuVendorToString(cpuInfo.vendor) << "\",\n";
            file << "    \"architecture\": \"" << cpuArchitectureToString(cpuInfo.architecture) << "\",\n";
            file << "    \"physical_cores\": " << cpuInfo.numPhysicalCores << ",\n";
            file << "    \"logical_cores\": " << cpuInfo.numLogicalCores << ",\n";
            file << "    \"base_frequency_ghz\": " << cpuInfo.baseFrequency << ",\n";
            file << "    \"temperature_celsius\": " << cpuInfo.temperature << ",\n";
            file << "    \"usage_percent\": " << cpuInfo.usage << "\n";
            file << "  },\n";
        } catch (const std::exception& e) {
            spdlog::error("Error getting CPU info for JSON export: {}", e.what());
            file << "  \"cpu\": { \"error\": \"" << e.what() << "\" },\n";
        }
        
        // Memory information
        try {
            auto memInfo = getDetailedMemoryStats();
            file << "  \"memory\": {\n";
            file << "    \"total_physical_bytes\": " << memInfo.totalPhysicalMemory << ",\n";
            file << "    \"available_physical_bytes\": " << memInfo.availablePhysicalMemory << ",\n";
            file << "    \"memory_load_percent\": " << memInfo.memoryLoadPercentage << ",\n";
            file << "    \"virtual_memory_max_bytes\": " << memInfo.virtualMemoryMax << "\n";
            file << "  }\n";
        } catch (const std::exception& e) {
            spdlog::error("Error getting memory info for JSON export: {}", e.what());
            file << "  \"memory\": { \"error\": \"" << e.what() << "\" }\n";
        }
        
        file << "}\n";
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error exporting to JSON: {}", e.what());
        return false;
    }
}

bool SystemInfoPrinter::exportToMarkdown(const std::string& filename) {
    spdlog::info("Exporting system information to Markdown: {}", filename);
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for writing: {}", filename);
            return false;
        }

        file << "# System Information Report\n\n";
        file << "Generated at: " << 
            std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now()) << "\n\n";

        // Operating system information
        try {
            auto osInfo = getOperatingSystemInfo();
            file << "## Operating System Information\n\n";
            file << "| Parameter | Value |\n";
            file << "|-----------|-------|\n";
            file << "| OS Name | " << osInfo.osName << " |\n";
            file << "| OS Version | " << osInfo.osVersion << " |\n";
            file << "| Kernel Version | " << osInfo.kernelVersion << " |\n";
            file << "| Architecture | " << osInfo.architecture << " |\n";
            file << "| Computer Name | " << osInfo.computerName << " |\n";
            file << "| Boot Time | " << osInfo.bootTime << " |\n";
            file << "| Install Date | " << osInfo.installDate << " |\n";
            file << "| Last Update | " << osInfo.lastUpdate << " |\n";
            file << "| Time Zone | " << osInfo.timeZone << " |\n";
            file << "| Character Set | " << osInfo.charSet << " |\n";
            file << "| Is Server | " << (osInfo.isServer ? "Yes" : "No") << " |\n\n";
        } catch (const std::exception& e) {
            spdlog::error("Error getting OS info for Markdown export: {}", e.what());
            file << "Error retrieving operating system information: " << e.what() << "\n\n";
        }
        
        // Add additional sections for CPU, memory, etc.
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Error exporting to Markdown: {}", e.what());
        return false;
    }
}

}  // namespace atom::system