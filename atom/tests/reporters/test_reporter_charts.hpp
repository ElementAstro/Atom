#ifndef ATOM_TEST_REPORTER_CHARTS_HPP
#define ATOM_TEST_REPORTER_CHARTS_HPP

#if defined(ATOM_USE_PYBIND11)

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "atom/tests/test.hpp"
#include "atom/tests/test_reporter.hpp"

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "atom/type/json.hpp"
#endif

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
                py::finalize_interpreter();
            } catch (const std::exception& e) {
                std::cerr << "Error during Python finalization: " << e.what()
                          << std::endl;
            }
        }
    }

    ChartReporter(const ChartReporter&) = delete;
    auto operator=(const ChartReporter&) -> ChartReporter& = delete;

    ChartReporter(ChartReporter&& other) noexcept
        : config_(std::move(other.config_)),
          pyInitialized_(other.pyInitialized_),
          totalTestCount_(other.totalTestCount_),
          currentSuite_(std::move(other.currentSuite_)),
          currentTest_(std::move(other.currentTest_)),
          startTime_(other.startTime_),
          endTime_(other.endTime_),
          testStartTime_(other.testStartTime_),
          suiteData_(std::move(other.suiteData_)),
          stats_(std::move(other.stats_)) {
        other.pyInitialized_ = false;
    }

    auto operator=(ChartReporter&& other) noexcept -> ChartReporter& {
        if (this != &other) {
            config_ = std::move(other.config_);
            pyInitialized_ = other.pyInitialized_;
            totalTestCount_ = other.totalTestCount_;
            currentSuite_ = std::move(other.currentSuite_);
            currentTest_ = std::move(other.currentTest_);
            startTime_ = other.startTime_;
            endTime_ = other.endTime_;
            testStartTime_ = other.testStartTime_;
            suiteData_ = std::move(other.suiteData_);
            stats_ = std::move(other.stats_);
            other.pyInitialized_ = false;
        }
        return *this;
    }

    /**
     * @brief Handles test run start event
     * @param totalTests The total number of test cases
     */
    void onTestRunStart(int totalTests) override {
        totalTestCount_ = totalTests;
        startTime_ = std::chrono::high_resolution_clock::now();
        suiteData_.clear();
        suiteData_.reserve(totalTests / 5);
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
        nlohmann::json testData;
        testData["name"] = result.name;
        testData["duration"] = result.duration;
        testData["passed"] = result.passed;
        testData["skipped"] = result.skipped;
        testData["message"] = result.message;
        testData["timed_out"] = result.timedOut;

        if (!suiteData_.contains(currentSuite_)) {
            suiteData_[currentSuite_] = nlohmann::json::array();
        }
        suiteData_[currentSuite_].emplace_back(std::move(testData));
    }

    /**
     * @brief Generates charts and visual reports
     * @param stats Complete statistics from the test run
     * @param outputPath Path where the report should be saved
     */
    void generateReport(const TestStats& stats,
                        std::string_view outputPath) override {
        if (!pyInitialized_) {
            std::cerr << "Python interpreter not initialized, skipping chart "
                         "generation"
                      << std::endl;
            return;
        }

        try {
            std::filesystem::path basePath(outputPath);
            if (std::filesystem::is_directory(basePath)) {
                basePath /= config_.outputDirectory;
            } else {
                basePath = std::filesystem::path(outputPath).parent_path() /
                           config_.outputDirectory;
            }
            std::filesystem::create_directories(basePath);

            calculateDerivedMetrics();

            std::string dataFilePath = (basePath / "test_data.json").string();
            std::ofstream dataFile(dataFilePath);
            dataFile << std::setw(4) << suiteData_ << std::endl;
            dataFile.close();

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

            auto sys = py::module::import("sys");
            py::str currentDir = py::str(".");
            sys.attr("path").attr("append")(currentDir);

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
    [[nodiscard]] static auto extractSuiteName(std::string_view fullTestName)
        -> std::string {
        size_t dotPos = fullTestName.find('.');
        if (dotPos != std::string::npos) {
            return std::string(fullTestName.substr(0, dotPos));
        }
        return "DefaultSuite";
    }

    /**
     * @brief Calculate additional metrics from test results
     */
    void calculateDerivedMetrics() {
        for (auto& [suite, tests] : suiteData_.items()) {
            int passCount = 0;
            const int totalTests = static_cast<int>(tests.size());

            for (auto& test : tests) {
                if (test["passed"].get<bool>() &&
                    !test["skipped"].get<bool>()) {
                    passCount++;
                }
            }

            const double passRate =
                (totalTests > 0)
                    ? (static_cast<double>(passCount) / totalTests) * 100.0
                    : 0.0;

            for (auto& test : tests) {
                test["passRate"] = passRate;
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
            auto chartsModule = py::module::import("atom.tests.charts");

            auto generator = chartsModule.attr("ChartGenerator")(
                py::none(), dataFilePath, convertStyleToString(config_.style),
                config_.darkMode);

            if (config_.generateReport) {
                generator.attr("generate_report")(config_.metrics, outputDir);
            } else {
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
    [[nodiscard]] static constexpr auto convertStyleToString(ChartStyle style)
        -> std::string_view {
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
[[nodiscard]] inline auto isChartReportingAvailable() -> bool {
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
