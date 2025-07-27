#include "custom_report.hpp"
#include <spdlog/spdlog.h>

namespace atom::system {

CustomReport::CustomReport() {
    options_.type = ReportType::CUSTOM;
    options_.title = "Custom System Report";
    initializeCustomReport();
}

CustomReport::CustomReport(const ReportOptions& options) {
    setOptions(options);
    options_.type = ReportType::CUSTOM;
    if (options_.title.empty()) {
        options_.title = "Custom System Report";
    }
    initializeCustomReport();
}

auto CustomReport::generate() -> std::string {
    spdlog::info("Generating custom system report");

    std::unordered_map<ReportSection, std::string> sectionContent;

    // Generate content for user-specified sections only
    for (const auto& section : options_.sections) {
        try {
            auto content = generateSection(section);
            if (!content.empty()) {
                sectionContent[section] = content;
            }
        } catch (const std::exception& e) {
            spdlog::error("Error generating section {}: {}", sectionToString(section), e.what());
        }
    }

    // Combine sections as specified by user
    auto result = combineSections(sectionContent);

    spdlog::info("Custom system report generated successfully");
    return result;
}

auto CustomReport::getDefaultSections() const -> std::vector<ReportSection> {
    // Custom reports have no default sections - user must specify
    return {};
}

void CustomReport::initializeCustomReport() {
    // Custom reports start with empty sections - user must add them
    if (options_.sections.empty()) {
        spdlog::warn("Custom report created with no sections specified");
    }
}

} // namespace atom::system