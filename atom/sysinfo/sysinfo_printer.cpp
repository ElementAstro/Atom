#include "sysinfo_printer.hpp"
#include <format>
#include <fstream>
#include <sstream>
#include "atom/log/loguru.hpp"
#include "wifi.hpp"

namespace atom::system {

// 格式化助手函数
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

// 格式化各个组件信息
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

// 生成综合报告
auto SystemInfoPrinter::generateFullReport() -> std::string {
    LOG_F(INFO, "Generating full system report");
    std::stringstream ss;

    ss << "=== Complete System Information Report ===\n";
    ss << "Generated at: "
       << std::format("{:%Y-%m-%d %H:%M:%S}", std::chrono::system_clock::now())
       << "\n\n";

    try {
        // 系统信息
        auto osInfo = getOperatingSystemInfo();
        ss << formatOsInfo(osInfo);

        // CPU信息
        auto cpuInfo = getCpuInfo();
        ss << formatCpuInfo(cpuInfo);

        // 内存信息
        auto memInfo = getDetailedMemoryStats();
        ss << formatMemoryInfo(memInfo);

        // 电池信息
        auto batteryInfo = getDetailedBatteryInfo();
        ss << formatBatteryInfo(batteryInfo);

        // 网络信息
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
        LOG_F(ERROR, "Error generating report: {}", e.what());
        ss << "\nError generating some parts of the report: " << e.what()
           << "\n";
    }

    return ss.str();
}

// 导出功能实现
bool SystemInfoPrinter::exportToHTML(const std::string& filename) {
    LOG_F(INFO, "Exporting system information to HTML: {}", filename);
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            LOG_F(ERROR, "Failed to open file for writing: {}", filename);
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
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
)";

        // 将report中的ASCII表格转换为HTML表格
        // 这里需要解析和转换ASCII表格格式
        html += "<pre>" + report + "</pre>";
        html += "</body></html>";

        file << html;
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error exporting to HTML: {}", e.what());
        return false;
    }
}

// 添加其他导出方法实现...
// exportToJSON
// exportToMarkdown
// generateSimpleReport
// generatePerformanceReport
// generateSecurityReport

}  // namespace atom::system
