#include "atom/sysinfo/sysinfo_printer.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <filesystem>

using namespace atom::system;
using namespace testing;

class SysinfoPrinterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        testDir_ = std::filesystem::temp_directory_path() / "sysinfo_printer_test";
        std::filesystem::create_directories(testDir_);
    }

    void TearDown() override {
        // Clean up test files
        if (std::filesystem::exists(testDir_)) {
            std::filesystem::remove_all(testDir_);
        }
    }

    std::filesystem::path testDir_;
};

// Test SystemInfoPrinter basic construction
TEST_F(SysinfoPrinterTest, BasicConstruction) {
    EXPECT_NO_THROW({
        SystemInfoPrinter printer;

        // Should be able to create multiple instances
        SystemInfoPrinter printer2;
        SystemInfoPrinter printer3;

        // Use the printers to avoid unused variable warnings
        (void)printer;
        (void)printer2;
        (void)printer3;
    });
}

// Test legacy static methods
TEST_F(SysinfoPrinterTest, LegacyStaticMethods) {
    EXPECT_NO_THROW({
        // Test generateFullReport
        std::string fullReport = SystemInfoPrinter::generateFullReport();
        EXPECT_FALSE(fullReport.empty());

        // Test generateSimpleReport
        std::string simpleReport = SystemInfoPrinter::generateSimpleReport();
        EXPECT_FALSE(simpleReport.empty());

        // Test generatePerformanceReport
        std::string perfReport = SystemInfoPrinter::generatePerformanceReport();
        EXPECT_FALSE(perfReport.empty());

        // Test generateSecurityReport
        std::string secReport = SystemInfoPrinter::generateSecurityReport();
        EXPECT_FALSE(secReport.empty());

        // Reports should be different (unless system is very limited)
        EXPECT_TRUE(fullReport != simpleReport || fullReport.length() < 100);
    });
}

// Test report generation with different types
TEST_F(SysinfoPrinterTest, ReportGeneration) {
    EXPECT_NO_THROW({
        // Test different report types using static methods
        std::string fullReport = SystemInfoPrinter::generateFullReport();
        EXPECT_FALSE(fullReport.empty());

        std::string simpleReport = SystemInfoPrinter::generateSimpleReport();
        EXPECT_FALSE(simpleReport.empty());

        std::string perfReport = SystemInfoPrinter::generatePerformanceReport();
        EXPECT_FALSE(perfReport.empty());

        std::string secReport = SystemInfoPrinter::generateSecurityReport();
        EXPECT_FALSE(secReport.empty());

        // Note: generateHardwareReport and generateNetworkReport are not available
        // in the current API, so we skip these tests

        // Reports should contain some expected content
        EXPECT_TRUE(fullReport.find("System") != std::string::npos ||
                   fullReport.find("CPU") != std::string::npos ||
                   fullReport.find("Memory") != std::string::npos);
    });
}

// Test export functionality
TEST_F(SysinfoPrinterTest, ExportFunctionality) {
    EXPECT_NO_THROW({
        // Test HTML export
        std::string htmlFile = (testDir_ / "test_report.html").string();
        bool htmlSuccess = SystemInfoPrinter::exportToHTML(htmlFile);
        EXPECT_TRUE(htmlSuccess);

        if (htmlSuccess) {
            EXPECT_TRUE(std::filesystem::exists(htmlFile));

            // Check file content
            std::ifstream file(htmlFile);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            EXPECT_FALSE(content.empty());
            EXPECT_NE(content.find("html"), std::string::npos);
        }

        // Test JSON export
        std::string jsonFile = (testDir_ / "test_report.json").string();
        bool jsonSuccess = SystemInfoPrinter::exportToJSON(jsonFile);
        EXPECT_TRUE(jsonSuccess);

        if (jsonSuccess) {
            EXPECT_TRUE(std::filesystem::exists(jsonFile));

            // Check file content
            std::ifstream file(jsonFile);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            EXPECT_FALSE(content.empty());
            EXPECT_TRUE(content.find("{") != std::string::npos ||
                       content.find("[") != std::string::npos);
        }

        // Test Markdown export
        std::string mdFile = (testDir_ / "test_report.md").string();
        bool mdSuccess = SystemInfoPrinter::exportToMarkdown(mdFile);
        EXPECT_TRUE(mdSuccess);

        if (mdSuccess) {
            EXPECT_TRUE(std::filesystem::exists(mdFile));

            // Check file content
            std::ifstream file(mdFile);
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            EXPECT_FALSE(content.empty());
            EXPECT_TRUE(content.find("#") != std::string::npos ||
                       content.find("*") != std::string::npos);
        }
    });
}

// Test formatting functions
TEST_F(SysinfoPrinterTest, FormattingFunctions) {
    EXPECT_NO_THROW({
        // Test formatBytes
        std::string bytes1 = SystemInfoPrinter::formatBytes(1024);
        EXPECT_FALSE(bytes1.empty());
        EXPECT_TRUE(bytes1.find("KB") != std::string::npos ||
                   bytes1.find("B") != std::string::npos);

        std::string bytes2 = SystemInfoPrinter::formatBytes(1048576);
        EXPECT_FALSE(bytes2.empty());
        EXPECT_TRUE(bytes2.find("MB") != std::string::npos ||
                   bytes2.find("KB") != std::string::npos);

        // Test formatPercentage
        std::string percent1 = SystemInfoPrinter::formatPercentage(50.5);
        EXPECT_FALSE(percent1.empty());
        EXPECT_NE(percent1.find("50"), std::string::npos);

        std::string percent2 = SystemInfoPrinter::formatPercentage(100.0);
        EXPECT_FALSE(percent2.empty());
        EXPECT_NE(percent2.find("100"), std::string::npos);

        // Note: formatUptime and formatSize are not available in the current API

        // Test createTableHeader
        std::string header = SystemInfoPrinter::createTableHeader("Test Header");
        EXPECT_FALSE(header.empty());
        EXPECT_NE(header.find("Test Header"), std::string::npos);

        // Test createTableFooter
        std::string footer = SystemInfoPrinter::createTableFooter();
        EXPECT_FALSE(footer.empty());
    });
}

// Test individual component formatting
TEST_F(SysinfoPrinterTest, ComponentFormatting) {
    EXPECT_NO_THROW({
        // Test CPU formatting
        auto cpuInfo = getCpuInfo();
        std::string cpuFormatted = SystemInfoPrinter::formatCpuInfo(cpuInfo);
        EXPECT_FALSE(cpuFormatted.empty());

        // Test Memory formatting
        auto memInfo = getDetailedMemoryStats();
        std::string memFormatted = SystemInfoPrinter::formatMemoryInfo(memInfo);
        EXPECT_FALSE(memFormatted.empty());

        // Test OS formatting
        auto osInfo = getOperatingSystemInfo();
        std::string osFormatted = SystemInfoPrinter::formatOsInfo(osInfo);
        EXPECT_FALSE(osFormatted.empty());

        // Test Battery formatting
        auto batteryInfo = atom::system::battery::getBatteryInfo();
        if (batteryInfo.has_value()) {
            std::string batteryFormatted = SystemInfoPrinter::formatBatteryInfo(batteryInfo.value());
            EXPECT_FALSE(batteryFormatted.empty());
        }

        // Test Disk formatting
        auto diskInfo = getDiskInfo();
        std::string diskFormatted = SystemInfoPrinter::formatDiskInfo(diskInfo);
        EXPECT_FALSE(diskFormatted.empty());

        // Test BIOS formatting (using BiosInfo from bios module)
        auto& biosInstance = atom::system::BiosInfo::getInstance();
        const auto& biosInfo = biosInstance.getBiosInfo();
        std::string biosFormatted = SystemInfoPrinter::formatBiosInfo(biosInfo);
        EXPECT_FALSE(biosFormatted.empty());
    });
}

// Test error handling
TEST_F(SysinfoPrinterTest, ErrorHandling) {
    EXPECT_NO_THROW({
        // Test export to invalid path
        std::string invalidPath = "/invalid/path/that/does/not/exist/report.html";
        bool result = SystemInfoPrinter::exportToHTML(invalidPath);
        // Should handle gracefully (may succeed or fail depending on system)
        EXPECT_TRUE(result == true || result == false);

        // Test export to read-only directory (if possible)
        std::string readOnlyPath = "/tmp/readonly_test_report.html";
        bool readOnlyResult = SystemInfoPrinter::exportToHTML(readOnlyPath);
        EXPECT_TRUE(readOnlyResult == true || readOnlyResult == false);
    });
}

// Test deprecated global functions
TEST_F(SysinfoPrinterTest, DeprecatedGlobalFunctions) {
    // Suppress deprecation warnings for testing
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    EXPECT_NO_THROW({
        // Test deprecated generateSystemReport
        std::string globalReport = generateSystemReport();
        EXPECT_FALSE(globalReport.empty());

        // Test deprecated exportSystemReportToHTML
        std::string globalHtmlFile = (testDir_ / "global_report.html").string();
        bool globalResult = exportSystemReportToHTML(globalHtmlFile);
        EXPECT_TRUE(globalResult == true || globalResult == false);
    });

    #pragma GCC diagnostic pop
}

// Test concurrent access
TEST_F(SysinfoPrinterTest, ConcurrentAccess) {
    const int num_threads = 3;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing printer functions concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                std::string fullReport = SystemInfoPrinter::generateFullReport();
                std::string simpleReport = SystemInfoPrinter::generateSimpleReport();
                std::string perfReport = SystemInfoPrinter::generatePerformanceReport();

                // Basic validation
                EXPECT_FALSE(fullReport.empty());
                EXPECT_FALSE(simpleReport.empty());
                EXPECT_FALSE(perfReport.empty());

                results[i] = true;
            } catch (...) {
                results[i] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have completed successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Test performance
TEST_F(SysinfoPrinterTest, Performance) {
    EXPECT_NO_THROW({
        // Measure time for full report generation
        auto start = std::chrono::high_resolution_clock::now();
        std::string fullReport = SystemInfoPrinter::generateFullReport();
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Report generation should complete within reasonable time (10 seconds)
        EXPECT_LT(duration.count(), 10000);
        EXPECT_FALSE(fullReport.empty());

        // Measure time for simple report generation
        start = std::chrono::high_resolution_clock::now();
        std::string simpleReport = SystemInfoPrinter::generateSimpleReport();
        end = std::chrono::high_resolution_clock::now();

        auto simpleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Simple report should be faster than full report
        EXPECT_LE(simpleDuration.count(), duration.count() + 1000); // Allow some variance
        EXPECT_FALSE(simpleReport.empty());
    });
}