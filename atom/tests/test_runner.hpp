// filepath: [test_runner.hpp](http://_vscodecontentref_/2)
#ifndef ATOM_TEST_RUNNER_HPP
#define ATOM_TEST_RUNNER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <random>
#include <regex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "atom/tests/test.hpp"

namespace atom::test {

/**
 * @brief Configuration options for the TestRunner
 * @details Provides detailed control over how tests are run, including
 * parallelism, retry mechanisms, output formats, etc.
 */
struct TestRunnerConfig {
    bool enableParallel{false};
    int numThreads{static_cast<int>(std::thread::hardware_concurrency())};
    int maxRetries{0};
    bool failFast{false};
    std::optional<std::string> outputFormat;
    std::string outputPath;
    std::optional<std::string> testFilter;
    bool enableVerboseOutput{false};
    std::chrono::milliseconds globalTimeout{0};
    bool shuffleTests{false};
    std::optional<uint64_t> randomSeed;
    bool includeSkippedInReport{true};

    /**
     * @brief Sets the parallel execution flag
     * @param enable True to enable parallel execution, false otherwise
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withParallel(bool enable = true) -> TestRunnerConfig& {
        enableParallel = enable;
        return *this;
    }

    /**
     * @brief Sets the number of threads for parallel execution
     * @param threads The number of threads to use
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withThreads(int threads) -> TestRunnerConfig& {
        numThreads = threads;
        return *this;
    }

    /**
     * @brief Sets the maximum number of retries for failed tests
     * @param retries The maximum number of retries
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withRetries(int retries) -> TestRunnerConfig& {
        maxRetries = retries;
        return *this;
    }

    /**
     * @brief Sets the fail-fast flag
     * @param enable True to enable fail-fast, false otherwise
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withFailFast(bool enable = true) -> TestRunnerConfig& {
        failFast = enable;
        return *this;
    }

    /**
     * @brief Sets the output report format
     * @param format The desired output format (e.g., "json", "xml")
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withOutputFormat(std::string_view format) -> TestRunnerConfig& {
        outputFormat = std::string(format);
        return *this;
    }

    /**
     * @brief Sets the output path for the report file
     * @param path The directory or file path for the report
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withOutputPath(std::string_view path) -> TestRunnerConfig& {
        outputPath = std::string(path);
        return *this;
    }

    /**
     * @brief Sets the test filter regular expression
     * @param filter The regular expression to filter test names
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withFilter(std::string_view filter) -> TestRunnerConfig& {
        testFilter = std::string(filter);
        return *this;
    }

    /**
     * @brief Sets the verbose output flag
     * @param enable True to enable verbose output, false otherwise
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withVerboseOutput(bool enable = true) -> TestRunnerConfig& {
        enableVerboseOutput = enable;
        return *this;
    }

    /**
     * @brief Sets the global timeout for asynchronous tests
     * @param timeout The global timeout duration. 0 disables the global timeout
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withGlobalTimeout(std::chrono::milliseconds timeout)
        -> TestRunnerConfig& {
        globalTimeout = timeout;
        return *this;
    }

    /**
     * @brief Sets the test shuffling flag
     * @param enable True to enable test shuffling, false otherwise
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withShuffleTests(bool enable = true) -> TestRunnerConfig& {
        shuffleTests = enable;
        return *this;
    }

    /**
     * @brief Sets the random seed for test shuffling
     * @param seed The seed value
     * @return Reference to the modified TestRunnerConfig object
     */
    auto withRandomSeed(uint64_t seed) -> TestRunnerConfig& {
        randomSeed = seed;
        return *this;
    }
};

/**
 * @brief Test lifecycle hooks
 */
struct TestHooks {
    std::function<void()> beforeAll;
    std::function<void()> afterAll;
    std::function<void()> beforeEach;
    std::function<void()> afterEach;
};

/**
 * @brief A modern test runner class
 * @details Provides a robust and flexible test execution environment,
 * supporting parallel execution, dependency sorting, test filtering, and more.
 * Uses RAII principles for resource management, adhering to modern C++ design.
 */
class TestRunner {
public:
    /**
     * @brief Constructs a TestRunner with optional configuration
     * @param config The configuration settings for the test run
     */
    explicit TestRunner(TestRunnerConfig config = {})
        : config_(std::move(config)), shouldStop_(false) {}

    ~TestRunner() = default;

    TestRunner(const TestRunner&) = delete;
    auto operator=(const TestRunner&) -> TestRunner& = delete;

    TestRunner(TestRunner&& other) noexcept
        : config_(std::move(other.config_)),
          preparedTests_(std::move(other.preparedTests_)),
          shouldStop_(other.shouldStop_.load()) {
        other.shouldStop_.store(false);
    }

    auto operator=(TestRunner&& other) noexcept -> TestRunner& {
        if (this != &other) {
            config_ = std::move(other.config_);
            preparedTests_ = std::move(other.preparedTests_);
            shouldStop_.store(other.shouldStop_.load());
            other.shouldStop_.store(false);
        }
        return *this;
    }

    /**
     * @brief Runs all registered tests according to the configuration
     * @return A TestStats object containing the results of all executed tests
     */
    [[nodiscard]] auto runAll() -> TestStats {
        prepareTests();
        executeTests();
        generateReport();
        return getTestStats();
    }

    /**
     * @brief Runs tests belonging to a specific suite
     * @param suiteName The name of the test suite to run
     * @return A TestStats object containing the results of the executed tests
     * in the specified suite
     */
    [[nodiscard]] auto runSuite(std::string_view suiteName) -> TestStats {
        auto& suites = getTestSuites();
        std::vector<TestSuite> filteredSuites;
        filteredSuites.reserve(suites.size());

        std::copy_if(suites.begin(), suites.end(),
                     std::back_inserter(filteredSuites),
                     [suiteName](const TestSuite& suite) {
                         return suite.name == suiteName;
                     });

        if (filteredSuites.empty()) {
            return {};
        }

        auto originalSuites = std::move(suites);
        suites = std::move(filteredSuites);

        prepareTests();
        executeTests();
        generateReport();

        auto result = getTestStats();
        suites = std::move(originalSuites);
        return result;
    }

    /**
     * @brief Adds a single test case to be run
     * @details The test case is added to an anonymous suite
     * @param testCase The TestCase object to add
     * @return Reference to the TestRunner object for chaining
     * @note This method modifies the global test registry
     */
    auto addTest(TestCase testCase) -> TestRunner& {
        getTestSuites().emplace_back(
            "", std::vector<TestCase>{std::move(testCase)});
        return *this;
    }

    /**
     * @brief Adds a test suite containing multiple test cases
     * @param suite The TestSuite object to add
     * @return Reference to the TestRunner object for chaining
     * @note This method modifies the global test registry
     */
    auto addSuite(TestSuite suite) -> TestRunner& {
        getTestSuites().emplace_back(std::move(suite));
        return *this;
    }

    /**
     * @brief Sets the configuration for the TestRunner
     * @param config The TestRunnerConfig object
     * @return Reference to the TestRunner object for chaining
     */
    auto setConfig(TestRunnerConfig config) -> TestRunner& {
        config_ = std::move(config);
        return *this;
    }

private:
    TestRunnerConfig config_;
    std::vector<TestCase> preparedTests_;
    std::atomic<bool> shouldStop_;
    mutable std::shared_mutex resultsMutex_;

    std::vector<TestSuite>& getTestSuites() {
        return atom::test::getTestSuites();
    }

    TestStats& getTestStats() { return atom::test::getTestStats(); }

    TestHooks& getHooks() {
        static TestHooks hooks;
        return hooks;
    }

    std::mutex& getTestMutex() { return atom::test::getTestMutex(); }

    /**
     * @brief Prepares the test cases for execution
     * @details Collects tests from registered suites, applies filters,
     * sorts by dependencies, and shuffles if configured. Resets test
     * statistics.
     */
    void prepareTests() {
        std::vector<TestCase> allTests;
        size_t totalTestCount = 0;

        for (const auto& suite : getTestSuites()) {
            totalTestCount += suite.testCases.size();
        }
        allTests.reserve(totalTestCount);

        for (const auto& suite : getTestSuites()) {
            allTests.insert(allTests.end(), suite.testCases.begin(),
                            suite.testCases.end());
        }

        if (config_.testFilter) {
            try {
                const std::regex pattern(*config_.testFilter);
                auto it = std::remove_if(allTests.begin(), allTests.end(),
                                         [&pattern](const TestCase& test) {
                                             return !std::regex_search(
                                                 test.name, pattern);
                                         });
                allTests.erase(it, allTests.end());
            } catch (const std::regex_error& e) {
                std::cerr << "Warning: Invalid test filter regex: " << e.what()
                          << std::endl;
            }
        }

        allTests = sortTestsByDependencies(allTests);

        if (config_.shuffleTests) {
            const uint64_t seed = config_.randomSeed.value_or(
                static_cast<uint64_t>(std::chrono::system_clock::now()
                                          .time_since_epoch()
                                          .count()));
            std::shuffle(allTests.begin(), allTests.end(),
                         std::mt19937_64(seed));
            if (config_.enableVerboseOutput) {
                std::cout << "Shuffling tests with seed: " << seed << std::endl;
            }
        }

        preparedTests_ = std::move(allTests);

        auto& stats = getTestStats();
        stats = TestStats{};
        shouldStop_.store(false);
    }

    /**
     * @brief Executes the prepared test cases
     * @details Runs the beforeAll hook, executes tests sequentially or in
     * parallel based on configuration, and runs the afterAll hook
     */
    void executeTests() {
        auto& hooks = getHooks();
        if (hooks.beforeAll) {
            try {
                hooks.beforeAll();
            } catch (const std::exception& e) {
                std::cerr << "Exception in beforeAll hook: " << e.what()
                          << std::endl;
                shouldStop_.store(true);
            } catch (...) {
                std::cerr << "Unknown exception in beforeAll hook."
                          << std::endl;
                shouldStop_.store(true);
            }
        }

        if (!shouldStop_.load()) {
            if (config_.enableParallel && config_.numThreads > 1 &&
                preparedTests_.size() > 1) {
                executeTestsInParallel();
            } else {
                executeTestsSequentially();
            }
        }

        if (hooks.afterAll) {
            try {
                hooks.afterAll();
            } catch (const std::exception& e) {
                std::cerr << "Exception in afterAll hook: " << e.what()
                          << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in afterAll hook." << std::endl;
            }
        }
    }

    /**
     * @brief Executes tests sequentially in the prepared order
     */
    void executeTestsSequentially() {
        auto& hooks = getHooks();
        for (const auto& test : preparedTests_) {
            if (shouldStop_.load()) {
                break;
            }

            if (hooks.beforeEach) {
                try {
                    hooks.beforeEach();
                } catch (const std::exception& e) {
                    std::cerr << "Exception in beforeEach hook for test '"
                              << test.name << "': " << e.what() << std::endl;
                } catch (...) {
                    std::cerr
                        << "Unknown exception in beforeEach hook for test '"
                        << test.name << "'." << std::endl;
                }
            }

            executeTestCase(test, config_.maxRetries);

            if (hooks.afterEach) {
                try {
                    hooks.afterEach();
                } catch (const std::exception& e) {
                    std::cerr << "Exception in afterEach hook for test '"
                              << test.name << "': " << e.what() << std::endl;
                } catch (...) {
                    std::cerr
                        << "Unknown exception in afterEach hook for test '"
                        << test.name << "'." << std::endl;
                }
            }

            if (config_.failFast && checkFailFastCondition(test.name)) {
                shouldStop_.store(true);
                if (config_.enableVerboseOutput) {
                    std::cout << "Fail-fast triggered by test: " << test.name
                              << std::endl;
                }
                break;
            }
        }
    }

    /**
     * @brief Executes tests in parallel using a thread pool
     */
    void executeTestsInParallel() {
        const int actualThreads = std::clamp(
            config_.numThreads, 1, static_cast<int>(preparedTests_.size()));
        std::vector<std::thread> threads;
        threads.reserve(actualThreads);

        std::atomic<size_t> nextTestIndex{0};
        auto& hooks = getHooks();
        auto& testMutex = getTestMutex();

        auto threadFunc = [this, &nextTestIndex, &hooks, &testMutex]() {
            while (!shouldStop_.load()) {
                const size_t index = nextTestIndex.fetch_add(1);
                if (index >= preparedTests_.size()) {
                    break;
                }

                const auto& test = preparedTests_[index];

                if (hooks.beforeEach) {
                    try {
                        std::lock_guard lock(testMutex);
                        hooks.beforeEach();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in beforeEach hook for test '"
                                  << test.name << "': " << e.what()
                                  << std::endl;
                    }
                }

                executeTestCase(test, config_.maxRetries);

                if (hooks.afterEach) {
                    try {
                        std::lock_guard lock(testMutex);
                        hooks.afterEach();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in afterEach hook for test '"
                                  << test.name << "': " << e.what()
                                  << std::endl;
                    }
                }

                if (config_.failFast && checkFailFastCondition(test.name)) {
                    shouldStop_.store(true);
                    if (config_.enableVerboseOutput) {
                        std::cout
                            << "Fail-fast triggered by test: " << test.name
                            << std::endl;
                    }
                }
            }
        };

        for (int i = 0; i < actualThreads; ++i) {
            threads.emplace_back(threadFunc);
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }

    /**
     * @brief Execute a single test case with retry logic
     * @param testCase The test case to execute
     * @param maxRetries Maximum number of retry attempts
     */
    void executeTestCase(const TestCase& testCase, int maxRetries) {
        if (testCase.skip) {
            if (config_.enableVerboseOutput) {
                std::cout << "SKIP: " << testCase.name << " (disabled)"
                          << std::endl;
            }
            recordTestResult(testCase.name, false, true, "Test is disabled");
            return;
        }

        bool passed = false;
        std::string errorMessage;

        for (int attempt = 0; attempt <= maxRetries && !shouldStop_.load();
             ++attempt) {
            if (attempt > 0 && config_.enableVerboseOutput) {
                std::cout << "Retrying test: " << testCase.name << " (attempt "
                          << attempt + 1 << "/" << maxRetries + 1 << ")"
                          << std::endl;
            }

            try {
                passed = testCase.testFunction();
                if (passed) {
                    break;
                }
            } catch (const std::exception& e) {
                errorMessage = e.what();
                if (config_.enableVerboseOutput) {
                    std::cout << "Exception in test '" << testCase.name
                              << "': " << errorMessage << std::endl;
                }
            } catch (...) {
                errorMessage = "Unknown exception";
                if (config_.enableVerboseOutput) {
                    std::cout << "Unknown exception in test '" << testCase.name
                              << "'" << std::endl;
                }
            }
        }

        recordTestResult(testCase.name, passed, false,
                         passed ? "" : errorMessage);
    }

    /**
     * @brief Record the result of a test execution
     * @param testName Name of the test
     * @param passed Whether the test passed
     * @param skipped Whether the test was skipped
     * @param message Error or status message
     */
    void recordTestResult(const std::string& testName, bool passed,
                          bool skipped, const std::string& message) {
        std::lock_guard lock(resultsMutex_);
        auto& stats = getTestStats();

        TestResult result;
        result.name = testName;
        result.passed = passed;
        result.skipped = skipped;
        result.message = message;

        stats.results.emplace_back(std::move(result));

        if (skipped) {
            stats.skippedTests++;
        } else if (passed) {
            stats.passedAsserts++;
        } else {
            stats.failedAsserts++;
        }
        stats.totalTests++;
    }

    /**
     * @brief Check if fail-fast condition is met for a specific test
     * @param testName Name of the test to check
     * @return True if fail-fast should be triggered
     */
    bool checkFailFastCondition(const std::string& testName) {
        std::shared_lock lock(resultsMutex_);
        const auto& results = getTestStats().results;

        return std::any_of(results.begin(), results.end(),
                           [&testName](const TestResult& result) {
                               return result.name == testName &&
                                      !result.passed && !result.skipped;
                           });
    }

    /**
     * @brief Generate a report based on the test run results
     */
    void generateReport() {
        if (!config_.outputFormat) {
            return;
        }

        std::string filename = config_.outputPath;
        if (filename.empty()) {
            filename = "test_report";
            const auto& format = *config_.outputFormat;
            if (format == "json") {
                filename += ".json";
            } else if (format == "xml") {
                filename += ".xml";
            } else if (format == "html") {
                filename += ".html";
            } else {
                filename += ".txt";
            }
        }

        exportResults(filename, *config_.outputFormat);
    }

    /**
     * @brief Export test results to a file in the specified format
     * @param filename The name of the file to write to
     * @param format The format to use (json, xml, etc.)
     */
    void exportResults(const std::string& filename, const std::string& format) {
        std::ofstream file(filename);
        if (!file) {
            std::cerr << "Failed to open file for report: " << filename
                      << std::endl;
            return;
        }

        const auto& stats = getTestStats();

        if (format == "json") {
            writeJsonReport(file, stats);
        } else if (format == "xml") {
            writeXmlReport(file, stats);
        } else {
            writeTextReport(file, stats);
        }

        if (config_.enableVerboseOutput) {
            std::cout << "Test report written to: " << filename << std::endl;
        }
    }

    /**
     * @brief Write test results in JSON format
     */
    void writeJsonReport(std::ofstream& file, const TestStats& stats) {
        file << "{\n  \"summary\": {\n"
             << "    \"total\": " << stats.totalTests << ",\n"
             << "    \"passed\": " << stats.passedAsserts << ",\n"
             << "    \"failed\": " << stats.failedAsserts << "\n"
             << "  },\n  \"results\": [\n";

        bool first = true;
        for (const auto& result : stats.results) {
            if (!first) {
                file << ",\n";
            }
            first = false;

            file << "    {\n"
                 << "      \"name\": \"" << result.name << "\",\n"
                 << "      \"passed\": " << (result.passed ? "true" : "false")
                 << ",\n"
                 << "      \"skipped\": "
                 << (result.skipped ? "true" : "false");

            if (!result.message.empty()) {
                file << ",\n      \"reason\": \"" << result.message << "\"\n";
            } else {
                file << "\n";
            }
            file << "    }";
        }

        file << "\n  ]\n}";
    }

    /**
     * @brief Write test results in XML format
     */
    void writeXmlReport(std::ofstream& file, const TestStats& stats) {
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             << "<testsuites>\n"
             << "  <testsuite tests=\"" << stats.totalTests << "\" failures=\""
             << stats.failedAsserts << "\">\n";

        for (const auto& result : stats.results) {
            file << "    <testcase name=\"" << result.name << "\"";
            if (result.skipped) {
                file << ">\n      <skipped";
                if (!result.message.empty()) {
                    file << " message=\"" << result.message << "\"";
                }
                file << "/>\n    </testcase>\n";
            } else if (!result.passed) {
                file << ">\n      <failure";
                if (!result.message.empty()) {
                    file << " message=\"" << result.message << "\"";
                }
                file << "/>\n    </testcase>\n";
            } else {
                file << "/>\n";
            }
        }

        file << "  </testsuite>\n</testsuites>";
    }

    /**
     * @brief Write test results in plain text format
     */
    void writeTextReport(std::ofstream& file, const TestStats& stats) {
        file << "Test Report\n===========\n\n"
             << "Summary:\n"
             << "  Total:  " << stats.totalTests << "\n"
             << "  Passed: " << stats.passedAsserts << "\n"
             << "  Failed: " << stats.failedAsserts << "\n\n"
             << "Results:\n";

        for (const auto& result : stats.results) {
            if (result.skipped) {
                file << "  SKIP: " << result.name;
                if (!result.message.empty()) {
                    file << " (" << result.message << ")";
                }
            } else if (result.passed) {
                file << "  PASS: " << result.name;
            } else {
                file << "  FAIL: " << result.name;
                if (!result.message.empty()) {
                    file << " (" << result.message << ")";
                }
            }
            file << "\n";
        }
    }
};

}  // namespace atom::test

#endif  // ATOM_TEST_RUNNER_HPP
