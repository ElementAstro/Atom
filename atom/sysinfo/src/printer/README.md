# System Information Printer Module

A comprehensive, modular system information formatting and reporting library for the Atom project. This module provides advanced formatting, reporting, and export capabilities for system information across multiple platforms.

## Features

### 🎨 Advanced Formatting
- **Modular Formatters**: Separate formatters for each system component (CPU, Memory, Disk, etc.)
- **Customizable Output**: Support for multiple output formats and styles
- **Template-Based**: Flexible template engine for custom formatting
- **Table Utilities**: Advanced table formatting with alignment and styling

### 📊 Comprehensive Reports
- **Full System Report**: Complete hardware and software information
- **Performance Report**: Focus on performance metrics and benchmarks
- **Security Report**: Security-related information and vulnerabilities
- **Hardware Report**: Detailed hardware component information
- **Software Report**: Installed software and system configuration
- **Custom Reports**: Build your own reports with custom templates

### 📤 Multiple Export Formats
- **HTML**: Rich, styled HTML reports with CSS
- **JSON**: Structured data for programmatic access
- **Markdown**: Documentation-friendly format
- **XML**: Structured markup for data exchange
- **CSV**: Tabular data for spreadsheet applications
- **PDF**: Professional reports (future feature)

### 🔧 Enhanced API
- **Real-time Monitoring**: Live system information updates
- **Caching**: Intelligent caching for performance
- **Error Handling**: Robust error handling and recovery
- **Thread Safety**: Safe for multi-threaded applications

## Quick Start

### Basic Usage

```cpp
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

using namespace atom::system;

// Create a printer instance
SystemInfoPrinter printer;

// Generate a simple report
auto report = printer.generateSimpleReport();
std::cout << report << std::endl;

// Export to HTML
printer.exportToHTML("system_report.html");
```

### Custom Formatting

```cpp
#include "atom/sysinfo/sysinfo_printer/formatters/cpu_formatter.hpp"

// Get CPU information
auto cpuInfo = getCpuInfo();

// Create custom formatter
CpuFormatter formatter;
formatter.setStyle(FormatterStyle::DETAILED);
formatter.setColorEnabled(true);

// Format the information
auto formatted = formatter.format(cpuInfo);
std::cout << formatted << std::endl;
```

### Advanced Reports

```cpp
#include "atom/sysinfo/sysinfo_printer/reports/custom_report.hpp"

// Create a custom report
CustomReport report;
report.addSection("cpu", "CPU Information")
      .addSection("memory", "Memory Information")
      .addSection("disk", "Storage Information");

// Configure formatting
report.setTemplate("detailed_template.html");
report.setOutputFormat(OutputFormat::HTML);

// Generate and save
auto content = report.generate();
report.saveToFile("custom_report.html");
```

## Architecture

### Module Structure

```
sysinfo_printer/
├── printer.hpp/cpp           # Main SystemInfoPrinter class
├── formatters/               # Component-specific formatters
├── exporters/               # Export format implementations
├── reports/                 # Report generators
├── utils/                   # Utility functions and helpers
├── templates/               # Template files for reports
├── examples/                # Usage examples
└── tests/                   # Unit tests
```

### Key Components

1. **Formatters**: Convert raw system data into formatted strings
2. **Exporters**: Handle different output formats (HTML, JSON, etc.)
3. **Reports**: Combine multiple formatters into comprehensive reports
4. **Utils**: Common utilities for string manipulation, table formatting, etc.

## Compatibility

This module maintains full backward compatibility with the original `sysinfo_printer.hpp` API while providing enhanced functionality through the new modular structure.

### Legacy Support

```cpp
// Original API still works
#include "atom/sysinfo/sysinfo_printer.hpp"

auto report = SystemInfoPrinter::generateFullReport();
bool success = SystemInfoPrinter::exportToHTML("report.html");
```

### New Enhanced API

```cpp
// New modular API
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

SystemInfoPrinter printer;
printer.setOutputFormat(OutputFormat::HTML);
printer.setTemplate("modern_template.html");
auto report = printer.generateReport(ReportType::FULL);
```

## Building

### Requirements

- C++20 compatible compiler
- CMake 3.20 or higher
- spdlog library

### Build Options

```bash
# Basic build
mkdir build && cd build
cmake ..
make

# With examples
cmake -DBUILD_SYSINFO_PRINTER_EXAMPLES=ON ..
make

# With tests
cmake -DBUILD_SYSINFO_PRINTER_TESTS=ON ..
make test
```

## Platform Support

- ✅ Windows (Windows 10/11)
- ✅ Linux (Ubuntu 20.04+, CentOS 8+)
- ✅ macOS (10.15+)
- ✅ FreeBSD (12.0+)

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## License

Copyright (C) 2023-2024 Max Qian <lightapt.com>

This project is part of the Atom system information library.

## Changelog

### Version 1.0.0
- Initial modular implementation
- Separated formatters, exporters, and reports
- Added template engine support
- Enhanced API with new features
- Maintained backward compatibility
- Added comprehensive test suite

## Migration Guide

### From Original sysinfo_printer

If you're migrating from the original sysinfo_printer implementation, the process is seamless:

#### No Changes Required
```cpp
// This code continues to work exactly as before
#include "atom/sysinfo/sysinfo_printer.hpp"

auto report = atom::system::SystemInfoPrinter::generateFullReport();
bool success = atom::system::SystemInfoPrinter::exportToHTML("report.html");
```

#### Enhanced Features Available
```cpp
// New enhanced API (optional upgrade)
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

atom::system::SystemInfoPrinter printer;
printer.setFormatterOptions({
    .style = FormatterStyle::DETAILED,
    .colorEnabled = true,
    .timestampEnabled = true
});

auto report = printer.generateReport(ReportType::PERFORMANCE);
printer.exportReport(report, "enhanced_report.html", ExportFormat::HTML);
```

### File Structure Changes

The original files have been reorganized:
- `sysinfo_printer.hpp` → Compatibility wrapper (unchanged API)
- `sysinfo_printer.cpp` → Compatibility implementation
- `sysinfo_printer/` → New modular implementation

### Build System Changes

Add to your CMakeLists.txt:
```cmake
find_package(atom_sysinfo_printer REQUIRED)
target_link_libraries(your_target PRIVATE atom_sysinfo_printer)
```

## Advanced Usage

### Custom Report Generation

```cpp
#include "atom/sysinfo/sysinfo_printer/reports/custom_report.hpp"

// Create a custom report with specific sections
ReportOptions options;
options.sections = {ReportSection::CPU, ReportSection::MEMORY};
options.style = FormatterStyle::VERBOSE;
options.title = "Performance Analysis Report";

auto report = createReport<CustomReport>(options);
auto content = report->generate();
```

### Template-Based Formatting

```cpp
#include "atom/sysinfo/sysinfo_printer/utils/template_engine.hpp"

// Use custom templates for formatting
ExportOptions exportOptions;
exportOptions.templatePath = "custom_template.html";
exportOptions.title = "Custom System Report";

HtmlExporter exporter(exportOptions);
exporter.exportToFile(content, "custom_report.html");
```

### Real-Time Monitoring

```cpp
// Monitor system changes in real-time
SystemInfoPrinter printer;
printer.setFormatterOptions({.timestampEnabled = true});

while (monitoring) {
    auto report = printer.generateReport(ReportType::PERFORMANCE);
    // Process real-time data
    std::this_thread::sleep_for(std::chrono::seconds(5));
}
```

## API Reference

### Core Classes

- **SystemInfoPrinter**: Main interface for report generation
- **BaseFormatter**: Base class for all formatters
- **BaseExporter**: Base class for all exporters
- **BaseReport**: Base class for all report generators

### Formatters

- **CpuFormatter**: CPU information formatting
- **MemoryFormatter**: Memory information formatting
- **BatteryFormatter**: Battery information formatting
- **DiskFormatter**: Storage information formatting
- **OsFormatter**: Operating system information formatting
- **BiosFormatter**: BIOS/UEFI information formatting

### Exporters

- **HtmlExporter**: HTML format with CSS styling
- **JsonExporter**: JSON format for structured data
- **MarkdownExporter**: Markdown format for documentation
- **XmlExporter**: XML format for data exchange
- **CsvExporter**: CSV format for spreadsheet applications

### Reports

- **FullReport**: Complete system information
- **SimpleReport**: Essential system overview
- **PerformanceReport**: Performance-focused metrics
- **SecurityReport**: Security-related information
- **HardwareReport**: Hardware component details
- **SoftwareReport**: Software and configuration details
- **CustomReport**: User-defined report sections

## Future Roadmap

- [ ] PDF export support
- [ ] Real-time dashboard web interface
- [ ] Plugin system for custom formatters
- [ ] Database export capabilities
- [ ] Performance benchmarking integration
- [ ] Cloud service integration
- [ ] Mobile platform support
- [ ] Machine learning-based anomaly detection
- [ ] Integration with monitoring systems (Prometheus, Grafana)
- [ ] REST API for remote system information access
