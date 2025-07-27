#include "simple_report.hpp"
#include <spdlog/spdlog.h>

namespace atom::system {

SimpleReport::SimpleReport() {
    options_.type = ReportType::SIMPLE;
    options_.title = "System Overview";
    options_.style = FormatterStyle::COMPACT;
    initializeSimpleReport();
}

SimpleReport::SimpleReport(const ReportOptions& options) {
    setOptions(options);
    options_.type = ReportType::SIMPLE;
    if (options_.title.empty()) {
        options_.title = "System Overview";
    }
    if (options_.style == FormatterStyle::STANDARD) {
        options_.style = FormatterStyle::COMPACT;
    }
    initializeSimpleReport();
}

auto SimpleReport::generate() -> std::string {
    spdlog::info("Generating simple system report");

    std::unordered_map<ReportSection, std::string> sectionContent;

    // Generate content for each section using basic formatters
    for (const auto& section : options_.sections) {
        try {
            auto formatter = getFormatter(section);
            if (formatter) {
                formatter->setStyle(FormatterStyle::COMPACT);
                auto content = generateSection(section);
                if (!content.empty()) {
                    sectionContent[section] = content;
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error generating section {}: {}", sectionToString(section), e.what());
        }
    }

    // Create a simplified header
    std::string result = createHeader();

    // Add essential information in compact format
    for (const auto& section : options_.sections) {
        auto it = sectionContent.find(section);
        if (it != sectionContent.end() && !it->second.empty()) {
            result += it->second;
        }
    }

    result += createFooter();

    spdlog::info("Simple system report generated successfully");
    return result;
}

auto SimpleReport::getDefaultSections() const -> std::vector<ReportSection> {
    return {
        ReportSection::OS,
        ReportSection::CPU,
        ReportSection::MEMORY,
        ReportSection::DISK
    };
}

void SimpleReport::initializeSimpleReport() {
    if (options_.sections.empty()) {
        options_.sections = getDefaultSections();
    }
}

} // namespace atom::system