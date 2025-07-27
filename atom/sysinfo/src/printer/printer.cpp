#include "printer.hpp"
#include "formatters/battery_formatter.hpp"
#include "formatters/bios_formatter.hpp"
#include "formatters/cpu_formatter.hpp"
#include "formatters/disk_formatter.hpp"
#include "formatters/memory_formatter.hpp"
#include "formatters/os_formatter.hpp"
#include "exporters/html_exporter.hpp"
#include "exporters/json_exporter.hpp"
#include "reports/full_report.hpp"
#include <spdlog/spdlog.h>

namespace atom::system {

// Static member initialization
std::unique_ptr<SystemInfoPrinter> SystemInfoPrinter::legacyInstance_ = nullptr;

SystemInfoPrinter::SystemInfoPrinter() {
    // Initialize default options
    defaultFormatterOptions_.style = FormatterStyle::STANDARD;
    defaultFormatterOptions_.format = OutputFormat::TABLE;
    defaultFormatterOptions_.colorEnabled = false;
    defaultFormatterOptions_.timestampEnabled = true;
    
    defaultExportOptions_.format = ExportFormat::HTML;
    defaultExportOptions_.includeTimestamp = true;
    defaultExportOptions_.includeMetadata = true;
    defaultExportOptions_.title = "System Information Report";
}

SystemInfoPrinter::SystemInfoPrinter(const FormatterOptions& options) 
    : defaultFormatterOptions_(options) {
    defaultExportOptions_.format = ExportFormat::HTML;
    defaultExportOptions_.includeTimestamp = true;
    defaultExportOptions_.includeMetadata = true;
    defaultExportOptions_.title = "System Information Report";
}

void SystemInfoPrinter::setFormatterOptions(const FormatterOptions& options) {
    defaultFormatterOptions_ = options;
}

auto SystemInfoPrinter::getFormatterOptions() const -> const FormatterOptions& {
    return defaultFormatterOptions_;
}

void SystemInfoPrinter::setExportOptions(const ExportOptions& options) {
    defaultExportOptions_ = options;
}

auto SystemInfoPrinter::getExportOptions() const -> const ExportOptions& {
    return defaultExportOptions_;
}

auto SystemInfoPrinter::generateReport(ReportType type, const ReportOptions& options) -> std::string {
    ReportOptions effectiveOptions = options;
    if (effectiveOptions.style == FormatterStyle::STANDARD) {
        effectiveOptions.style = defaultFormatterOptions_.style;
    }
    if (effectiveOptions.format == OutputFormat::TABLE) {
        effectiveOptions.format = defaultFormatterOptions_.format;
    }
    
    switch (type) {
        case ReportType::FULL: {
            auto report = createReport<FullReport>(effectiveOptions);
            return report->generate();
        }
        default:
            spdlog::warn("Report type not implemented, falling back to full report");
            auto report = createReport<FullReport>(effectiveOptions);
            return report->generate();
    }
}

bool SystemInfoPrinter::exportReport(const std::string& content, const std::string& filename, ExportFormat format) {
    switch (format) {
        case ExportFormat::HTML: {
            auto exporter = createExporter<HtmlExporter>(defaultExportOptions_);
            return exporter->exportToFile(content, filename);
        }
        case ExportFormat::JSON: {
            auto exporter = createExporter<JsonExporter>(defaultExportOptions_);
            return exporter->exportToFile(content, filename);
        }
        default:
            spdlog::error("Export format not supported");
            return false;
    }
}

// ========== Legacy API Implementation ==========

auto SystemInfoPrinter::getLegacyInstance() -> SystemInfoPrinter& {
    if (!legacyInstance_) {
        legacyInstance_ = std::make_unique<SystemInfoPrinter>(createLegacyFormatterOptions());
    }
    return *legacyInstance_;
}

auto SystemInfoPrinter::createLegacyFormatterOptions() -> FormatterOptions {
    FormatterOptions options;
    options.style = FormatterStyle::STANDARD;
    options.format = OutputFormat::TABLE;
    options.colorEnabled = false;
    options.timestampEnabled = false;
    options.tableWidth = 80;
    return options;
}

auto SystemInfoPrinter::createLegacyExportOptions() -> ExportOptions {
    ExportOptions options;
    options.format = ExportFormat::HTML;
    options.includeTimestamp = true;
    options.includeMetadata = true;
    options.title = "System Information Report";
    return options;
}

auto SystemInfoPrinter::formatBatteryInfo(const BatteryInfo& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<BatteryFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatBiosInfo(const BiosInfoData& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<BiosFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatCpuInfo(const CpuInfo& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<CpuFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatDiskInfo(const std::vector<DiskInfo>& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<DiskFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatGpuInfo() -> std::string {
    // GPU formatter not implemented yet
    return "GPU information not available\n";
}

auto SystemInfoPrinter::formatLocaleInfo(const LocaleInfo& info) -> std::string {
    // Locale formatter not implemented yet
    return "Locale information not available\n";
}

auto SystemInfoPrinter::formatMemoryInfo(const MemoryInfo& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<MemoryFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatOsInfo(const OperatingSystemInfo& info) -> std::string {
    auto& instance = getLegacyInstance();
    auto formatter = instance.createFormatter<OsFormatter>();
    return formatter->format(info);
}

auto SystemInfoPrinter::formatSystemInfo(const SystemInfo& info) -> std::string {
    // System formatter not implemented yet
    return "System information not available\n";
}

auto SystemInfoPrinter::generateFullReport() -> std::string {
    auto& instance = getLegacyInstance();
    return instance.generateReport(ReportType::FULL);
}

auto SystemInfoPrinter::generateSimpleReport() -> std::string {
    auto& instance = getLegacyInstance();
    return instance.generateReport(ReportType::SIMPLE);
}

auto SystemInfoPrinter::generatePerformanceReport() -> std::string {
    auto& instance = getLegacyInstance();
    return instance.generateReport(ReportType::PERFORMANCE);
}

auto SystemInfoPrinter::generateSecurityReport() -> std::string {
    auto& instance = getLegacyInstance();
    return instance.generateReport(ReportType::SECURITY);
}

bool SystemInfoPrinter::exportToHTML(const std::string& filename) {
    auto& instance = getLegacyInstance();
    auto content = instance.generateReport(ReportType::FULL);
    return instance.exportReport(content, filename, ExportFormat::HTML);
}

bool SystemInfoPrinter::exportToJSON(const std::string& filename) {
    auto& instance = getLegacyInstance();
    auto content = instance.generateReport(ReportType::FULL);
    return instance.exportReport(content, filename, ExportFormat::JSON);
}

bool SystemInfoPrinter::exportToMarkdown(const std::string& filename) {
    auto& instance = getLegacyInstance();
    auto content = instance.generateReport(ReportType::FULL);
    return instance.exportReport(content, filename, ExportFormat::MARKDOWN);
}

} // namespace atom::system
