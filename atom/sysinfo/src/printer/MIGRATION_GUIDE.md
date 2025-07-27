# Migration Guide: sysinfo_printer Modular Refactoring

## Overview

The sysinfo_printer module has been successfully refactored into a modular, extensible architecture while maintaining 100% backward compatibility with the original API. This guide explains the changes and how to take advantage of the new features.

## What Changed

### 🏗️ Architecture

**Before:**
- Single monolithic files: `sysinfo_printer.hpp` and `sysinfo_printer.cpp`
- All functionality in one class
- Limited customization options
- Basic export formats

**After:**
- Modular folder structure with specialized components
- Separate formatters, exporters, and report generators
- Extensive customization and configuration options
- Enhanced export formats with templates
- Utility libraries for common operations

### 📁 New Folder Structure

```
sysinfo_printer/
├── printer.hpp/cpp           # Main enhanced API
├── formatters/               # Component-specific formatters
│   ├── base_formatter.hpp/cpp
│   ├── cpu_formatter.hpp/cpp
│   ├── memory_formatter.hpp/cpp
│   ├── battery_formatter.hpp/cpp
│   └── ... (other formatters)
├── exporters/               # Export format implementations
│   ├── base_exporter.hpp/cpp
│   ├── html_exporter.hpp/cpp
│   ├── json_exporter.hpp/cpp
│   └── ... (other exporters)
├── reports/                 # Report generators
│   ├── base_report.hpp/cpp
│   ├── full_report.hpp/cpp
│   ├── custom_report.hpp/cpp
│   └── ... (other reports)
├── utils/                   # Utility functions
│   ├── table_utils.hpp/cpp
│   ├── format_utils.hpp/cpp
│   └── ... (other utilities)
├── templates/               # Template files
├── examples/                # Usage examples
└── tests/                   # Unit tests
```

## Backward Compatibility

### ✅ No Changes Required

Your existing code continues to work exactly as before:

```cpp
#include "atom/sysinfo/sysinfo_printer.hpp"

// All original functions work unchanged
auto report = atom::system::SystemInfoPrinter::generateFullReport();
bool success = atom::system::SystemInfoPrinter::exportToHTML("report.html");

// Helper functions still available
auto bytes = atom::system::SystemInfoPrinter::formatBytes(1024*1024);
auto row = atom::system::SystemInfoPrinter::createTableRow("Label", "Value");
```

### 🔄 Compatibility Layer

The original `sysinfo_printer.hpp` now acts as a compatibility wrapper that:
- Preserves all original function signatures
- Delegates to the new modular implementation
- Maintains identical behavior and output
- Provides deprecation warnings for outdated global functions

## New Enhanced Features

### 🎨 Advanced Formatting Options

```cpp
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

// Create printer with custom options
FormatterOptions options;
options.style = FormatterStyle::DETAILED;
options.colorEnabled = true;
options.timestampEnabled = true;
options.tableWidth = 120;

SystemInfoPrinter printer(options);
```

### 📊 Specialized Report Types

```cpp
// Generate different types of reports
auto fullReport = printer.generateReport(ReportType::FULL);
auto perfReport = printer.generateReport(ReportType::PERFORMANCE);
auto secReport = printer.generateReport(ReportType::SECURITY);
auto hwReport = printer.generateReport(ReportType::HARDWARE);
```

### 📤 Enhanced Export Formats

```cpp
// Export with custom options
ExportOptions exportOptions;
exportOptions.title = "Custom System Report";
exportOptions.author = "System Administrator";
exportOptions.templatePath = "custom_template.html";

printer.setExportOptions(exportOptions);
printer.exportReport(content, "report.html", ExportFormat::HTML);
```

### 🔧 Custom Formatters

```cpp
#include "atom/sysinfo/sysinfo_printer/formatters/cpu_formatter.hpp"

// Create specialized formatter
auto cpuFormatter = printer.createFormatter<CpuFormatter>();
cpuFormatter->setStyle(FormatterStyle::VERBOSE);
cpuFormatter->setColorEnabled(true);

auto cpuInfo = getCpuInfo();
auto formatted = cpuFormatter->format(cpuInfo);
```

## Migration Strategies

### 1. No Migration (Recommended for Existing Code)

Keep using the original API - no changes needed:

```cpp
// This continues to work exactly as before
#include "atom/sysinfo/sysinfo_printer.hpp"
auto report = SystemInfoPrinter::generateFullReport();
```

### 2. Gradual Migration

Gradually adopt new features while keeping existing code:

```cpp
// Mix old and new APIs
#include "atom/sysinfo/sysinfo_printer.hpp"
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

// Use legacy for basic functionality
auto basicReport = SystemInfoPrinter::generateFullReport();

// Use enhanced API for advanced features
SystemInfoPrinter enhancedPrinter;
enhancedPrinter.setFormatterOptions({.style = FormatterStyle::DETAILED});
auto detailedReport = enhancedPrinter.generateReport(ReportType::PERFORMANCE);
```

### 3. Full Migration

Adopt the new enhanced API completely:

```cpp
#include "atom/sysinfo/sysinfo_printer/printer.hpp"

SystemInfoPrinter printer;
// Configure as needed
auto report = printer.generateReport(ReportType::FULL);
printer.exportReport(report, "report.html", ExportFormat::HTML);
```

## Build System Changes

### CMakeLists.txt Updates

The build system automatically includes the new modular structure:

```cmake
# No changes needed - the new module is automatically included
target_link_libraries(your_target PRIVATE atom_sysinfo)

# Or link directly to the printer module
target_link_libraries(your_target PRIVATE atom_sysinfo_printer)
```

### New Build Options

```cmake
# Enable examples
cmake -DBUILD_SYSINFO_PRINTER_EXAMPLES=ON ..

# Enable tests
cmake -DBUILD_SYSINFO_PRINTER_TESTS=ON ..
```

## Testing Your Migration

### 1. Compatibility Test

Run the compatibility test to ensure everything works:

```bash
cd build
make test_compatibility
./test_compatibility
```

### 2. Feature Test

Test new features with examples:

```bash
make basic_usage
./basic_usage

make custom_formatting
./custom_formatting
```

### 3. Integration Test

Verify your existing code still works by running your current tests.

## Benefits of the New Architecture

### 🚀 Performance
- Modular loading - only load what you need
- Optimized formatters for specific components
- Efficient memory usage

### 🎨 Customization
- Extensive formatting options
- Custom templates and themes
- Pluggable architecture

### 🔧 Maintainability
- Clear separation of concerns
- Easy to extend and modify
- Comprehensive test coverage

### 📈 Scalability
- Add new formatters easily
- Support for new export formats
- Plugin system ready

## Troubleshooting

### Common Issues

1. **Build Errors**: Ensure CMake finds the new module
2. **Missing Headers**: Check include paths
3. **Link Errors**: Verify library dependencies

### Getting Help

- Check the examples in `sysinfo_printer/examples/`
- Run the test suite for validation
- Review the comprehensive documentation in `README.md`

## Future Considerations

The new modular architecture provides a foundation for:
- Plugin system for custom formatters
- Real-time monitoring capabilities
- Web-based dashboards
- Integration with monitoring systems
- Machine learning-based analysis

## Conclusion

The modular refactoring provides significant enhancements while maintaining complete backward compatibility. You can continue using the original API without any changes, or gradually adopt new features as needed. The new architecture positions the sysinfo_printer module for future growth and extensibility.
