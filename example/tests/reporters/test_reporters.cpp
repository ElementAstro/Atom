/**
 * @file test_reporters.cpp
 * @brief Test reporter functionality in the Atom Test Framework
 */

#include "atom/tests/atom_test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace atom::test;

// ============================================================================
// Console Reporter Tests
// ============================================================================

TEST(ReporterTests, CreateConsoleReporter) {
    auto reporter = createReporter("console");
    expect_not_null(reporter.get());
}

TEST(ReporterTests, ConsoleReporterGeneratesOutput) {
    auto reporter = createReporter("console");

    TestStats stats;
    stats.totalTests = 10;
    stats.passedAsserts = 8;
    stats.failedAsserts = 2;
    stats.skippedTests = 0;

    stats.results.push_back({"Test1", true, false, "", 10.5, false});
    stats.results.push_back({"Test2", true, false, "", 20.3, false});
    stats.results.push_back({"Test3", false, false, "Assertion failed", 5.0, false});

    std::string report = reporter->generateReport(stats);
    expect_not_empty(report);
}

// ============================================================================
// JSON Reporter Tests
// ============================================================================

TEST(ReporterTests, CreateJsonReporter) {
    auto reporter = createReporter("json");
    expect_not_null(reporter.get());
}

TEST(ReporterTests, JsonReporterGeneratesValidJson) {
    auto reporter = createReporter("json");

    TestStats stats;
    stats.totalTests = 5;
    stats.passedAsserts = 4;
    stats.failedAsserts = 1;
    stats.skippedTests = 0;

    stats.results.push_back({"JsonTest1", true, false, "", 15.0, false});
    stats.results.push_back({"JsonTest2", false, false, "Error", 10.0, false});

    std::string report = reporter->generateReport(stats);

    expect_not_empty(report);
    expect_contains(report, "totalTests");
    expect_contains(report, "passedAsserts");
    expect_contains(report, "results");
}

// ============================================================================
// XML Reporter Tests
// ============================================================================

TEST(ReporterTests, CreateXmlReporter) {
    auto reporter = createReporter("xml");
    expect_not_null(reporter.get());
}

TEST(ReporterTests, XmlReporterGeneratesValidXml) {
    auto reporter = createReporter("xml");

    TestStats stats;
    stats.totalTests = 3;
    stats.passedAsserts = 2;
    stats.failedAsserts = 1;
    stats.skippedTests = 0;

    stats.results.push_back({"XmlTest1", true, false, "", 5.0, false});
    stats.results.push_back({"XmlTest2", true, false, "", 8.0, false});
    stats.results.push_back({"XmlTest3", false, false, "Failed", 3.0, false});

    std::string report = reporter->generateReport(stats);

    expect_not_empty(report);
    expect_contains(report, "<?xml");
    expect_contains(report, "<testsuites>");
    expect_contains(report, "</testsuites>");
}

// ============================================================================
// HTML Reporter Tests
// ============================================================================

TEST(ReporterTests, CreateHtmlReporter) {
    auto reporter = createReporter("html");
    expect_not_null(reporter.get());
}

TEST(ReporterTests, HtmlReporterGeneratesValidHtml) {
    auto reporter = createReporter("html");

    TestStats stats;
    stats.totalTests = 4;
    stats.passedAsserts = 3;
    stats.failedAsserts = 1;
    stats.skippedTests = 0;

    stats.results.push_back({"HtmlTest1", true, false, "", 12.0, false});
    stats.results.push_back({"HtmlTest2", true, false, "", 8.0, false});
    stats.results.push_back({"HtmlTest3", true, false, "", 15.0, false});
    stats.results.push_back({"HtmlTest4", false, false, "Error", 5.0, false});

    std::string report = reporter->generateReport(stats);

    expect_not_empty(report);
    expect_contains(report, "<!DOCTYPE html>");
    expect_contains(report, "<html>");
    expect_contains(report, "</html>");
}

// ============================================================================
// Markdown Reporter Tests
// ============================================================================

TEST(ReporterTests, CreateMarkdownReporter) {
    auto reporter = createReporter("markdown");
    expect_not_null(reporter.get());

    auto reporter2 = createReporter("md");
    expect_not_null(reporter2.get());
}

TEST(ReporterTests, MarkdownReporterGeneratesValidMarkdown) {
    auto reporter = createReporter("markdown");

    TestStats stats;
    stats.totalTests = 5;
    stats.passedAsserts = 4;
    stats.failedAsserts = 1;
    stats.skippedTests = 0;

    stats.results.push_back({"MdTest1", true, false, "", 10.0, false});
    stats.results.push_back({"MdTest2", true, false, "", 20.0, false});
    stats.results.push_back({"MdTest3", true, false, "", 15.0, false});
    stats.results.push_back({"MdTest4", true, false, "", 8.0, false});
    stats.results.push_back({"MdTest5", false, false, "Assertion failed", 5.0, false});

    std::string report = reporter->generateReport(stats);

    expect_not_empty(report);
    expect_contains(report, "# Test Report");
    expect_contains(report, "## Summary");
    expect_contains(report, "|");  // Table format
}

// ============================================================================
// Reporter Factory Tests
// ============================================================================

TEST(ReporterTests, FactoryCreatesCorrectReporters) {
    auto console = createReporter("console");
    auto json = createReporter("json");
    auto xml = createReporter("xml");
    auto html = createReporter("html");
    auto markdown = createReporter("markdown");

    expect_not_null(console.get());
    expect_not_null(json.get());
    expect_not_null(xml.get());
    expect_not_null(html.get());
    expect_not_null(markdown.get());
}

TEST(ReporterTests, FactoryDefaultsToConsole) {
    auto unknown = createReporter("unknown_format");
    expect_not_null(unknown.get());
}

// ============================================================================
// Reporter with Skipped Tests
// ============================================================================

TEST(ReporterTests, ReporterHandlesSkippedTests) {
    auto reporter = createReporter("json");

    TestStats stats;
    stats.totalTests = 5;
    stats.passedAsserts = 2;
    stats.failedAsserts = 1;
    stats.skippedTests = 2;

    stats.results.push_back({"Test1", true, false, "", 10.0, false});
    stats.results.push_back({"Test2", true, false, "", 15.0, false});
    stats.results.push_back({"Test3", false, false, "Failed", 5.0, false});
    stats.results.push_back({"Test4", false, true, "Skipped", 0.0, false});
    stats.results.push_back({"Test5", false, true, "Platform not supported", 0.0, false});

    std::string report = reporter->generateReport(stats);

    expect_contains(report, "skippedTests");
}

// ============================================================================
// Reporter with Timed Out Tests
// ============================================================================

TEST(ReporterTests, ReporterHandlesTimedOutTests) {
    auto reporter = createReporter("json");

    TestStats stats;
    stats.totalTests = 3;
    stats.passedAsserts = 1;
    stats.failedAsserts = 2;
    stats.skippedTests = 0;

    stats.results.push_back({"Test1", true, false, "", 10.0, false});
    stats.results.push_back({"Test2", false, false, "Timed out", 5000.0, true});
    stats.results.push_back({"Test3", false, false, "Timed out", 3000.0, true});

    std::string report = reporter->generateReport(stats);

    expect_not_empty(report);
}

// ============================================================================
// Reporter Lifecycle Tests
// ============================================================================

TEST(ReporterTests, ReporterLifecycle) {
    auto reporter = createReporter("console");

    TestStats stats;
    stats.totalTests = 2;
    stats.passedAsserts = 2;
    stats.failedAsserts = 0;
    stats.skippedTests = 0;

    reporter->onTestRunStart();

    reporter->onTestStart("Test1");
    reporter->onTestEnd("Test1", true, 10.0);

    reporter->onTestStart("Test2");
    reporter->onTestEnd("Test2", true, 15.0);

    reporter->onTestRunEnd(stats);

    std::string report = reporter->generateReport(stats);
    expect_not_empty(report);
}

// ============================================================================
// Write Report to File Tests
// ============================================================================

TEST(ReporterTests, WriteReportToFile) {
    auto reporter = createReporter("json");

    TestStats stats;
    stats.totalTests = 2;
    stats.passedAsserts = 2;
    stats.failedAsserts = 0;
    stats.skippedTests = 0;

    stats.results.push_back({"FileTest1", true, false, "", 10.0, false});
    stats.results.push_back({"FileTest2", true, false, "", 15.0, false});

    std::string report = reporter->generateReport(stats);

    // Write to temp file
    std::filesystem::path tempPath =
        std::filesystem::temp_directory_path() / "test_report.json";

    std::ofstream file(tempPath);
    expect_true(file.is_open());
    file << report;
    file.close();

    // Verify file exists and has content
    expect_true(std::filesystem::exists(tempPath));
    expect_gt(std::filesystem::file_size(tempPath), 0);

    // Cleanup
    std::filesystem::remove(tempPath);
}

// ============================================================================
// Multiple Reports Test
// ============================================================================

TEST(ReporterTests, GenerateMultipleFormats) {
    TestStats stats;
    stats.totalTests = 3;
    stats.passedAsserts = 2;
    stats.failedAsserts = 1;
    stats.skippedTests = 0;

    stats.results.push_back({"MultiTest1", true, false, "", 10.0, false});
    stats.results.push_back({"MultiTest2", true, false, "", 15.0, false});
    stats.results.push_back({"MultiTest3", false, false, "Error", 5.0, false});

    std::vector<std::string> formats = {"console", "json", "xml", "html", "markdown"};

    for (const auto& format : formats) {
        auto reporter = createReporter(format);
        std::string report = reporter->generateReport(stats);
        expect_not_empty(report);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    return runAllTests(argc, argv);
}
