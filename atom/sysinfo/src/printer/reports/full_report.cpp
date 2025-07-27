#include "full_report.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <format>

namespace atom::system {

FullReport::FullReport() {
    options_.type = ReportType::FULL;
    options_.title = "Complete System Information Report";
    initializeFullReport();
}

FullReport::FullReport(const ReportOptions& options) {
    setOptions(options);
    options_.type = ReportType::FULL;
    if (options_.title.empty()) {
        options_.title = "Complete System Information Report";
    }
    initializeFullReport();
}

auto FullReport::generate() -> std::string {
    spdlog::info("Generating full system report");

    std::unordered_map<ReportSection, std::string> sectionContent;

    // Generate content for each section
    for (const auto& section : options_.sections) {
        try {
            auto content = generateSection(section);
            if (!content.empty()) {
                sectionContent[section] = content;
            }
        } catch (const std::exception& e) {
            spdlog::error("Error generating section {}: {}", sectionToString(section), e.what());
            sectionContent[section] = "Error generating " + sectionToString(section) + " information\n";
        }
    }

    // Add timestamp to header if enabled
    std::string header = createHeader();
    if (options_.includeTimestamp) {
        auto now = std::chrono::system_clock::now();
        header += std::format("Generated at: {:%Y-%m-%d %H:%M:%S}\n\n", now);
    }

    // Combine all sections
    std::string result = header;

    for (const auto& section : options_.sections) {
        auto it = sectionContent.find(section);
        if (it != sectionContent.end() && !it->second.empty()) {
            result += it->second;
        }
    }

    result += createFooter();

    spdlog::info("Full system report generated successfully");
    return result;
}

auto FullReport::getDefaultSections() const -> std::vector<ReportSection> {
    return {
        ReportSection::OS,
        ReportSection::CPU,
        ReportSection::MEMORY,
        ReportSection::DISK,
        ReportSection::BATTERY,
        ReportSection::NETWORK,
        ReportSection::BIOS,
        ReportSection::GPU,
        ReportSection::LOCALE,
        ReportSection::SYSTEM
    };
}

void FullReport::initializeFullReport() {
    if (options_.sections.empty()) {
        options_.sections = getDefaultSections();
    }
}

} // namespace atom::system
