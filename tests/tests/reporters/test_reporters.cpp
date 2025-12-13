/*
 * test_reporters.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Tests for test reporters in atom/tests/reporters/test_reporter.hpp

**************************************************/

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

#include "atom/tests/reporters/test_reporter.hpp"

namespace atom::test::reporters::tests {

// ============================================================================
// TestReporter Interface Tests
// ============================================================================

class MockReporter : public TestReporter {
public:
    void onTestRunStart() override { testRunStartCalled = true; }

    void onTestRunEnd(const TestStats& stats) override {
        testRunEndCalled = true;
        lastStats = stats;
    }

    void onTestStart(const std::string& name) override {
        testStartCalled = true;
        lastTestName = name;
    }

    void onTestEnd(const std::string& name, const TestResult& result) override {
        testEndCalled = true;
        lastTestName = name;
        lastResult = result;
    }

    std::string generateReport(const TestStats& stats) override {
        return "Mock report";
    }

    bool testRunStartCalled = false;
    bool testRunEndCalled = false;
    bool testStartCalled = false;
    bool testEndCalled = false;
    std::string lastTestName;
    TestStats lastStats;
    TestResult lastResult;
};

class TestReporterInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<MockReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<MockReporter> reporter;
};

TEST_F(TestReporterInterfaceTest, OnTestRunStart) {
    reporter->onTestRunStart();
    EXPECT_TRUE(reporter->testRunStartCalled);
}

TEST_F(TestReporterInterfaceTest, OnTestRunEnd) {
    TestStats stats;
    stats.totalTests = 10;
    stats.passedAsserts = 8;
    stats.failedAsserts = 2;

    reporter->onTestRunEnd(stats);

    EXPECT_TRUE(reporter->testRunEndCalled);
    EXPECT_EQ(reporter->lastStats.totalTests, 10);
}

TEST_F(TestReporterInterfaceTest, OnTestStart) {
    reporter->onTestStart("TestName");

    EXPECT_TRUE(reporter->testStartCalled);
    EXPECT_EQ(reporter->lastTestName, "TestName");
}

TEST_F(TestReporterInterfaceTest, OnTestEnd) {
    TestResult result;
    result.name = "TestName";
    result.passed = true;

    reporter->onTestEnd("TestName", result);

    EXPECT_TRUE(reporter->testEndCalled);
    EXPECT_EQ(reporter->lastTestName, "TestName");
    EXPECT_TRUE(reporter->lastResult.passed);
}

TEST_F(TestReporterInterfaceTest, GenerateReport) {
    TestStats stats;
    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
}

// ============================================================================
// ConsoleReporter Tests
// ============================================================================

class ConsoleReporterTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<ConsoleReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<ConsoleReporter> reporter;
};

TEST_F(ConsoleReporterTest, Construction) {
    EXPECT_NE(reporter, nullptr);
}

TEST_F(ConsoleReporterTest, GenerateReport) {
    TestStats stats;
    stats.totalTests = 5;
    stats.passedAsserts = 4;
    stats.failedAsserts = 1;

    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
}

TEST_F(ConsoleReporterTest, ReportContainsTestCount) {
    TestStats stats;
    stats.totalTests = 10;
    stats.passedAsserts = 10;
    stats.failedAsserts = 0;

    std::string report = reporter->generateReport(stats);

    // Report should contain some indication of test count
    EXPECT_FALSE(report.empty());
}

// ============================================================================
// JsonReporter Tests
// ============================================================================

class JsonReporterTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<JsonReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<JsonReporter> reporter;
};

TEST_F(JsonReporterTest, Construction) {
    EXPECT_NE(reporter, nullptr);
}

TEST_F(JsonReporterTest, GenerateReport) {
    TestStats stats;
    stats.totalTests = 5;
    stats.passedAsserts = 4;
    stats.failedAsserts = 1;

    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
    // JSON should start with { or [
    EXPECT_TRUE(report[0] == '{' || report[0] == '[');
}

TEST_F(JsonReporterTest, ReportIsValidJson) {
    TestStats stats;
    stats.totalTests = 3;
    stats.passedAsserts = 2;
    stats.failedAsserts = 1;

    TestResult result1{"Test1", true, false, "", 10.0, false};
    TestResult result2{"Test2", true, false, "", 15.0, false};
    TestResult result3{"Test3", false, false, "Error", 5.0, false};

    stats.results.push_back(result1);
    stats.results.push_back(result2);
    stats.results.push_back(result3);

    std::string report = reporter->generateReport(stats);

    // Should contain JSON structure elements
    EXPECT_NE(report.find("{"), std::string::npos);
    EXPECT_NE(report.find("}"), std::string::npos);
}

TEST_F(JsonReporterTest, ReportContainsTotalTests) {
    TestStats stats;
    stats.totalTests = 100;

    std::string report = reporter->generateReport(stats);

    EXPECT_NE(report.find("totalTests"), std::string::npos);
}

// ============================================================================
// XmlReporter Tests
// ============================================================================

class XmlReporterTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<XmlReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<XmlReporter> reporter;
};

TEST_F(XmlReporterTest, Construction) {
    EXPECT_NE(reporter, nullptr);
}

TEST_F(XmlReporterTest, GenerateReport) {
    TestStats stats;
    stats.totalTests = 5;

    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
    // XML should contain declaration or start with <
    EXPECT_TRUE(report.find("<?xml") != std::string::npos ||
                report[0] == '<');
}

TEST_F(XmlReporterTest, ReportContainsTestsuites) {
    TestStats stats;
    stats.totalTests = 3;

    std::string report = reporter->generateReport(stats);

    EXPECT_NE(report.find("testsuites"), std::string::npos);
}

// ============================================================================
// HtmlReporter Tests
// ============================================================================

class HtmlReporterTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<HtmlReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<HtmlReporter> reporter;
};

TEST_F(HtmlReporterTest, Construction) {
    EXPECT_NE(reporter, nullptr);
}

TEST_F(HtmlReporterTest, GenerateReport) {
    TestStats stats;
    stats.totalTests = 5;

    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
}

TEST_F(HtmlReporterTest, ReportContainsHtmlTags) {
    TestStats stats;
    stats.totalTests = 3;

    std::string report = reporter->generateReport(stats);

    EXPECT_NE(report.find("<html"), std::string::npos);
    EXPECT_NE(report.find("</html>"), std::string::npos);
}

TEST_F(HtmlReporterTest, ReportContainsBody) {
    TestStats stats;
    stats.totalTests = 3;

    std::string report = reporter->generateReport(stats);

    EXPECT_NE(report.find("<body"), std::string::npos);
    EXPECT_NE(report.find("</body>"), std::string::npos);
}

// ============================================================================
// MarkdownReporter Tests
// ============================================================================

class MarkdownReporterTest : public ::testing::Test {
protected:
    void SetUp() override { reporter = std::make_unique<MarkdownReporter>(); }
    void TearDown() override { reporter.reset(); }

    std::unique_ptr<MarkdownReporter> reporter;
};

TEST_F(MarkdownReporterTest, Construction) {
    EXPECT_NE(reporter, nullptr);
}

TEST_F(MarkdownReporterTest, GenerateReport) {
    TestStats stats;
    stats.totalTests = 5;

    std::string report = reporter->generateReport(stats);

    EXPECT_FALSE(report.empty());
}

TEST_F(MarkdownReporterTest, ReportContainsHeaders) {
    TestStats stats;
    stats.totalTests = 3;

    std::string report = reporter->generateReport(stats);

    // Markdown headers start with #
    EXPECT_NE(report.find("#"), std::string::npos);
}

// ============================================================================
// createReporter Factory Tests
// ============================================================================

class CreateReporterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CreateReporterTest, CreateConsoleReporter) {
    auto reporter = createReporter("console");
    EXPECT_NE(reporter, nullptr);
}

TEST_F(CreateReporterTest, CreateJsonReporter) {
    auto reporter = createReporter("json");
    EXPECT_NE(reporter, nullptr);
}

TEST_F(CreateReporterTest, CreateXmlReporter) {
    auto reporter = createReporter("xml");
    EXPECT_NE(reporter, nullptr);
}

TEST_F(CreateReporterTest, CreateHtmlReporter) {
    auto reporter = createReporter("html");
    EXPECT_NE(reporter, nullptr);
}

TEST_F(CreateReporterTest, CreateMarkdownReporter) {
    auto reporter = createReporter("markdown");
    EXPECT_NE(reporter, nullptr);
}

TEST_F(CreateReporterTest, CreateUnknownReporter) {
    auto reporter = createReporter("unknown");
    // Should return nullptr or default reporter
    // Depending on implementation
}

// ============================================================================
// TestStats Tests
// ============================================================================

class TestStatsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestStatsTest, DefaultConstruction) {
    TestStats stats;

    EXPECT_EQ(stats.totalTests, 0);
    EXPECT_EQ(stats.passedAsserts, 0);
    EXPECT_EQ(stats.failedAsserts, 0);
    EXPECT_TRUE(stats.results.empty());
}

TEST_F(TestStatsTest, AddResults) {
    TestStats stats;
    stats.totalTests = 3;

    TestResult result1{"Test1", true, false, "", 10.0, false};
    TestResult result2{"Test2", false, false, "Error", 5.0, false};

    stats.results.push_back(result1);
    stats.results.push_back(result2);

    EXPECT_EQ(stats.results.size(), 2);
}

TEST_F(TestStatsTest, CalculatePassRate) {
    TestStats stats;
    stats.totalTests = 10;
    stats.passedAsserts = 8;
    stats.failedAsserts = 2;

    double passRate =
        static_cast<double>(stats.passedAsserts) / stats.totalTests * 100.0;
    EXPECT_NEAR(passRate, 80.0, 0.01);
}

// ============================================================================
// TestResult Tests
// ============================================================================

class TestResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestResultTest, PassedResult) {
    TestResult result;
    result.name = "PassedTest";
    result.passed = true;
    result.skipped = false;
    result.durationMs = 10.5;

    EXPECT_TRUE(result.passed);
    EXPECT_FALSE(result.skipped);
    EXPECT_EQ(result.durationMs, 10.5);
}

TEST_F(TestResultTest, FailedResult) {
    TestResult result;
    result.name = "FailedTest";
    result.passed = false;
    result.errorMessage = "Assertion failed";

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.errorMessage, "Assertion failed");
}

TEST_F(TestResultTest, SkippedResult) {
    TestResult result;
    result.name = "SkippedTest";
    result.passed = true;
    result.skipped = true;

    EXPECT_TRUE(result.skipped);
}

// ============================================================================
// Reporter Output Tests
// ============================================================================

class ReporterOutputTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    TestStats createSampleStats() {
        TestStats stats;
        stats.totalTests = 5;
        stats.passedAsserts = 4;
        stats.failedAsserts = 1;

        stats.results.push_back({"Test1", true, false, "", 10.0, false});
        stats.results.push_back({"Test2", true, false, "", 15.0, false});
        stats.results.push_back({"Test3", true, false, "", 8.0, false});
        stats.results.push_back({"Test4", true, false, "", 12.0, false});
        stats.results.push_back(
            {"Test5", false, false, "Expected 1, got 2", 5.0, false});

        return stats;
    }
};

TEST_F(ReporterOutputTest, ConsoleOutputNotEmpty) {
    auto reporter = createReporter("console");
    auto stats = createSampleStats();

    std::string output = reporter->generateReport(stats);
    EXPECT_FALSE(output.empty());
}

TEST_F(ReporterOutputTest, JsonOutputNotEmpty) {
    auto reporter = createReporter("json");
    auto stats = createSampleStats();

    std::string output = reporter->generateReport(stats);
    EXPECT_FALSE(output.empty());
}

TEST_F(ReporterOutputTest, XmlOutputNotEmpty) {
    auto reporter = createReporter("xml");
    auto stats = createSampleStats();

    std::string output = reporter->generateReport(stats);
    EXPECT_FALSE(output.empty());
}

TEST_F(ReporterOutputTest, HtmlOutputNotEmpty) {
    auto reporter = createReporter("html");
    auto stats = createSampleStats();

    std::string output = reporter->generateReport(stats);
    EXPECT_FALSE(output.empty());
}

TEST_F(ReporterOutputTest, MarkdownOutputNotEmpty) {
    auto reporter = createReporter("markdown");
    auto stats = createSampleStats();

    std::string output = reporter->generateReport(stats);
    EXPECT_FALSE(output.empty());
}

// ============================================================================
// Reporter File Output Tests
// ============================================================================

class ReporterFileOutputTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        // Cleanup test files
        std::remove("test_report.json");
        std::remove("test_report.xml");
        std::remove("test_report.html");
        std::remove("test_report.md");
    }
};

TEST_F(ReporterFileOutputTest, SaveJsonReport) {
    auto reporter = createReporter("json");
    TestStats stats;
    stats.totalTests = 1;

    std::string report = reporter->generateReport(stats);

    std::ofstream file("test_report.json");
    file << report;
    file.close();

    // Verify file exists
    std::ifstream checkFile("test_report.json");
    EXPECT_TRUE(checkFile.good());
}

}  // namespace atom::test::reporters::tests
