#include "atom/sysinfo/sysinfo_printer.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <fstream>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(sysinfo_printer, m) {
    m.doc() =
        "System information formatting and reporting utilities for the atom "
        "package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // SystemInfoPrinter class binding
    py::class_<SystemInfoPrinter>(
        m, "SystemInfoPrinter",
        R"(Formats and presents system information in human-readable formats.

This class provides methods to format different types of system information
into readable text, generate comprehensive system reports, and export this 
information to various file formats like HTML, JSON, and Markdown.

Examples:
    >>> from atom.sysinfo import sysinfo_printer, cpu, memory, os
    >>> # Get CPU information
    >>> cpu_info = cpu.get_cpu_info()
    >>> # Format it as readable text
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_cpu_info(cpu_info)
    >>> print(formatted)
    >>> 
    >>> # Generate a comprehensive system report
    >>> full_report = sysinfo_printer.SystemInfoPrinter.generate_full_report()
    >>> print(full_report)
)")
        .def(py::init<>(), "Constructs a new SystemInfoPrinter object.")
        .def_static("format_battery_info",
                    &SystemInfoPrinter::formatBatteryInfo, py::arg("info"),
                    R"(Format battery information as a string.

Args:
    info: The battery information to format

Returns:
    A formatted string containing battery details

Examples:
    >>> from atom.sysinfo import sysinfo_printer, battery
    >>> # Get battery information
    >>> battery_info = battery.get_battery_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_battery_info(battery_info)
    >>> print(formatted)
)")
        .def_static("format_bios_info", &SystemInfoPrinter::formatBiosInfo,
                    py::arg("info"),
                    R"(Format BIOS information as a string.

Args:
    info: The BIOS information to format

Returns:
    A formatted string containing BIOS details

Examples:
    >>> from atom.sysinfo import sysinfo_printer, bios
    >>> # Get BIOS information
    >>> bios_info = bios.get_bios_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_bios_info(bios_info)
    >>> print(formatted)
)")
        .def_static("format_cpu_info", &SystemInfoPrinter::formatCpuInfo,
                    py::arg("info"),
                    R"(Format CPU information as a string.

Args:
    info: The CPU information to format

Returns:
    A formatted string containing CPU details

Examples:
    >>> from atom.sysinfo import sysinfo_printer, cpu
    >>> # Get CPU information
    >>> cpu_info = cpu.get_cpu_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_cpu_info(cpu_info)
    >>> print(formatted)
)")
        .def_static("format_disk_info", &SystemInfoPrinter::formatDiskInfo,
                    py::arg("info"),
                    R"(Format disk information as a string.

Args:
    info: Vector of disk information objects to format

Returns:
    A formatted string containing disk details for all drives

Examples:
    >>> from atom.sysinfo import sysinfo_printer, disk
    >>> # Get disk information
    >>> disk_info = disk.get_disk_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_disk_info(disk_info)
    >>> print(formatted)
)")
        .def_static("format_gpu_info", &SystemInfoPrinter::formatGpuInfo,
                    R"(Format GPU information as a string.

Returns:
    A formatted string containing GPU details

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Format GPU information
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_gpu_info()
    >>> print(formatted)
)")
        .def_static("format_locale_info", &SystemInfoPrinter::formatLocaleInfo,
                    py::arg("info"),
                    R"(Format locale information as a string.

Args:
    info: The locale information to format

Returns:
    A formatted string containing locale settings

Examples:
    >>> from atom.sysinfo import sysinfo_printer, locale
    >>> # Get locale information
    >>> locale_info = locale.get_locale_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_locale_info(locale_info)
    >>> print(formatted)
)")
        .def_static("format_memory_info", &SystemInfoPrinter::formatMemoryInfo,
                    py::arg("info"),
                    R"(Format memory information as a string.

Args:
    info: The memory information to format

Returns:
    A formatted string containing memory details

Examples:
    >>> from atom.sysinfo import sysinfo_printer, memory
    >>> # Get memory information
    >>> memory_info = memory.get_detailed_memory_stats()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_memory_info(memory_info)
    >>> print(formatted)
)")
        .def_static("format_os_info", &SystemInfoPrinter::formatOsInfo,
                    py::arg("info"),
                    R"(Format operating system information as a string.

Args:
    info: The OS information to format

Returns:
    A formatted string containing OS details

Examples:
    >>> from atom.sysinfo import sysinfo_printer, os
    >>> # Get OS information
    >>> os_info = os.get_operating_system_info()
    >>> # Format it
    >>> formatted = sysinfo_printer.SystemInfoPrinter.format_os_info(os_info)
    >>> print(formatted)
)")
        .def_static("generate_full_report",
                    &SystemInfoPrinter::generateFullReport,
                    R"(Generate a comprehensive report of all system components.

Creates a detailed report including information about all hardware and
software components of the system.

Returns:
    A string containing the full system report

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate a full system report
    >>> report = sysinfo_printer.SystemInfoPrinter.generate_full_report()
    >>> print(report)
    >>> 
    >>> # Optionally, save to a file
    >>> with open('system_report.txt', 'w') as f:
    ...     f.write(report)
)")
        .def_static("generate_simple_report",
                    &SystemInfoPrinter::generateSimpleReport,
                    R"(Generate a simplified overview of key system information.

Creates a concise report with the most important system details
suitable for quick reference.

Returns:
    A string containing the simplified system report

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate a simple system overview
    >>> report = sysinfo_printer.SystemInfoPrinter.generate_simple_report()
    >>> print(report)
)")
        .def_static("generate_performance_report",
                    &SystemInfoPrinter::generatePerformanceReport,
                    R"(Generate a report focused on system performance metrics.

Creates a report with emphasis on performance-related information
like CPU speed, memory usage, disk speeds, etc.

Returns:
    A string containing the performance-focused report

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate a performance-focused report
    >>> report = sysinfo_printer.SystemInfoPrinter.generate_performance_report()
    >>> print(report)
)")
        .def_static("generate_security_report",
                    &SystemInfoPrinter::generateSecurityReport,
                    R"(Generate a report focused on system security features.

Creates a report highlighting security-related information such as
OS security features, firmware versions, and potential vulnerabilities.

Returns:
    A string containing the security-focused report

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate a security-focused report
    >>> report = sysinfo_printer.SystemInfoPrinter.generate_security_report()
    >>> print(report)
)")
        .def_static("export_to_html", &SystemInfoPrinter::exportToHTML,
                    py::arg("filename"),
                    R"(Export system information to HTML format.

Generates a complete system information report and saves it as an
HTML file at the specified location.

Args:
    filename: The path where the HTML file will be saved

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Export system information to HTML
    >>> success = sysinfo_printer.SystemInfoPrinter.export_to_html("system_info.html")
    >>> if success:
    ...     print("Successfully exported system information to HTML")
    ... else:
    ...     print("Failed to export system information")
)")
        .def_static("export_to_json", &SystemInfoPrinter::exportToJSON,
                    py::arg("filename"),
                    R"(Export system information to JSON format.

Generates a complete system information report and saves it as a
structured JSON file at the specified location.

Args:
    filename: The path where the JSON file will be saved

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Export system information to JSON
    >>> success = sysinfo_printer.SystemInfoPrinter.export_to_json("system_info.json")
    >>> if success:
    ...     print("Successfully exported system information to JSON")
    ... else:
    ...     print("Failed to export system information")
)")
        .def_static("export_to_markdown", &SystemInfoPrinter::exportToMarkdown,
                    py::arg("filename"),
                    R"(Export system information to Markdown format.

Generates a complete system information report and saves it as a
Markdown file at the specified location.

Args:
    filename: The path where the Markdown file will be saved

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Export system information to Markdown
    >>> success = sysinfo_printer.SystemInfoPrinter.export_to_markdown("system_info.md")
    >>> if success:
    ...     print("Successfully exported system information to Markdown")
    ... else:
    ...     print("Failed to export system information")
)");

    // Additional utility functions
    m.def(
        "save_report_to_file",
        [](const std::string& report, const std::string& filename) {
            try {
                std::ofstream file(filename);
                if (!file.is_open()) {
                    return false;
                }
                file << report;
                file.close();
                return true;
            } catch (...) {
                return false;
            }
        },
        py::arg("report"), py::arg("filename"),
        R"(Save a system information report to a text file.

Args:
    report: The report string to save
    filename: The path where the file will be saved

Returns:
    Boolean indicating success or failure

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate a report
    >>> report = sysinfo_printer.SystemInfoPrinter.generate_full_report()
    >>> # Save it to a file
    >>> success = sysinfo_printer.save_report_to_file(report, "system_report.txt")
    >>> if success:
    ...     print("Successfully saved report to file")
    ... else:
    ...     print("Failed to save report")
)");

    m.def(
        "generate_all_reports",
        []() {
            py::dict reports;
            reports["full"] = SystemInfoPrinter::generateFullReport();
            reports["simple"] = SystemInfoPrinter::generateSimpleReport();
            reports["performance"] =
                SystemInfoPrinter::generatePerformanceReport();
            reports["security"] = SystemInfoPrinter::generateSecurityReport();
            return reports;
        },
        R"(Generate all available system information reports.

Returns:
    Dictionary containing all report types

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Generate all report types
    >>> reports = sysinfo_printer.generate_all_reports()
    >>> # Access individual reports
    >>> print("Simple Report:\n", reports["simple"])
    >>> print("\nPerformance Report:\n", reports["performance"])
)");

    m.def(
        "export_all_formats",
        [](const std::string& base_filename) {
            bool html_success =
                SystemInfoPrinter::exportToHTML(base_filename + ".html");
            bool json_success =
                SystemInfoPrinter::exportToJSON(base_filename + ".json");
            bool md_success =
                SystemInfoPrinter::exportToMarkdown(base_filename + ".md");

            py::dict results;
            results["html"] = html_success;
            results["json"] = json_success;
            results["markdown"] = md_success;
            results["all_succeeded"] =
                html_success && json_success && md_success;

            return results;
        },
        py::arg("base_filename"),
        R"(Export system information in all available formats.

Args:
    base_filename: The base filename to use (without extension)

Returns:
    Dictionary with results for each format and overall success status

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Export system information in all formats
    >>> results = sysinfo_printer.export_all_formats("system_info")
    >>> if results["all_succeeded"]:
    ...     print("Successfully exported in all formats")
    ... else:
    ...     for fmt, success in results.items():
    ...         if fmt != "all_succeeded":
    ...             print(f"{fmt}: {'Success' if success else 'Failed'}")
)");

    // Convenience context manager for report generation
    py::class_<py::object>(m, "ReportContext")
        .def(py::init([](const std::string& report_type) {
                 py::object obj;
                 if (report_type != "full" && report_type != "simple" &&
                     report_type != "performance" &&
                     report_type != "security") {
                     throw std::invalid_argument(
                         "Invalid report type. Valid options: 'full', "
                         "'simple', 'performance', 'security'");
                 }
                 obj.attr("report_type") = py::str(report_type);
                 return obj;
             }),
             py::arg("report_type") = "full",
             "Create a context manager for system report generation")
        .def("__enter__",
             [](py::object& self) {
                 std::string report_type =
                     py::cast<std::string>(self.attr("report_type"));

                 std::string report;
                 if (report_type == "full") {
                     report = SystemInfoPrinter::generateFullReport();
                 } else if (report_type == "simple") {
                     report = SystemInfoPrinter::generateSimpleReport();
                 } else if (report_type == "performance") {
                     report = SystemInfoPrinter::generatePerformanceReport();
                 } else if (report_type == "security") {
                     report = SystemInfoPrinter::generateSecurityReport();
                 }

                 self.attr("report") = py::str(report);
                 self.attr("start_time") =
                     py::module::import("time").attr("time")();

                 return self;
             })
        .def("__exit__",
             [](py::object& self, py::object, py::object, py::object) {
                 py::object end_time =
                     py::module::import("time").attr("time")();
                 py::object elapsed = end_time - self.attr("start_time");
                 self.attr("elapsed_time") = elapsed;

                 return py::bool_(false);  // Don't suppress exceptions
             })
        .def_property_readonly(
            "content", [](py::object& self) { return self.attr("report"); },
            "The generated report content")
        .def_property_readonly(
            "elapsed_time",
            [](py::object& self) { return self.attr("elapsed_time"); },
            "Time taken to generate the report (seconds)")
        .def(
            "save",
            [](py::object& self, const std::string& filename) {
                std::string report = py::cast<std::string>(self.attr("report"));
                try {
                    std::ofstream file(filename);
                    if (!file.is_open()) {
                        return false;
                    }
                    file << report;
                    file.close();
                    return true;
                } catch (...) {
                    return false;
                }
            },
            py::arg("filename"), "Save the report to a file");

    // Factory function for report context
    m.def(
        "generate_report",
        [&m](const std::string& report_type) {
            return m.attr("ReportContext")(report_type);
        },
        py::arg("report_type") = "full",
        R"(Create a context manager for generating system reports.

Args:
    report_type: Type of report to generate. Options: 'full', 'simple', 'performance', 'security'

Returns:
    A context manager that generates the specified report

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Use as a context manager
    >>> with sysinfo_printer.generate_report("performance") as report:
    ...     print(f"Generated performance report in {report.elapsed_time:.2f} seconds")
    ...     print("First 200 characters of report:")
    ...     print(report.content[:200])
    ...     # Save to file if needed
    ...     report.save("performance_report.txt")
)");

    // Batch export function
    m.def(
        "batch_export",
        [](const std::string& directory,
           const std::vector<std::string>& formats,
           const std::vector<std::string>& report_types) {
            py::dict results;

            // Create a mapping of report type to generator function
            std::map<std::string, std::function<std::string()>>
                report_generators;
            report_generators["full"] = SystemInfoPrinter::generateFullReport;
            report_generators["simple"] =
                SystemInfoPrinter::generateSimpleReport;
            report_generators["performance"] =
                SystemInfoPrinter::generatePerformanceReport;
            report_generators["security"] =
                SystemInfoPrinter::generateSecurityReport;

            // Create a mapping of format extensions
            std::map<std::string, std::string> format_extensions;
            format_extensions["txt"] = ".txt";
            format_extensions["html"] = ".html";
            format_extensions["json"] = ".json";
            format_extensions["markdown"] = ".md";

            // Function to ensure directory has trailing separator
            auto ensure_dir_sep = [](const std::string& dir) -> std::string {
                if (dir.empty())
                    return "";
                char last_char = dir[dir.size() - 1];
                if (last_char != '/' && last_char != '\\') {
                    return dir + "/";
                }
                return dir;
            };

            std::string dir_path = ensure_dir_sep(directory);

            // Process each report type
            for (const auto& report_type : report_types) {
                // Check if this is a valid report type
                if (report_generators.find(report_type) ==
                    report_generators.end()) {
                    results[py::str(report_type)] =
                        py::str("Invalid report type");
                    continue;
                }

                // Generate the report
                std::string report;
                try {
                    report = report_generators[report_type]();
                } catch (const std::exception& e) {
                    results[py::str(report_type)] = py::str(
                        "Error generating report: " + std::string(e.what()));
                    continue;
                }

                // Create a sub-dictionary for this report type
                py::dict format_results;

                // Export in each requested format
                for (const auto& format : formats) {
                    try {
                        std::string filename =
                            dir_path + "system_" + report_type;
                        bool success = false;

                        if (format == "txt") {
                            std::ofstream file(filename + ".txt");
                            if (file.is_open()) {
                                file << report;
                                file.close();
                                success = true;
                            }
                        } else if (format == "html") {
                            success = SystemInfoPrinter::exportToHTML(filename +
                                                                      ".html");
                        } else if (format == "json") {
                            success = SystemInfoPrinter::exportToJSON(filename +
                                                                      ".json");
                        } else if (format == "markdown") {
                            success = SystemInfoPrinter::exportToMarkdown(
                                filename + ".md");
                        } else {
                            format_results[py::str(format)] =
                                py::str("Invalid format");
                            continue;
                        }

                        format_results[py::str(format)] = py::bool_(success);
                    } catch (const std::exception& e) {
                        format_results[py::str(format)] =
                            py::str("Error: " + std::string(e.what()));
                    }
                }

                results[py::str(report_type)] = format_results;
            }

            return results;
        },
        py::arg("directory") = "",
        py::arg("formats") =
            std::vector<std::string>{"txt", "html", "json", "markdown"},
        py::arg("report_types") =
            std::vector<std::string>{"full", "simple", "performance",
                                     "security"},
        R"(Export multiple report types in multiple formats.

Args:
    directory: Directory where reports will be saved
    formats: List of formats to export (valid: 'txt', 'html', 'json', 'markdown')
    report_types: List of report types to generate (valid: 'full', 'simple', 'performance', 'security')

Returns:
    Nested dictionary with results for each report type and format

Examples:
    >>> from atom.sysinfo import sysinfo_printer
    >>> # Export performance and security reports in HTML and Markdown formats
    >>> results = sysinfo_printer.batch_export(
    ...     directory="reports",
    ...     formats=["html", "markdown"],
    ...     report_types=["performance", "security"]
    ... )
    >>> 
    >>> # Check results
    >>> for report_type, formats in results.items():
    ...     print(f"{report_type} report:")
    ...     for format_name, result in formats.items():
    ...         print(f"  {format_name}: {result}")
)");
}