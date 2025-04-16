#ifndef ATOM_TEST_REPORTER_HPP
#define ATOM_TEST_REPORTER_HPP

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "atom/type/json.hpp"
#endif

#include "atom/tests/test.hpp"
#include "atom/utils/color_print.hpp"

namespace atom::test {

/**
 * @brief Interface for test reporters
 * @details Defines the essential functionality that all test reporters must
 * implement
 */
class TestReporter {
public:
    /**
     * @brief Virtual destructor ensuring proper cleanup of derived classes
     */
    virtual ~TestReporter() = default;

    /**
     * @brief Called before test execution begins
     * @param totalTests The total number of test cases to be executed
     */
    virtual void onTestRunStart(int totalTests) = 0;

    /**
     * @brief Called after all tests have completed
     * @param stats Statistics collected during the test run
     */
    virtual void onTestRunEnd(const TestStats& stats) = 0;

    /**
     * @brief Called before executing an individual test case
     * @param testCase The test case about to be executed
     */
    virtual void onTestStart(const TestCase& testCase) = 0;

    /**
     * @brief Called after an individual test case completes
     * @param result The result of the completed test case
     */
    virtual void onTestEnd(const TestResult& result) = 0;

    /**
     * @brief Generates the final test report
     * @param stats Complete statistics from the test run
     * @param outputPath Path where the report should be saved
     */
    virtual void generateReport(const TestStats& stats,
                                const std::string& outputPath) = 0;
};

/**
 * @brief Console-based test reporter
 * @details Displays test progress and results in real-time on the console
 */
class ConsoleReporter : public TestReporter {
public:
    /**
     * @brief Notifies the start of test execution
     * @param totalTests The total number of test cases to be executed
     */
    void onTestRunStart(int totalTests) override {
        std::cout << "Starting execution of " << totalTests
                  << " test cases...\n";
        std::cout << "======================================================\n";
    }

    /**
     * @brief Displays the final test summary
     * @param stats Statistics collected during the test run
     */
    void onTestRunEnd(const TestStats& stats) override {
        std::cout << "======================================================\n";
        std::cout << "Tests completed: " << stats.totalTests << " tests, "
                  << stats.passedAsserts << " passed assertions, "
                  << stats.failedAsserts << " failed assertions, "
                  << stats.skippedTests << " skipped tests\n";

        if (stats.failedAsserts > 0) {
            std::cout << "\nFailed tests:\n";
            for (const auto& result : stats.results) {
                if (!result.passed && !result.skipped) {
                    std::cout << "- " << result.name << ": " << result.message
                              << "\n";
                }
            }
        }
    }

    /**
     * @brief Notifies the start of a specific test case
     * @param testCase The test case about to be executed
     */
    void onTestStart(const TestCase& testCase) override {
        std::cout << "Executing test: " << testCase.name << " ... ";
        std::cout.flush();
    }

    /**
     * @brief Displays the result of an individual test case
     * @param result The result of the completed test case
     */
    void onTestEnd(const TestResult& result) override {
        if (result.skipped) {
            ColorPrinter::printColored("SKIPPED", ColorCode::Yellow);
        } else if (result.passed) {
            ColorPrinter::printColored("PASSED", ColorCode::Green);
        } else {
            ColorPrinter::printColored("FAILED", ColorCode::Red);
        }

        std::cout << " (" << result.duration << " ms)";

        if (!result.passed && !result.skipped) {
            std::cout << "\n    Error: " << result.message;
        }

        std::cout << "\n";
    }

    /**
     * @brief Generates a report (no-op for console reporter)
     * @param stats Statistics collected during the test run
     * @param outputPath Path where the report should be saved (ignored)
     */
    void generateReport(const TestStats& stats,
                        const std::string& outputPath) override {
        // Console reporter displays results in real-time; no file report needed
        (void)stats;
        (void)outputPath;
    }
};

/**
 * @brief JSON format test reporter
 * @details Generates a comprehensive test report in JSON format
 */
class JsonReporter : public TestReporter {
public:
    /**
     * @brief Handles test run start event (no-op)
     * @param totalTests The total number of test cases (unused)
     */
    void onTestRunStart(int totalTests) override { (void)totalTests; }

    /**
     * @brief Handles test run end event (no-op)
     * @param stats Statistics collected during the test run (unused)
     */
    void onTestRunEnd(const TestStats& stats) override { (void)stats; }

    /**
     * @brief Handles test case start event (no-op)
     * @param testCase The test case about to be executed (unused)
     */
    void onTestStart(const TestCase& testCase) override { (void)testCase; }

    /**
     * @brief Records the result of a completed test case
     * @param result The result of the completed test case
     */
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }

    /**
     * @brief Generates a JSON format test report
     * @param stats Statistics collected during the test run
     * @param outputPath Path where the report should be saved
     */
    void generateReport(const TestStats& stats,
                        const std::string& outputPath) override {
        nlohmann::json report;
        report["total_tests"] = stats.totalTests;
        report["total_asserts"] = stats.totalAsserts;
        report["passed_asserts"] = stats.passedAsserts;
        report["failed_asserts"] = stats.failedAsserts;
        report["skipped_tests"] = stats.skippedTests;

        report["results"] = nlohmann::json::array();
        for (const auto& result : results_) {
            nlohmann::json jsonResult;
            jsonResult["name"] = result.name;
            jsonResult["passed"] = result.passed;
            jsonResult["skipped"] = result.skipped;
            jsonResult["message"] = result.message;
            jsonResult["duration"] = result.duration;
            jsonResult["timed_out"] = result.timedOut;
            report["results"].push_back(jsonResult);
        }

        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.json";
        }

        std::ofstream file(filePath);
        file << std::setw(4) << report << std::endl;
        std::cout << "JSON report saved to: " << filePath << std::endl;
    }

private:
    std::vector<TestResult> results_;  ///< Collected test results
};

/**
 * @brief XML format test reporter
 * @details Generates a test report in JUnit-compatible XML format
 */
class XmlReporter : public TestReporter {
public:
    /**
     * @brief Handles test run start event (no-op)
     * @param totalTests The total number of test cases (unused)
     */
    void onTestRunStart(int totalTests) override { (void)totalTests; }

    /**
     * @brief Handles test run end event (no-op)
     * @param stats Statistics collected during the test run (unused)
     */
    void onTestRunEnd(const TestStats& stats) override { (void)stats; }

    /**
     * @brief Handles test case start event (no-op)
     * @param testCase The test case about to be executed (unused)
     */
    void onTestStart(const TestCase& testCase) override { (void)testCase; }

    /**
     * @brief Records the result of a completed test case
     * @param result The result of the completed test case
     */
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }

    /**
     * @brief Generates an XML format test report
     * @param stats Statistics collected during the test run
     * @param outputPath Path where the report should be saved
     */
    void generateReport(const TestStats& stats,
                        const std::string& outputPath) override {
        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.xml";
        }

        std::ofstream file(filePath);
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<testsuites>\n";
        file << "    <testsuite name=\"AtomTests\" tests=\"" << stats.totalTests
             << "\" failures=\"" << stats.failedAsserts << "\" skipped=\""
             << stats.skippedTests << "\">\n";

        for (const auto& result : results_) {
            file << "        <testcase name=\"" << result.name << "\" time=\""
                 << (result.duration / 1000.0) << "\"";

            if (result.skipped) {
                file << ">\n            <skipped/>\n        </testcase>\n";
            } else if (!result.passed) {
                file << ">\n            <failure message=\"" << result.message
                     << "\"></failure>\n        </testcase>\n";
            } else {
                file << "/>\n";
            }
        }

        file << "    </testsuite>\n";
        file << "</testsuites>\n";

        std::cout << "XML report saved to: " << filePath << std::endl;
    }

private:
    std::vector<TestResult> results_;  ///< Collected test results
};

/**
 * @brief HTML format test reporter
 * @details Generates a visually appealing, human-readable test report in HTML
 * format
 */
class HtmlReporter : public TestReporter {
public:
    /**
     * @brief Handles test run start event (no-op)
     * @param totalTests The total number of test cases (unused)
     */
    void onTestRunStart(int totalTests) override { (void)totalTests; }

    /**
     * @brief Handles test run end event (no-op)
     * @param stats Statistics collected during the test run (unused)
     */
    void onTestRunEnd(const TestStats& stats) override { (void)stats; }

    /**
     * @brief Handles test case start event (no-op)
     * @param testCase The test case about to be executed (unused)
     */
    void onTestStart(const TestCase& testCase) override { (void)testCase; }

    /**
     * @brief Records the result of a completed test case
     * @param result The result of the completed test case
     */
    void onTestEnd(const TestResult& result) override {
        results_.push_back(result);
    }

    /**
     * @brief Generates an HTML format test report
     * @param stats Statistics collected during the test run
     * @param outputPath Path where the report should be saved
     */
    void generateReport(const TestStats& stats,
                        const std::string& outputPath) override {
        std::filesystem::path filePath(outputPath);
        if (std::filesystem::is_directory(filePath)) {
            filePath /= "test_report.html";
        }

        std::ofstream file(filePath);
        file << "<!DOCTYPE html>\n";
        file << "<html lang=\"en\">\n";
        file << "<head>\n";
        file << "    <meta charset=\"UTF-8\">\n";
        file << "    <meta name=\"viewport\" content=\"width=device-width, "
                "initial-scale=1.0\">\n";
        file << "    <title>Atom Test Report</title>\n";
        file << "    <style>\n";
        file << "        body { font-family: Arial, sans-serif; margin: 0; "
                "padding: 20px; }\n";
        file << "        h1 { color: #333; }\n";
        file << "        .summary { background-color: #f0f0f0; padding: 15px; "
                "border-radius: 5px; margin-bottom: 20px; }\n";
        file << "        .passed { color: green; }\n";
        file << "        .failed { color: red; }\n";
        file << "        .skipped { color: orange; }\n";
        file << "        table { width: 100%; border-collapse: collapse; }\n";
        file << "        th, td { text-align: left; padding: 8px; "
                "border-bottom: 1px solid #ddd; }\n";
        file << "        tr:hover { background-color: #f5f5f5; }\n";
        file << "        th { background-color: #4CAF50; color: white; }\n";
        file << "    </style>\n";
        file << "</head>\n";
        file << "<body>\n";
        file << "    <h1>Atom Test Report</h1>\n";

        file << "    <div class=\"summary\">\n";
        file << "        <h2>Test Summary</h2>\n";
        file << "        <p>Total Tests: " << stats.totalTests << "</p>\n";
        file << "        <p>Total Assertions: " << stats.totalAsserts
             << "</p>\n";
        file << "        <p>Passed Assertions: <span class=\"passed\">"
             << stats.passedAsserts << "</span></p>\n";
        file << "        <p>Failed Assertions: <span class=\"failed\">"
             << stats.failedAsserts << "</span></p>\n";
        file << "        <p>Skipped Tests: <span class=\"skipped\">"
             << stats.skippedTests << "</span></p>\n";
        file << "    </div>\n";

        file << "    <h2>Test Details</h2>\n";
        file << "    <table>\n";
        file << "        <tr>\n";
        file << "            <th>Test Name</th>\n";
        file << "            <th>Status</th>\n";
        file << "            <th>Duration (ms)</th>\n";
        file << "            <th>Message</th>\n";
        file << "        </tr>\n";

        for (const auto& result : results_) {
            file << "        <tr>\n";
            file << "            <td>" << result.name << "</td>\n";

            if (result.skipped) {
                file << "            <td><span "
                        "class=\"skipped\">SKIPPED</span></td>\n";
            } else if (result.passed) {
                file << "            <td><span "
                        "class=\"passed\">PASSED</span></td>\n";
            } else {
                file << "            <td><span "
                        "class=\"failed\">FAILED</span></td>\n";
            }

            file << "            <td>" << result.duration << "</td>\n";
            file << "            <td>" << result.message << "</td>\n";
            file << "        </tr>\n";
        }

        file << "    </table>\n";
        file << "</body>\n";
        file << "</html>\n";

        std::cout << "HTML report saved to: " << filePath << std::endl;
    }

private:
    std::vector<TestResult> results_;  ///< Collected test results
};

/**
 * @brief Factory function to create a specific reporter instance
 * @param format Report format identifier ("console", "json", "xml", "html")
 * @return Smart pointer to the appropriate reporter implementation
 */
[[nodiscard]] inline auto createReporter(const std::string& format)
    -> std::unique_ptr<TestReporter> {
    if (format == "json") {
        return std::make_unique<JsonReporter>();
    } else if (format == "xml") {
        return std::make_unique<XmlReporter>();
    } else if (format == "html") {
        return std::make_unique<HtmlReporter>();
    }
#if defined(ATOM_USE_PYBIND11)
    else if (format == "chart" || format == "charts") {
// Include the chart reporter only if PyBind11 is available
#include "atom/tests/test_reporter_charts.hpp"
        if (isChartReportingAvailable()) {
            return createChartReporter();
        } else {
            std::cerr << "Chart reporting is not available. Falling back to "
                         "HTML reporter."
                      << std::endl;
            return std::make_unique<HtmlReporter>();
        }
    }
#endif
    // Default to console reporter
    return std::make_unique<ConsoleReporter>();
}

}  // namespace atom::test

#endif  // ATOM_TEST_REPORTER_HPP