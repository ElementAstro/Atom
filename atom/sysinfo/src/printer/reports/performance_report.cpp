#include "performance_report.hpp"
#include "../../cpu.hpp"
#include "../../memory.hpp"
#include "../../disk.hpp"
#include <spdlog/spdlog.h>
#include <sstream>

namespace atom::system {

PerformanceReport::PerformanceReport() {
    options_.type = ReportType::PERFORMANCE;
    options_.title = "System Performance Report";
    options_.style = FormatterStyle::DETAILED;
    initializePerformanceReport();
}

PerformanceReport::PerformanceReport(const ReportOptions& options) {
    setOptions(options);
    options_.type = ReportType::PERFORMANCE;
    if (options_.title.empty()) {
        options_.title = "System Performance Report";
    }
    if (options_.style == FormatterStyle::STANDARD) {
        options_.style = FormatterStyle::DETAILED;
    }
    initializePerformanceReport();
}

auto PerformanceReport::generate() -> std::string {
    spdlog::info("Generating performance system report");

    std::unordered_map<ReportSection, std::string> sectionContent;

    // Generate content for each section with performance focus
    for (const auto& section : options_.sections) {
        try {
            auto formatter = getFormatter(section);
            if (formatter) {
                formatter->setStyle(FormatterStyle::DETAILED);
                auto content = generateSection(section);
                if (!content.empty()) {
                    sectionContent[section] = content;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error generating section {}: {}", sectionToString(section), e.what());
        }
    }

    // Create performance-focused header
    std::string result = createHeader();

    // Add performance metrics summary
    result += generatePerformanceMetrics();

    // Add detailed component information
    for (const auto& section : options_.sections) {
        auto it = sectionContent.find(section);
        if (it != sectionContent.end() && !it->second.empty()) {
            result += it->second;
        }
    }

    result += createFooter();

    spdlog::info("Performance system report generated successfully");
    return result;
}

auto PerformanceReport::getDefaultSections() const -> std::vector<ReportSection> {
    return {
        ReportSection::CPU,
        ReportSection::MEMORY,
        ReportSection::DISK,
        ReportSection::GPU,
        ReportSection::NETWORK
    };
}

void PerformanceReport::initializePerformanceReport() {
    if (options_.sections.empty()) {
        options_.sections = getDefaultSections();
    }
}

auto PerformanceReport::generatePerformanceMetrics() -> std::string {
    std::stringstream ss;

    ss << "\n=== Performance Summary ===\n";

    try {
        // CPU performance
        auto cpuInfo = getCpuInfo();
        if (cpuInfo.usage >= 0) {
            ss << "CPU Usage: " << cpuInfo.usage << "%\n";
        }
        if (cpuInfo.temperature > 0) {
            ss << "CPU Temperature: " << cpuInfo.temperature << "°C\n";
        }

        // Memory performance
        auto memInfo = getDetailedMemoryStats();
        ss << "Memory Usage: " << memInfo.memoryLoadPercentage << "%\n";
        ss << "Available Memory: " << (memInfo.availablePhysicalMemory / (1024*1024*1024)) << " GB\n";

        // Disk performance (basic)
        auto diskInfo = getDiskInfo();
        if (!diskInfo.empty()) {
            for (const auto& disk : diskInfo) {
                if (disk.totalSpace > 0) {
                    double usagePercent = ((disk.totalSpace - disk.freeSpace) * 100.0) / disk.totalSpace;
                    ss << "Disk Usage (" << disk.model << "): " << usagePercent << "%\n";
                }
            }
        }

    } catch (const std::exception& e) {
        ss << "Error gathering performance metrics: " << e.what() << "\n";
    }

    ss << "========================\n\n";

    return ss.str();
}

} // namespace atom::system
