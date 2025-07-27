# Atom System Information Printer - API Reference

## Overview

The Atom System Information Printer provides a comprehensive, modular API for gathering, formatting, and exporting system information. This document provides detailed API reference for all public interfaces.

## Core Classes

### SystemInfoPrinter

The main interface for generating system information reports.

```cpp
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

class SystemInfoPrinter {
public:
    SystemInfoPrinter();
    explicit SystemInfoPrinter(const FormatterOptions& options);
    
    // Report generation
    auto generateReport(ReportType type) -> std::string;
    auto generateReport(ReportType type, const ReportOptions& options) -> std::string;
    
    // Export functionality
    bool exportReport(const std::string& content, const std::string& filename, ExportFormat format);
    
    // Configuration
    void setFormatterOptions(const FormatterOptions& options);
    void setExportOptions(const ExportOptions& options);
    
    // Static legacy API (backward compatibility)
    static auto generateFullReport() -> std::string;
    static auto generateSimpleReport() -> std::string;
    static auto generatePerformanceReport() -> std::string;
    static auto generateSecurityReport() -> std::string;
    
    static bool exportToHTML(const std::string& filename);
    static bool exportToJSON(const std::string& filename);
    static bool exportToMarkdown(const std::string& filename);
};
```

#### Usage Examples

```cpp
// Basic usage
SystemInfoPrinter printer;
auto report = printer.generateReport(ReportType::FULL);
printer.exportReport(report, "system_report.html", ExportFormat::HTML);

// With custom options
FormatterOptions fmtOptions;
fmtOptions.style = FormatterStyle::DETAILED;
fmtOptions.colorEnabled = true;

SystemInfoPrinter printer(fmtOptions);
auto report = printer.generateReport(ReportType::PERFORMANCE);
```

## Enumerations

### ReportType

Defines the type of system information report to generate.

```cpp
enum class ReportType {
    FULL,           // Complete system information
    SIMPLE,         // Essential information only
    PERFORMANCE,    // Performance-focused metrics
    SECURITY,       // Security-related information
    HARDWARE,       // Hardware component details
    SOFTWARE,       // Software and configuration
    CUSTOM          // User-defined sections
};
```

### FormatterStyle

Controls the verbosity and detail level of formatted output.

```cpp
enum class FormatterStyle {
    MINIMAL,        // Minimal information
    COMPACT,        // Compact single-line format
    STANDARD,       // Standard table format
    DETAILED,       // Detailed with additional metrics
    VERBOSE         // Maximum detail with all available information
};
```

### ExportFormat

Supported export formats for system information reports.

```cpp
enum class ExportFormat {
    HTML,           // Rich HTML with CSS styling
    JSON,           // Structured JSON data
    MARKDOWN,       // Markdown documentation format
    XML,            // XML data format
    CSV,            // Comma-separated values
    PLAIN_TEXT      // Plain text format
};
```

### ReportSection

Individual sections that can be included in reports.

```cpp
enum class ReportSection {
    OS,             // Operating system information
    CPU,            // CPU details and performance
    MEMORY,         // Memory usage and configuration
    DISK,           // Storage devices and usage
    GPU,            // Graphics card information
    BATTERY,        // Battery status and health
    BIOS,           // BIOS/UEFI information
    NETWORK,        // Network interfaces and connectivity
    LOCALE,         // Locale and language settings
    SYSTEM          // Desktop environment and window manager
};
```

## Configuration Structures

### FormatterOptions

Configuration for formatting system information output.

```cpp
struct FormatterOptions {
    FormatterStyle style = FormatterStyle::STANDARD;
    OutputFormat format = OutputFormat::TABLE;
    bool colorEnabled = false;
    bool timestampEnabled = false;
    int tableWidth = 100;
    std::string indentation = "  ";
    
    // Custom options map for formatter-specific settings
    std::unordered_map<std::string, std::string> customOptions;
};
```

### ExportOptions

Configuration for exporting reports to various formats.

```cpp
struct ExportOptions {
    std::string title = "System Information Report";
    std::string description;
    std::string author;
    std::string templatePath;
    bool includeTimestamp = true;
    bool includeMetadata = true;
    bool prettyPrint = false;
};
```

### ReportOptions

Configuration for generating custom reports.

```cpp
struct ReportOptions {
    ReportType type = ReportType::FULL;
    std::string title;
    std::string description;
    FormatterStyle style = FormatterStyle::STANDARD;
    OutputFormat format = OutputFormat::TABLE;
    std::vector<ReportSection> sections;
    bool includeTimestamp = true;
    bool colorEnabled = false;
};
```

## Formatter Classes

### Base Formatter

All formatters inherit from `BaseFormatter` and provide consistent interfaces.

```cpp
class BaseFormatter {
public:
    void setOptions(const FormatterOptions& options);
    void setStyle(FormatterStyle style);
    void setColorEnabled(bool enabled);
    
protected:
    // Utility methods available to all formatters
    auto createTableHeader(const std::string& title, int width = 100) -> std::string;
    auto createTableRow(const std::string& label, const std::string& value, int width = 100) -> std::string;
    auto createTableFooter(int width = 100) -> std::string;
    auto formatBytes(uint64_t bytes) -> std::string;
    auto formatPercentage(double percentage) -> std::string;
    auto formatTemperature(double celsius) -> std::string;
    auto formatFrequency(double hertz) -> std::string;
    auto colorize(const std::string& text, const std::string& color) -> std::string;
    auto addTimestamp(const std::string& content) -> std::string;
};
```

### Specialized Formatters

#### CpuFormatter

```cpp
class CpuFormatter : public BaseFormatter {
public:
    auto format(const CpuInfo& info) -> std::string;
    auto format(const CpuInfo& info, const std::string& title) -> std::string;
    auto formatBasic(const CpuInfo& info) -> std::string;
    auto formatPerformance(const CpuInfo& info) -> std::string;
};
```

#### MemoryFormatter

```cpp
class MemoryFormatter : public BaseFormatter {
public:
    auto format(const DetailedMemoryInfo& info) -> std::string;
    auto format(const DetailedMemoryInfo& info, const std::string& title) -> std::string;
    auto formatBasic(const DetailedMemoryInfo& info) -> std::string;
    auto formatUsage(const DetailedMemoryInfo& info) -> std::string;
};
```

#### BatteryFormatter

```cpp
class BatteryFormatter : public BaseFormatter {
public:
    auto format(const BatteryInfo& info) -> std::string;
    auto format(const BatteryInfo& info, const std::string& title) -> std::string;
    auto formatBasic(const BatteryInfo& info) -> std::string;
    auto formatStatus(const BatteryInfo& info) -> std::string;
};
```

## Exporter Classes

### Base Exporter

All exporters inherit from `BaseExporter`.

```cpp
class BaseExporter {
public:
    virtual bool exportToFile(const std::string& content, const std::string& filename) = 0;
    virtual auto exportToString(const std::string& content) -> std::string = 0;
    virtual auto getFileExtension() const -> std::string = 0;
    virtual auto getMimeType() const -> std::string = 0;
    
    void setOptions(const ExportOptions& options);
    
protected:
    virtual auto escapeText(const std::string& text) const -> std::string = 0;
    virtual auto addMetadata(const std::string& content) const -> std::string = 0;
    
    bool writeToFile(const std::string& content, const std::string& filename);
    auto getCurrentTimestamp() const -> std::string;
};
```

### Specialized Exporters

#### HtmlExporter

```cpp
class HtmlExporter : public BaseExporter {
public:
    bool exportToFile(const std::string& content, const std::string& filename) override;
    auto exportToString(const std::string& content) -> std::string override;
    auto getFileExtension() const -> std::string override; // Returns ".html"
    auto getMimeType() const -> std::string override;      // Returns "text/html"
};
```

#### JsonExporter

```cpp
class JsonExporter : public BaseExporter {
public:
    bool exportToFile(const std::string& content, const std::string& filename) override;
    auto exportToString(const std::string& content) -> std::string override;
    auto getFileExtension() const -> std::string override; // Returns ".json"
    auto getMimeType() const -> std::string override;      // Returns "application/json"
};
```

## Report Classes

### Base Report

All report generators inherit from `BaseReport`.

```cpp
class BaseReport {
public:
    virtual auto generate() -> std::string = 0;
    
    void setOptions(const ReportOptions& options);
    
protected:
    virtual auto getDefaultSections() const -> std::vector<ReportSection> = 0;
    
    auto generateSection(ReportSection section) -> std::string;
    auto combineSections(const std::unordered_map<ReportSection, std::string>& sections) -> std::string;
    auto createHeader() -> std::string;
    auto createFooter() -> std::string;
};
```

### Specialized Reports

#### FullReport

```cpp
class FullReport : public BaseReport {
public:
    auto generate() -> std::string override;
    
protected:
    auto getDefaultSections() const -> std::vector<ReportSection> override;
    // Returns: {OS, CPU, MEMORY, DISK, GPU, BATTERY, BIOS, NETWORK, LOCALE, SYSTEM}
};
```

#### CustomReport

```cpp
class CustomReport : public BaseReport {
public:
    auto generate() -> std::string override;
    
protected:
    auto getDefaultSections() const -> std::vector<ReportSection> override;
    // Returns: {} (empty - user must specify sections)
};
```

## Utility Functions

### Format Utils

```cpp
namespace atom::system::utils {
    auto formatBytes(uint64_t bytes) -> std::string;
    auto formatPercentage(double percentage, int precision = 1) -> std::string;
    auto formatTemperature(double celsius, int precision = 1) -> std::string;
    auto formatFrequency(double hertz, int precision = 2) -> std::string;
    auto formatDuration(uint64_t seconds) -> std::string;
    auto formatTimestamp() -> std::string;
    auto formatUptime(uint64_t uptimeSeconds) -> std::string;
    auto formatNetworkSpeed(double bytesPerSecond) -> std::string;
    auto formatBoolean(bool value) -> std::string;
}
```

### String Utils

```cpp
namespace atom::system::utils {
    auto trim(const std::string& str) -> std::string;
    auto toLower(const std::string& str) -> std::string;
    auto toUpper(const std::string& str) -> std::string;
    auto split(const std::string& str, char delimiter) -> std::vector<std::string>;
    auto join(const std::vector<std::string>& strings, const std::string& delimiter) -> std::string;
    auto replace(const std::string& str, const std::string& from, const std::string& to) -> std::string;
    auto startsWith(const std::string& str, const std::string& prefix) -> bool;
    auto endsWith(const std::string& str, const std::string& suffix) -> bool;
    auto wordWrap(const std::string& text, size_t width) -> std::vector<std::string>;
}
```

## Error Handling

All API functions use standard C++ exception handling. Common exceptions:

- `std::runtime_error`: General runtime errors
- `std::invalid_argument`: Invalid parameters
- `std::filesystem::filesystem_error`: File I/O errors

```cpp
try {
    SystemInfoPrinter printer;
    auto report = printer.generateReport(ReportType::FULL);
    printer.exportReport(report, "report.html", ExportFormat::HTML);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## Thread Safety

- All formatters and exporters are thread-safe for read operations
- `SystemInfoPrinter` instances are not thread-safe; use separate instances per thread
- Utility functions are thread-safe
- Cache and performance monitoring utilities include internal synchronization

## Performance Considerations

- Use caching for repeated operations
- Enable performance monitoring for optimization
- Consider using `ReportType::SIMPLE` for frequent updates
- Batch multiple exports when possible

## Migration from Legacy API

The legacy API remains fully supported:

```cpp
// Legacy (still works)
auto report = SystemInfoPrinter::generateFullReport();
SystemInfoPrinter::exportToHTML("report.html");

// New API (recommended)
SystemInfoPrinter printer;
auto report = printer.generateReport(ReportType::FULL);
printer.exportReport(report, "report.html", ExportFormat::HTML);
```
