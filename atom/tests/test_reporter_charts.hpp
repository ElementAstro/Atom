#ifndef ATOM_TEST_REPORTER_CHARTS_HPP
#define ATOM_TEST_REPORTER_CHARTS_HPP

#if defined(ATOM_USE_PYBIND11)

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/tests/test.hpp"
#include "atom/tests/test_reporter.hpp"

namespace atom::test {

namespace py = pybind11;

/**
 * @brief Style options for chart generation
 */
enum class ChartStyle { Default, Seaborn, Ggplot, Minimal };

/**
 * @brief Type of charts to generate
 */
enum class ChartType { Bar, Line, Scatter, Pie, Histogram, Heatmap, All };

/**
 * @brief Configuration for chart generation
 * @details Controls the visual appearance and type of generated charts
 */
struct ChartConfig {
    ChartStyle style{ChartStyle::Default};
    bool darkMode{false};
    bool showTrendLine{false};
    bool interactive{false};
    std::string outputDirectory{"test_report_charts"};
    std::vector<std::string> metrics{"duration", "passRate"};
    ChartType chartType{ChartType::All};
    bool generateReport{true};
};

/**
 * @brief Chart-based test reporter
 * @details Generates visual charts and graphs using Python's matplotlib/seaborn
 */
class ChartReporter : public TestReporter {
public:
    /**
     * @brief Constructs a ChartReporter with the specified configuration
     * @param config Configuration options for chart generation
     */
    explicit ChartReporter(ChartConfig config = {})
        : config_(std::move(config)) {
        initPython();
    }

    /**
     * @brief Destructor for proper cleanup of Python interpreter
     */
    ~ChartReporter() override {
        if (pyInitialized_) {
            try {
                // Cleanup Python resources if needed
                py::finalize_interpreter();
            } catch (const std::exception& e) {
                std::cerr << "Error during Python finalization: " << e.what()
                          << std::endl;
            }
        }
    }

    /**
     * @brief Handles test run start event
     * @param totalTests The total number of test cases
     */
    void onTestRunStart(int totalTests) override {
        totalTestCount_ = totalTests;
        startTime_ = std::chrono::high_resolution_clock::now();
        suiteData_.clear();
    }

    /**
     * @brief Handles test run end event
     * @param stats Statistics collected during the test run
     */
    void onTestRunEnd(const TestStats& stats) override {
        endTime_ = std::chrono::high_resolution_clock::now();
        stats_ = stats;
    }

    /**
     * @brief Handles test case start event
     * @param testCase The test case about to be executed
     */
    void onTestStart(const TestCase& testCase) override {
        currentSuite_ = extractSuiteName(testCase.name);
        currentTest_ = testCase.name;
        testStartTime_ = std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Records the result of a completed test case
     * @param result The result of the completed test case
     */
    void onTestEnd(const TestResult& result) override {
        auto testEndTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            testEndTime - testStartTime_)
                            .count();

        // Prepare test result data for charting
        nlohmann::json testData;
        testData["name"] = result.name;
        testData["duration"] = result.duration;
        testData["passed"] = result.passed;
        testData["skipped"] = result.skipped;
        testData["message"] = result.message;
        testData["timedOut"] = result.timedOut;

        // Store in suite data structure
        if (!suiteData_.contains(currentSuite_)) {
            suiteData_[currentSuite_] = nlohmann::json::array();
        }
        suiteData_[currentSuite_].push_back(testData);
    }

    /**
     * @brief Generates charts and visual reports
     * @param stats Complete statistics from the test run
     * @param outputPath Path where the report should be saved
     */
    void generateReport(const TestStats& stats,
                        const std::string& outputPath) override {
        if (!pyInitialized_) {
            std::cerr << "Python interpreter not initialized, skipping chart "
                         "generation"
                      << std::endl;
            return;
        }

        try {
            // Create the output directory if it doesn't exist
            std::filesystem::path basePath(outputPath);
            if (std::filesystem::is_directory(basePath)) {
                basePath /= config_.outputDirectory;
            } else {
                basePath = std::filesystem::path(outputPath).parent_path() /
                           config_.outputDirectory;
            }
            std::filesystem::create_directories(basePath);

            // Calculate derived metrics
            calculateDerivedMetrics();

            // Save test data as JSON for Python
            std::string dataFilePath = (basePath / "test_data.json").string();
            std::ofstream dataFile(dataFilePath);
            dataFile << std::setw(4) << suiteData_ << std::endl;
            dataFile.close();

            // Call Python to generate charts
            generateCharts(dataFilePath, basePath.string());

            std::cout << "Charts and visual report generated in: " << basePath
                      << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error generating charts: " << e.what() << std::endl;
        }
    }

private:
    ChartConfig config_;
    bool pyInitialized_{false};
    int totalTestCount_{0};
    std::string currentSuite_;
    std::string currentTest_;
    std::chrono::high_resolution_clock::time_point startTime_;
    std::chrono::high_resolution_clock::time_point endTime_;
    std::chrono::high_resolution_clock::time_point testStartTime_;
    nlohmann::json suiteData_;
    TestStats stats_;

    /**
     * @brief Initialize Python interpreter and import required modules
     */
    void initPython() {
        try {
            py::initialize_interpreter();
            pyInitialized_ = true;

            // Import necessary Python modules
            auto sys = py::module::import("sys");
            auto os = py::module::import("os");

            // Add current directory to Python path to find charts.py
            py::str currentDir = py::str(".");
            sys.attr("path").attr("append")(currentDir);

            // Check if matplotlib is installed
            try {
                py::module::import("matplotlib");
            } catch (const std::exception&) {
                std::cerr << "Warning: matplotlib is not installed. Charts "
                             "cannot be generated."
                          << std::endl;
                pyInitialized_ = false;
                py::finalize_interpreter();
                return;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to initialize Python interpreter: " << e.what()
                      << std::endl;
            pyInitialized_ = false;
        }
    }

    /**
     * @brief Extract the suite name from the full test name
     * @param fullTestName Full test name, usually in format
     * "SuiteName.TestName"
     * @return The suite name portion
     */
    static std::string extractSuiteName(const std::string& fullTestName) {
        size_t dotPos = fullTestName.find('.');
        if (dotPos != std::string::npos) {
            return fullTestName.substr(0, dotPos);
        }
        return "DefaultSuite";
    }

    /**
     * @brief Calculate additional metrics from test results
     */
    void calculateDerivedMetrics() {
        for (auto& [suite, tests] : suiteData_.items()) {
            int passCount = 0;
            int totalTests = tests.size();

            for (auto& test : tests) {
                if (test["passed"].get<bool>() &&
                    !test["skipped"].get<bool>()) {
                    passCount++;
                }
            }

            // Add pass rate and other metrics to each test
            for (auto& test : tests) {
                test["passRate"] =
                    (totalTests > 0)
                        ? (static_cast<double>(passCount) / totalTests) * 100.0
                        : 0.0;
            }
        }
    }

    /**
     * @brief Call Python to generate charts from test data
     * @param dataFilePath Path to the JSON data file
     * @param outputDir Directory where charts should be saved
     */
    void generateCharts(const std::string& dataFilePath,
                        const std::string& outputDir) {
        try {
            // Import the charts module
            auto chartsModule = py::module::import("atom.tests.charts");

            // Create a ChartGenerator object
            auto generator = chartsModule.attr("ChartGenerator")(
                py::none(), dataFilePath, convertStyleToString(config_.style),
                config_.darkMode);

            // Configure chart generation
            if (config_.generateReport) {
                generator.attr("generate_report")(config_.metrics, outputDir);
            } else {
                // Generate specific chart types
                generateSpecificCharts(generator, outputDir);
            }
        } catch (const py::error_already_set& e) {
            std::cerr << "Python error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error generating charts: " << e.what() << std::endl;
        }
    }

    /**
     * @brief Generate specific chart types based on configuration
     * @param generator Python ChartGenerator object
     * @param outputDir Directory where charts should be saved
     */
    void generateSpecificCharts(const py::object& generator,
                                const std::string& outputDir) {
        py::dict kwargs;
        kwargs["show"] = config_.interactive;
        kwargs["trend_line"] = config_.showTrendLine;

        for (const auto& metric : config_.metrics) {
            std::string outputFile = outputDir + "/" + metric;

            switch (config_.chartType) {
                case ChartType::Bar:
                    generator.attr("bar_chart")(metric, outputFile + "_bar.png",
                                                **kwargs);
                    break;
                case ChartType::Line:
                    generator.attr("line_chart")(
                        metric, outputFile + "_line.png", **kwargs);
                    break;
                case ChartType::Pie:
                    generator.attr("pie_chart")(metric, outputFile + "_pie.png",
                                                **kwargs);
                    break;
                case ChartType::Histogram:
                    generator.attr("histogram")(
                        metric, outputFile + "_histogram.png", **kwargs);
                    break;
                case ChartType::Heatmap:
                    if (config_.metrics.size() >= 2) {
                        generator.attr("heatmap")(config_.metrics,
                                                  outputFile + "_heatmap.png",
                                                  **kwargs);
                    }
                    break;
                case ChartType::Scatter:
                    if (config_.metrics.size() >= 2) {
                        for (size_t i = 0; i < config_.metrics.size() - 1;
                             ++i) {
                            for (size_t j = i + 1; j < config_.metrics.size();
                                 ++j) {
                                generator.attr("scatter_chart")(
                                    config_.metrics[i], config_.metrics[j],
                                    outputFile + "_scatter.png", **kwargs);
                            }
                        }
                    }
                    break;
                case ChartType::All:
                    generator.attr("all_charts")(config_.metrics, outputDir,
                                                 config_.interactive);
                    break;
            }
        }
    }

    /**
     * @brief Convert ChartStyle enum to string representation
     * @param style The chart style to convert
     * @return String representation for Python
     */
    static std::string convertStyleToString(ChartStyle style) {
        switch (style) {
            case ChartStyle::Seaborn:
                return "seaborn";
            case ChartStyle::Ggplot:
                return "ggplot";
            case ChartStyle::Minimal:
                return "minimal";
            default:
                return "default";
        }
    }
};

/**
 * @brief Factory function to create a chart reporter
 * @param config Configuration for chart generation
 * @return Smart pointer to a ChartReporter
 */
[[nodiscard]] inline auto createChartReporter(ChartConfig config = {})
    -> std::unique_ptr<TestReporter> {
    return std::make_unique<ChartReporter>(std::move(config));
}

/**
 * @brief Check if chart reporting is available (PyBind11 initialized)
 * @return True if chart reporting is available
 */
[[nodiscard]] inline bool isChartReportingAvailable() {
    try {
        py::gil_scoped_acquire acquire;
        auto matplotlib = py::module::import("matplotlib");
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace atom::test

#endif  // ATOM_USE_PYBIND11

#endif  // ATOM_TEST_REPORTER_CHARTS_HPP