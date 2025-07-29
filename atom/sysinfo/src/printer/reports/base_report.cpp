#include "base_report.hpp"
#include "../formatters/cpu_formatter.hpp"
#include "../formatters/memory_formatter.hpp"
#include "../formatters/battery_formatter.hpp"
#include "../formatters/os_formatter.hpp"
#include "../formatters/bios_formatter.hpp"
#include "../formatters/disk_formatter.hpp"
#include "../exporters/html_exporter.hpp"
#include "../exporters/json_exporter.hpp"
#include "../../battery/battery.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace atom::system {

void BaseReport::setOptions(const ReportOptions& options) {
    options_ = options;
    if (options_.sections.empty()) {
        initializeDefaultSections();
    }
}

auto BaseReport::getOptions() const -> const ReportOptions& {
    return options_;
}

void BaseReport::addSection(ReportSection section) {
    if (!hasSection(section)) {
        options_.sections.push_back(section);
    }
}

void BaseReport::removeSection(ReportSection section) {
    auto it = std::find(options_.sections.begin(), options_.sections.end(), section);
    if (it != options_.sections.end()) {
        options_.sections.erase(it);
    }
}

bool BaseReport::hasSection(ReportSection section) const {
    return std::find(options_.sections.begin(), options_.sections.end(), section) != options_.sections.end();
}

void BaseReport::clearSections() {
    options_.sections.clear();
}

void BaseReport::setTitle(const std::string& title) {
    options_.title = title;
}

auto BaseReport::getTitle() const -> std::string {
    return options_.title;
}

void BaseReport::setCustomOption(const std::string& key, const std::string& value) {
    options_.customOptions[key] = value;
}

auto BaseReport::getCustomOption(const std::string& key, const std::string& defaultValue) const -> std::string {
    auto it = options_.customOptions.find(key);
    return (it != options_.customOptions.end()) ? it->second : defaultValue;
}

bool BaseReport::exportToFile(const std::string& filename, ExportFormat format) {
    try {
        auto content = generate();
        auto exporter = getExporter(format);
        if (!exporter) {
            spdlog::error("Failed to create exporter for format");
            return false;
        }

        return exporter->exportToFile(content, filename);
    } catch (const std::exception& e) {
        spdlog::error("Error exporting report to file {}: {}", filename, e.what());
        return false;
    }
}

auto BaseReport::generateSection(ReportSection section) -> std::string {
    try {
        auto formatter = getFormatter(section);
        if (!formatter) {
            spdlog::warn("No formatter available for section: {}", sectionToString(section));
            return "";
        }

        switch (section) {
            case ReportSection::OS: {
                auto osInfo = getOperatingSystemInfo();
                return std::static_pointer_cast<OsFormatter>(formatter)->format(osInfo);
            }
            case ReportSection::CPU: {
                auto cpuInfo = getCpuInfo();
                return std::static_pointer_cast<CpuFormatter>(formatter)->format(cpuInfo);
            }
            case ReportSection::MEMORY: {
                auto memInfo = getDetailedMemoryStats();
                return std::static_pointer_cast<MemoryFormatter>(formatter)->format(memInfo);
            }
            case ReportSection::BATTERY: {
                auto batteryResult = battery::getDetailedBatteryInfo();
                if (std::holds_alternative<battery::BatteryInfo>(batteryResult)) {
                    const auto& batteryInfo = std::get<battery::BatteryInfo>(batteryResult);
                    return std::static_pointer_cast<BatteryFormatter>(formatter)->format(batteryInfo);
                }
                return "Battery information not available\n";
            }
            case ReportSection::BIOS: {
                auto& bios = BiosInfo::getInstance();
                const auto& biosInfo = bios.getBiosInfo();
                return std::static_pointer_cast<BiosFormatter>(formatter)->format(biosInfo);
            }
            case ReportSection::DISK: {
                auto disks = getDiskInfo();
                return std::static_pointer_cast<DiskFormatter>(formatter)->format(disks);
            }
            default:
                return "Section not implemented: " + sectionToString(section) + "\n";
        }
    } catch (const std::exception& e) {
        spdlog::error("Error generating section {}: {}", sectionToString(section), e.what());
        return "Error generating " + sectionToString(section) + " information\n";
    }
}

auto BaseReport::getFormatter(ReportSection section) -> std::shared_ptr<BaseFormatter> {
    auto formatterOptions = createFormatterOptions();

    switch (section) {
        case ReportSection::OS:
            return std::make_shared<OsFormatter>(formatterOptions);
        case ReportSection::CPU:
            return std::make_shared<CpuFormatter>(formatterOptions);
        case ReportSection::MEMORY:
            return std::make_shared<MemoryFormatter>(formatterOptions);
        case ReportSection::BATTERY:
            return std::make_shared<BatteryFormatter>(formatterOptions);
        case ReportSection::BIOS:
            return std::make_shared<BiosFormatter>(formatterOptions);
        case ReportSection::DISK:
            return std::make_shared<DiskFormatter>(formatterOptions);
        default:
            return nullptr;
    }
}

auto BaseReport::getExporter(ExportFormat format) -> std::shared_ptr<BaseExporter> {
    auto exporterOptions = createExporterOptions();

    switch (format) {
        case ExportFormat::HTML:
            return std::make_shared<HtmlExporter>(exporterOptions);
        case ExportFormat::JSON:
            return std::make_shared<JsonExporter>(exporterOptions);
        default:
            return nullptr;
    }
}

auto BaseReport::createHeader() -> std::string {
    if (options_.title.empty()) {
        return "";
    }
    return "=== " + options_.title + " ===\n\n";
}

auto BaseReport::createFooter() -> std::string {
    return "\n=== End of Report ===\n";
}

auto BaseReport::combineSections(const std::unordered_map<ReportSection, std::string>& sections) -> std::string {
    std::string result = createHeader();

    for (const auto& section : options_.sections) {
        auto it = sections.find(section);
        if (it != sections.end() && !it->second.empty()) {
            result += it->second;
        }
    }

    result += createFooter();
    return result;
}

auto BaseReport::getDefaultSections() const -> std::vector<ReportSection> {
    return {ReportSection::OS, ReportSection::CPU, ReportSection::MEMORY,
            ReportSection::DISK, ReportSection::BATTERY, ReportSection::BIOS};
}

auto BaseReport::sectionToString(ReportSection section) const -> std::string {
    switch (section) {
        case ReportSection::OS: return "Operating System";
        case ReportSection::CPU: return "CPU";
        case ReportSection::MEMORY: return "Memory";
        case ReportSection::DISK: return "Disk";
        case ReportSection::BATTERY: return "Battery";
        case ReportSection::NETWORK: return "Network";
        case ReportSection::BIOS: return "BIOS";
        case ReportSection::GPU: return "GPU";
        case ReportSection::LOCALE: return "Locale";
        case ReportSection::SYSTEM: return "System";
        default: return "Unknown";
    }
}

auto BaseReport::applyTemplate(const std::string& content) -> std::string {
    if (options_.templatePath.empty()) {
        return content;
    }

    // Template application would be implemented here
    return content;
}

void BaseReport::initializeDefaultSections() {
    options_.sections = getDefaultSections();
}

auto BaseReport::createFormatterOptions() const -> FormatterOptions {
    FormatterOptions formatterOptions;
    formatterOptions.style = options_.style;
    formatterOptions.format = options_.format;
    formatterOptions.colorEnabled = options_.colorEnabled;
    formatterOptions.timestampEnabled = options_.includeTimestamp;
    return formatterOptions;
}

auto BaseReport::createExporterOptions() const -> ExportOptions {
    ExportOptions exporterOptions;
    exporterOptions.includeTimestamp = options_.includeTimestamp;
    exporterOptions.includeMetadata = options_.includeMetadata;
    exporterOptions.title = options_.title;
    exporterOptions.templatePath = options_.templatePath;
    return exporterOptions;
}

} // namespace atom::system
