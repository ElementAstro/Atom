/**
 * @file sysinfo_printer.hpp
 * @brief System information formatting and reporting utilities
 *
 * This file contains definitions for classes and functions that format system information
 * into human-readable text and reports. It provides utilities for generating formatted
 * system information displays and exporting them to various file formats.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

 #ifndef ATOM_SYSINFO_PRINTER_HPP
 #define ATOM_SYSINFO_PRINTER_HPP
 
 #include <string>
 #include <vector>
 
 #include "battery.hpp"
 #include "bios.hpp"
 #include "cpu.hpp"
 #include "disk.hpp"
 #include "locale.hpp"
 #include "memory.hpp"
 #include "os.hpp"
 #include "wm.hpp"
 
 namespace atom::system {
 
 /**
  * @class SystemInfoPrinter
  * @brief Formats and presents system information in human-readable formats
  *
  * This class provides static methods to format different types of system information
  * into readable text, generate comprehensive system reports, and export this information
  * to various file formats like HTML, JSON, and Markdown.
  */
 class SystemInfoPrinter {
 public:
     /**
      * @brief Format battery information as a string
      * @param info The battery information to format
      * @return A formatted string containing battery details
      */
     static auto formatBatteryInfo(const BatteryInfo& info) -> std::string;
 
     /**
      * @brief Format BIOS information as a string
      * @param info The BIOS information to format
      * @return A formatted string containing BIOS details
      */
     static auto formatBiosInfo(const BiosInfoData& info) -> std::string;
 
     /**
      * @brief Format CPU information as a string
      * @param info The CPU information to format
      * @return A formatted string containing CPU details
      */
     static auto formatCpuInfo(const CpuInfo& info) -> std::string;
 
     /**
      * @brief Format disk information as a string
      * @param info Vector of disk information objects to format
      * @return A formatted string containing disk details for all drives
      */
     static auto formatDiskInfo(const std::vector<DiskInfo>& info)
         -> std::string;
 
     /**
      * @brief Format GPU information as a string
      * @return A formatted string containing GPU details
      */
     static auto formatGpuInfo() -> std::string;
 
     /**
      * @brief Format locale information as a string
      * @param info The locale information to format
      * @return A formatted string containing locale settings
      */
     static auto formatLocaleInfo(const LocaleInfo& info) -> std::string;
 
     /**
      * @brief Format memory information as a string
      * @param info The memory information to format
      * @return A formatted string containing memory details
      */
     static auto formatMemoryInfo(const MemoryInfo& info) -> std::string;
 
     /**
      * @brief Format operating system information as a string
      * @param info The OS information to format
      * @return A formatted string containing OS details
      */
     static auto formatOsInfo(const OperatingSystemInfo& info) -> std::string;
 
     /**
      * @brief Format comprehensive system information as a string
      * @param info The system information structure to format
      * @return A formatted string containing system details
      */
     static auto formatSystemInfo(const SystemInfo& info) -> std::string;
 
     /**
      * @brief Generate a comprehensive report of all system components
      * 
      * Creates a detailed report including information about all hardware and
      * software components of the system.
      * 
      * @return A string containing the full system report
      */
     static auto generateFullReport() -> std::string;
 
     /**
      * @brief Generate a simplified overview of key system information
      * 
      * Creates a concise report with the most important system details
      * suitable for quick reference.
      * 
      * @return A string containing the simplified system report
      */
     static auto generateSimpleReport() -> std::string;
 
     /**
      * @brief Generate a report focused on system performance metrics
      * 
      * Creates a report with emphasis on performance-related information
      * like CPU speed, memory usage, disk speeds, etc.
      * 
      * @return A string containing the performance-focused report
      */
     static auto generatePerformanceReport() -> std::string;
 
     /**
      * @brief Generate a report focused on system security features
      * 
      * Creates a report highlighting security-related information such as
      * OS security features, firmware versions, and potential vulnerabilities.
      * 
      * @return A string containing the security-focused report
      */
     static auto generateSecurityReport() -> std::string;
 
     /**
      * @brief Export system information to HTML format
      * 
      * Generates a complete system information report and saves it as an
      * HTML file at the specified location.
      * 
      * @param filename The path where the HTML file will be saved
      * @return true if export was successful, false otherwise
      */
     static bool exportToHTML(const std::string& filename);
 
     /**
      * @brief Export system information to JSON format
      * 
      * Generates a complete system information report and saves it as a
      * structured JSON file at the specified location.
      * 
      * @param filename The path where the JSON file will be saved
      * @return true if export was successful, false otherwise
      */
     static bool exportToJSON(const std::string& filename);
 
     /**
      * @brief Export system information to Markdown format
      * 
      * Generates a complete system information report and saves it as a
      * Markdown file at the specified location.
      * 
      * @param filename The path where the Markdown file will be saved
      * @return true if export was successful, false otherwise
      */
     static bool exportToMarkdown(const std::string& filename);
 
 private:
     /**
      * @brief Helper method to create a formatted table row
      * @param label The label or name for the row
      * @param value The value to display in the row
      * @return A formatted string representing a table row
      */
     static auto createTableRow(const std::string& label,
                                const std::string& value) -> std::string;
 
     /**
      * @brief Helper method to create a formatted table header
      * @param title The title of the table
      * @return A formatted string representing a table header
      */
     static auto createTableHeader(const std::string& title) -> std::string;
 
     /**
      * @brief Helper method to create a formatted table footer
      * @return A formatted string representing a table footer
      */
     static auto createTableFooter() -> std::string;
 };
 
 }  // namespace atom::system
 
 #endif  // ATOM_SYSINFO_PRINTER_HPP