/*
 * sysinfo_printer.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for System Information Printer Module
Tests formatting functions and report generation.

**************************************************/

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "atom/sysinfo/battery.hpp"
#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/memory.hpp"
#include "atom/sysinfo/os.hpp"
#include "atom/sysinfo/sysinfo_printer.hpp"

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// System Information Printer Tests
// ============================================================================

class SysInfoPrinterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup system information printer tests
        printer = std::make_unique<SystemInfoPrinter>();
    }

    void TearDown() override {
        // Cleanup
        printer.reset();
    }

    std::unique_ptr<SystemInfoPrinter> printer;
};

TEST_F(SysInfoPrinterTest, FormatBatteryInfo) {
    // Test battery information formatting
    auto batteryInfo = getBatteryInfo();

    if (batteryInfo.has_value()) {
        std::string formatted =
            SystemInfoPrinter::formatBatteryInfo(batteryInfo.value());

        // Formatted output should not be empty
        EXPECT_FALSE(formatted.empty());
        EXPECT_GT(formatted.length(), 0);

        // Should contain some expected keywords
        EXPECT_TRUE(formatted.find("Battery") != std::string::npos ||
                    formatted.find("Power") != std::string::npos ||
                    formatted.find("Charge") != std::string::npos);
    } else {
        // If no battery is present, formatting should handle gracefully
        BatteryInfo emptyInfo;
        EXPECT_NO_THROW({
            std::string formatted =
                SystemInfoPrinter::formatBatteryInfo(emptyInfo);
        });
    }
}

TEST_F(SysInfoPrinterTest, FormatCpuInfo) {
    // Test CPU information formatting
    CpuInfo cpuInfo = getCpuInfo();
    std::string formatted = SystemInfoPrinter::formatCpuInfo(cpuInfo);

    // Formatted output should not be empty
    EXPECT_FALSE(formatted.empty());
    EXPECT_GT(formatted.length(), 0);

    // Should contain some expected keywords
    EXPECT_TRUE(formatted.find("CPU") != std::string::npos ||
                formatted.find("Processor") != std::string::npos ||
                formatted.find("Core") != std::string::npos);

    // Should contain CPU model if available
    if (!cpuInfo.model.empty()) {
        EXPECT_TRUE(formatted.find(cpuInfo.model) != std::string::npos);
    }
}

TEST_F(SysInfoPrinterTest, FormatMemoryInfo) {
    // Test memory information formatting
    MemoryInfo memInfo = getDetailedMemoryStats();
    std::string formatted = SystemInfoPrinter::formatMemoryInfo(memInfo);

    // Formatted output should not be empty
    EXPECT_FALSE(formatted.empty());
    EXPECT_GT(formatted.length(), 0);

    // Should contain some expected keywords
    EXPECT_TRUE(formatted.find("Memory") != std::string::npos ||
                formatted.find("RAM") != std::string::npos ||
                formatted.find("Physical") != std::string::npos);
}

TEST_F(SysInfoPrinterTest, FormatOsInfo) {
    // Test OS information formatting
    OperatingSystemInfo osInfo = getOperatingSystemInfo();
    std::string formatted = SystemInfoPrinter::formatOsInfo(osInfo);

    // Formatted output should not be empty
    EXPECT_FALSE(formatted.empty());
    EXPECT_GT(formatted.length(), 0);

    // Should contain some expected keywords
    EXPECT_TRUE(formatted.find("Operating System") != std::string::npos ||
                formatted.find("OS") != std::string::npos ||
                formatted.find("System") != std::string::npos);

    // Should contain OS name if available
    if (!osInfo.osName.empty()) {
        EXPECT_TRUE(formatted.find(osInfo.osName) != std::string::npos);
    }
}

TEST_F(SysInfoPrinterTest, FormatGpuInfo) {
    // Test GPU information formatting
    std::string formatted = SystemInfoPrinter::formatGpuInfo();

    // Formatted output should not be empty (even if no GPU detected)
    EXPECT_FALSE(formatted.empty());
    EXPECT_GT(formatted.length(), 0);

    // Should contain some expected keywords
    EXPECT_TRUE(formatted.find("GPU") != std::string::npos ||
                formatted.find("Graphics") != std::string::npos ||
                formatted.find("Video") != std::string::npos ||
                formatted.find("Display") != std::string::npos);
}

// ============================================================================
// Report Generation Tests
// ============================================================================

TEST_F(SysInfoPrinterTest, GenerateFullReport) {
    // Test full report generation
    std::string report = SystemInfoPrinter::generateFullReport();

    // Report should not be empty
    EXPECT_FALSE(report.empty());
    EXPECT_GT(report.length(), 100);  // Should be substantial

    // Should contain sections for different components
    EXPECT_TRUE(report.find("CPU") != std::string::npos ||
                report.find("Processor") != std::string::npos);
    EXPECT_TRUE(report.find("Memory") != std::string::npos ||
                report.find("RAM") != std::string::npos);
    EXPECT_TRUE(report.find("Operating System") != std::string::npos ||
                report.find("OS") != std::string::npos);
}

TEST_F(SysInfoPrinterTest, GenerateSimpleReport) {
    // Test simple report generation
    std::string report = SystemInfoPrinter::generateSimpleReport();

    // Report should not be empty
    EXPECT_FALSE(report.empty());
    EXPECT_GT(report.length(), 50);  // Should have some content

    // Simple report should be shorter than full report
    std::string fullReport = SystemInfoPrinter::generateFullReport();
    EXPECT_LT(report.length(), fullReport.length());
}

TEST_F(SysInfoPrinterTest, GeneratePerformanceReport) {
    // Test performance report generation
    std::string report = SystemInfoPrinter::generatePerformanceReport();

    // Report should not be empty
    EXPECT_FALSE(report.empty());
    EXPECT_GT(report.length(), 50);

    // Should contain performance-related keywords
    EXPECT_TRUE(report.find("Performance") != std::string::npos ||
                report.find("Usage") != std::string::npos ||
                report.find("Load") != std::string::npos ||
                report.find("Speed") != std::string::npos);
}

TEST_F(SysInfoPrinterTest, GenerateSecurityReport) {
    // Test security report generation
    std::string report = SystemInfoPrinter::generateSecurityReport();

    // Report should not be empty
    EXPECT_FALSE(report.empty());
    EXPECT_GT(report.length(), 50);

    // Should contain security-related keywords
    EXPECT_TRUE(report.find("Security") != std::string::npos ||
                report.find("Secure") != std::string::npos ||
                report.find("Protection") != std::string::npos ||
                report.find("Encryption") != std::string::npos);
}

// ============================================================================
// Formatting Quality Tests
// ============================================================================

TEST_F(SysInfoPrinterTest, FormattingConsistency) {
    // Test that formatting is consistent across multiple calls
    CpuInfo cpuInfo = getCpuInfo();

    std::string formatted1 = SystemInfoPrinter::formatCpuInfo(cpuInfo);
    std::string formatted2 = SystemInfoPrinter::formatCpuInfo(cpuInfo);

    EXPECT_EQ(formatted1, formatted2);
}

TEST_F(SysInfoPrinterTest, ReportStructure) {
    // Test that reports have proper structure
    std::string fullReport = SystemInfoPrinter::generateFullReport();

    // Should have some structure indicators (headers, sections, etc.)
    EXPECT_TRUE(fullReport.find("=") != std::string::npos ||
                fullReport.find("-") != std::string::npos ||
                fullReport.find("*") != std::string::npos ||
                fullReport.find(":") != std::string::npos);

    // Should have line breaks for readability
    EXPECT_TRUE(fullReport.find("\n") != std::string::npos);
}

TEST_F(SysInfoPrinterTest, NoSensitiveInformation) {
    // Test that reports don't contain obviously sensitive information
    std::string fullReport = SystemInfoPrinter::generateFullReport();

    // Should not contain common sensitive patterns
    EXPECT_TRUE(fullReport.find("password") == std::string::npos);
    EXPECT_TRUE(fullReport.find("secret") == std::string::npos);
    EXPECT_TRUE(fullReport.find("key") == std::string::npos ||
                fullReport.find("keyboard") !=
                    std::string::npos);  // "keyboard" is OK
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(SysInfoPrinterTest, EmptyDataHandling) {
    // Test handling of empty data structures
    BatteryInfo emptyBattery;
    CpuInfo emptyCpu;
    MemoryInfo emptyMemory;
    OperatingSystemInfo emptyOs;

    EXPECT_NO_THROW({ SystemInfoPrinter::formatBatteryInfo(emptyBattery); });

    EXPECT_NO_THROW({ SystemInfoPrinter::formatCpuInfo(emptyCpu); });

    EXPECT_NO_THROW({ SystemInfoPrinter::formatMemoryInfo(emptyMemory); });

    EXPECT_NO_THROW({ SystemInfoPrinter::formatOsInfo(emptyOs); });
}

TEST_F(SysInfoPrinterTest, NoThrowGuarantee) {
    // Test that all methods provide no-throw guarantee
    EXPECT_NO_THROW(SystemInfoPrinter::generateFullReport());
    EXPECT_NO_THROW(SystemInfoPrinter::generateSimpleReport());
    EXPECT_NO_THROW(SystemInfoPrinter::generatePerformanceReport());
    EXPECT_NO_THROW(SystemInfoPrinter::generateSecurityReport());
    EXPECT_NO_THROW(SystemInfoPrinter::formatGpuInfo());
}

TEST_F(SysInfoPrinterTest, LargeDataHandling) {
    // Test handling of potentially large data
    std::string fullReport = SystemInfoPrinter::generateFullReport();

    // Report should be reasonable in size (not empty, but not excessively
    // large)
    EXPECT_GT(fullReport.length(), 100);
    EXPECT_LT(fullReport.length(), 1000000);  // 1MB limit for sanity
}

// ============================================================================
// Static Method Tests
// ============================================================================

TEST_F(SysInfoPrinterTest, StaticMethodsWork) {
    // Test that static methods work without instance
    EXPECT_NO_THROW({
        std::string report = SystemInfoPrinter::generateFullReport();
        EXPECT_FALSE(report.empty());
    });

    EXPECT_NO_THROW({
        std::string gpu = SystemInfoPrinter::formatGpuInfo();
        EXPECT_FALSE(gpu.empty());
    });
}

}  // namespace atom::sysinfo::test
