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
 * @brief Configuration options for the TestRunner.
 * @details Provides detailed control over how tests are run, including
 * parallelism, retry mechanisms, output formats, etc.
 */
struct TestRunnerConfig {
    bool enableParallel{false};  ///< Enable parallel test execution.
    int numThreads{static_cast<int>(
        std::thread::hardware_concurrency())};  ///< Number of threads for
                                                ///< parallel execution.
                                                ///< Defaults to hardware
                                                ///< concurrency.
    int maxRetries{0};  ///< Maximum number of retries for failed tests.
    bool failFast{
        false};  ///< Stop execution immediately after the first failure.
    std::optional<std::string>
        outputFormat;  ///< Output format (e.g., "json", "xml", "html"). None
                       ///< means no file report.
    std::string outputPath;  ///< Path for the output report file.
    std::optional<std::string>
        testFilter;  ///< Regular expression to filter tests by name.
    bool enableVerboseOutput{
        false};  ///< Enable detailed console output during test execution.
    std::chrono::milliseconds globalTimeout{
        0};  ///< Global timeout for asynchronous tests (0 means no timeout).
    bool shuffleTests{false};  ///< Randomize the order of test execution.
    std::optional<uint64_t>
        randomSeed;  ///< Seed for the random number generator used for
                     ///< shuffling. Uses system clock if not set.
    bool includeSkippedInReport{
        true};  ///< Include skipped tests in the final report.

    /**
     * @brief Sets the parallel execution flag.
     * @param enable True to enable parallel execution, false otherwise.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withParallel(bool enable = true) -> TestRunnerConfig& {
        enableParallel = enable;
        return *this;
    }

    /**
     * @brief Sets the number of threads for parallel execution.
     * @param threads The number of threads to use.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withThreads(int threads) -> TestRunnerConfig& {
        numThreads = threads;
        return *this;
    }

    /**
     * @brief Sets the maximum number of retries for failed tests.
     * @param retries The maximum number of retries.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withRetries(int retries) -> TestRunnerConfig& {
        maxRetries = retries;
        return *this;
    }

    /**
     * @brief Sets the fail-fast flag.
     * @param enable True to enable fail-fast, false otherwise.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withFailFast(bool enable = true) -> TestRunnerConfig& {
        failFast = enable;
        return *this;
    }

    /**
     * @brief Sets the output report format.
     * @param format The desired output format (e.g., "json", "xml").
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withOutputFormat(std::string_view format) -> TestRunnerConfig& {
        outputFormat = std::string(format);
        return *this;
    }

    /**
     * @brief Sets the output path for the report file.
     * @param path The directory or file path for the report.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withOutputPath(std::string_view path) -> TestRunnerConfig& {
        outputPath = std::string(path);
        return *this;
    }

    /**
     * @brief Sets the test filter regular expression.
     * @param filter The regular expression to filter test names.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withFilter(std::string_view filter) -> TestRunnerConfig& {
        testFilter = std::string(filter);
        return *this;
    }

    /**
     * @brief Sets the verbose output flag.
     * @param enable True to enable verbose output, false otherwise.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withVerboseOutput(bool enable = true) -> TestRunnerConfig& {
        enableVerboseOutput = enable;
        return *this;
    }

    /**
     * @brief Sets the global timeout for asynchronous tests.
     * @param timeout The global timeout duration. 0 disables the global
     * timeout.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withGlobalTimeout(std::chrono::milliseconds timeout)
        -> TestRunnerConfig& {
        globalTimeout = timeout;
        return *this;
    }

    /**
     * @brief Sets the test shuffling flag.
     * @param enable True to enable test shuffling, false otherwise.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withShuffleTests(bool enable = true) -> TestRunnerConfig& {
        shuffleTests = enable;
        return *this;
    }

    /**
     * @brief Sets the random seed for test shuffling.
     * @param seed The seed value.
     * @return Reference to the modified TestRunnerConfig object.
     */
    auto withRandomSeed(uint64_t seed) -> TestRunnerConfig& {
        randomSeed = seed;
        return *this;
    }
};

// Forward declaration of needed types
struct TestHooks {
    std::function<void()> beforeAll;
    std::function<void()> afterAll;
    std::function<void()> beforeEach;
    std::function<void()> afterEach;
};

/**
 * @brief A modern test runner class.
 * @details Provides a robust and flexible test execution environment,
 * supporting parallel execution, dependency sorting, test filtering, and more.
 * Uses RAII principles for resource management, adhering to modern C++ design.
 */
class TestRunner {
public:
    /**
     * @brief Constructs a TestRunner with optional configuration.
     * @param config The configuration settings for the test run.
     */
    explicit TestRunner(TestRunnerConfig config = {})
        : config_(std::move(config)), shouldStop_(false) {}

    /**
     * @brief Default destructor.
     */
    ~TestRunner() = default;

    // Disable copy operations
    TestRunner(const TestRunner&) = delete;
    auto operator=(const TestRunner&) -> TestRunner& = delete;

    // Implement custom move operations since std::atomic is not movable
    TestRunner(TestRunner&& other)
        : config_(std::move(other.config_)),
          preparedTests_(std::move(other.preparedTests_)),
          shouldStop_(other.shouldStop_.load()) {
        // Reset the moved-from object's atomic flag
        other.shouldStop_.store(false);
    }

    auto operator=(TestRunner&& other) -> TestRunner& {
        if (this != &other) {
            config_ = std::move(other.config_);
            preparedTests_ = std::move(other.preparedTests_);
            shouldStop_.store(other.shouldStop_.load());
            // Reset the moved-from object
            other.shouldStop_.store(false);
        }
        return *this;
    }

    /**
     * @brief Runs all registered tests according to the configuration.
     * @return A TestStats object containing the results of all executed tests.
     */
    [[nodiscard]] auto runAll() -> TestStats {
        prepareTests();
        executeTests();
        generateReport();
        return getTestStats();
    }

    /**
     * @brief Runs tests belonging to a specific suite.
     * @param suiteName The name of the test suite to run.
     * @return A TestStats object containing the results of the executed tests
     * in the specified suite.
     */
    [[nodiscard]] auto runSuite(std::string_view suiteName) -> TestStats {
        auto& suites = getTestSuites();
        std::vector<TestSuite> filteredSuites;

        // Filter suites by name
        std::copy_if(suites.begin(), suites.end(),
                     std::back_inserter(filteredSuites),
                     [suiteName](const TestSuite& suite) {
                         return suite.name == suiteName;
                     });

        if (filteredSuites.empty()) {
            // No suite found, return empty stats
            return {};
        }

        // Temporarily replace the global suites with the filtered ones
        auto originalSuites = std::move(suites);
        suites = std::move(filteredSuites);

        // Run tests for the selected suite
        prepareTests();
        executeTests();
        generateReport();

        // Restore the original global suites and return the results
        auto result = getTestStats();
        suites = std::move(originalSuites);
        return result;
    }

    /**
     * @brief Adds a single test case to be run.
     * @details The test case is added to an anonymous suite.
     * @param testCase The TestCase object to add.
     * @return Reference to the TestRunner object for chaining.
     * @note This method modifies the global test registry.
     */
    auto addTest(TestCase testCase) -> TestRunner& {
        // Adds to the global registry, assuming getTestSuites() returns a
        // reference
        getTestSuites().push_back({"", {std::move(testCase)}});
        return *this;
    }

    /**
     * @brief Adds a test suite containing multiple test cases.
     * @param suite The TestSuite object to add.
     * @return Reference to the TestRunner object for chaining.
     * @note This method modifies the global test registry.
     */
    auto addSuite(TestSuite suite) -> TestRunner& {
        // Adds to the global registry, assuming getTestSuites() returns a
        // reference
        getTestSuites().push_back(std::move(suite));
        return *this;
    }

    /**
     * @brief Sets the configuration for the TestRunner.
     * @param config The TestRunnerConfig object.
     * @return Reference to the TestRunner object for chaining.
     */
    auto setConfig(TestRunnerConfig config) -> TestRunner& {
        config_ = std::move(config);
        return *this;
    }

private:
    TestRunnerConfig config_;  ///< Configuration for the current test run.
    std::vector<TestCase>
        preparedTests_;  ///< List of tests prepared for execution (filtered,
                         ///< sorted, shuffled).
    std::atomic<bool> shouldStop_;  ///< Flag to signal early termination (e.g.,
                                    ///< due to failFast).
    std::shared_mutex
        resultsMutex_;  ///< Mutex to protect access to shared test results.

    // Helper accessor methods
    std::vector<TestSuite>& getTestSuites() {
        // In a real implementation, this would access the TestRegistry
        static std::vector<TestSuite> suites;
        return suites;
    }

    TestStats& getTestStats() {
        // In a real implementation, this would access test statistics
        static TestStats stats;
        return stats;
    }

    TestHooks& getHooks() {
        // In a real implementation, this would access global hooks
        static TestHooks hooks;
        return hooks;
    }

    std::mutex& getTestMutex() {
        // Mutex for synchronizing test hooks
        static std::mutex mutex;
        return mutex;
    }

    /**
     * @brief Prepares the test cases for execution.
     * @details Collects tests from registered suites, applies filters,
     * sorts by dependencies (if implemented), and shuffles if configured.
     * Resets the test statistics.
     */
    void prepareTests() {
        std::vector<TestCase> allTests;

        // Collect all test cases from the registered suites
        for (const auto& suite : getTestSuites()) {
            allTests.insert(allTests.end(), suite.testCases.begin(),
                            suite.testCases.end());
        }

        // Apply the name filter if provided
        if (config_.testFilter) {
            try {
                std::regex pattern(*config_.testFilter);
                auto it = std::remove_if(allTests.begin(), allTests.end(),
                                         [&pattern](const TestCase& test) {
                                             // Match the full test name against
                                             // the regex
                                             return !std::regex_search(
                                                 test.name, pattern);
                                         });
                allTests.erase(it, allTests.end());
            } catch (const std::regex_error& e) {
                // Handle invalid regex pattern, e.g., log an error
                std::cerr << "Warning: Invalid test filter regex: " << e.what()
                          << std::endl;
                // Optionally, clear the filter or stop execution
                // config_.testFilter.reset();
            }
        }

        // Sort tests based on dependencies (Implementation needed)
        // allTests = sortTestsByDependencies(allTests); // Placeholder

        // Shuffle tests if enabled
        if (config_.shuffleTests) {
            uint64_t seed = config_.randomSeed.value_or(static_cast<uint64_t>(
                std::chrono::system_clock::now().time_since_epoch().count()));
            std::shuffle(allTests.begin(), allTests.end(),
                         std::mt19937_64(seed));
            if (config_.enableVerboseOutput) {
                std::cout << "Shuffling tests with seed: " << seed << std::endl;
            }
        }

        preparedTests_ = std::move(allTests);

        // Reset test statistics for the new run
        auto& stats = getTestStats();
        stats = TestStats{};  // Reset stats object
        shouldStop_ = false;  // Reset stop flag
    }

    /**
     * @brief Executes the prepared test cases.
     * @details Runs the `beforeAll` hook, executes tests sequentially or in
     * parallel based on configuration, and runs the `afterAll` hook.
     */
    void executeTests() {
        auto& hooks = getHooks();
        if (hooks.beforeAll) {
            try {
                hooks.beforeAll();
            } catch (const std::exception& e) {
                std::cerr << "Exception in beforeAll hook: " << e.what()
                          << std::endl;
                // Decide if execution should stop
                shouldStop_ = true;
            } catch (...) {
                std::cerr << "Unknown exception in beforeAll hook."
                          << std::endl;
                shouldStop_ = true;
            }
        }

        if (!shouldStop_) {
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
     * @brief Executes tests sequentially in the prepared order.
     */
    void executeTestsSequentially() {
        auto& hooks = getHooks();
        for (const auto& test : preparedTests_) {
            if (shouldStop_.load())  // Check stop flag before each test
                break;

            if (hooks.beforeEach) {
                try {
                    hooks.beforeEach();
                } catch (const std::exception& e) {
                    std::cerr << "Exception in beforeEach hook for test '"
                              << test.name << "': " << e.what() << std::endl;
                    // Optionally skip the test or mark as failed
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

            // Check failFast condition after executing the test
            if (config_.failFast) {
                std::shared_lock lock(
                    resultsMutex_);  // Use shared lock for reading
                const auto& results = getTestStats().results;
                // Check the result of the *last executed* test
                if (!results.empty() && !results.back().passed &&
                    !results.back().skipped) {
                    shouldStop_ = true;  // Set atomic flag to stop other
                                         // threads/iterations
                    if (config_.enableVerboseOutput) {
                        std::cout << "Fail-fast triggered by test: "
                                  << results.back().name << std::endl;
                    }
                }
            }
        }
    }

    /**
     * @brief Executes tests in parallel using a thread pool.
     */
    void executeTestsInParallel() {
        std::vector<std::thread> threads;
        // Ensure numThreads is not greater than the number of tests or a
        // reasonable limit
        int actualThreads = std::min(config_.numThreads,
                                     static_cast<int>(preparedTests_.size()));
        actualThreads =
            std::max(1, actualThreads);  // Ensure at least one thread
        threads.reserve(actualThreads);

        std::atomic<size_t> nextTestIndex{0};
        auto& hooks = getHooks();
        auto& testMutex = getTestMutex();

        auto threadFunc = [this, &nextTestIndex, &hooks, &testMutex]() {
            while (true) {
                if (shouldStop_.load())
                    break;  // Check stop flag before fetching next index

                size_t index = nextTestIndex.fetch_add(1);
                if (index >= preparedTests_.size()) {
                    break;  // No more tests left
                }

                const auto& test = preparedTests_[index];

                // Execute beforeEach hook (synchronized)
                if (hooks.beforeEach) {
                    try {
                        std::lock_guard lock(
                            testMutex);  // Lock mutex for hook execution
                        hooks.beforeEach();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in beforeEach hook for test '"
                                  << test.name << "': " << e.what()
                                  << std::endl;
                    }
                }

                // Execute the test case with potential retries
                executeTestCase(test, config_.maxRetries);

                // Execute afterEach hook (synchronized)
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

                // Check failFast condition after executing the test
                if (config_.failFast) {
                    bool shouldStopNow = false;
                    {
                        std::shared_lock lock(resultsMutex_);
                        const auto& results = getTestStats().results;
                        // Find the result for the current test
                        for (const auto& result : results) {
                            if (result.name == test.name && !result.passed &&
                                !result.skipped) {
                                shouldStopNow = true;
                                break;
                            }
                        }
                    }

                    if (shouldStopNow) {
                        shouldStop_ = true;  // Signal other threads to stop
                        if (config_.enableVerboseOutput) {
                            std::cout
                                << "Fail-fast triggered by test: " << test.name
                                << std::endl;
                        }
                    }
                }
            }
        };

        // Start worker threads
        for (int i = 0; i < actualThreads; ++i) {
            threads.emplace_back(threadFunc);
        }

        // Wait for all threads to complete
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
        // Skip disabled tests
        if (testCase.skip) {  // 使用 skip 而不是 disabled
            if (config_.enableVerboseOutput) {
                std::cout << "SKIP: " << testCase.name << " (disabled)"
                          << std::endl;
            }

            // Record skipped test in results
            std::lock_guard lock(resultsMutex_);
            TestResult result;
            result.name = testCase.name;
            result.skipped = true;
            result.message = "Test is disabled";  // 使用 message 而不是 reason
            getTestStats().results.push_back(result);
            return;
        }

        // Execute test with retry logic
        bool passed = false;
        std::string errorMessage;

        for (int attempt = 0; attempt <= maxRetries; ++attempt) {
            if (attempt > 0 && config_.enableVerboseOutput) {
                std::cout << "Retrying test: " << testCase.name << " (attempt "
                          << attempt + 1 << "/" << maxRetries + 1 << ")"
                          << std::endl;
            }

            try {
                // Execute the test function
                bool testPassed = testCase.testFunction();

                if (testPassed) {
                    passed = true;
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

            // Break early if we should stop
            if (shouldStop_.load()) {
                break;
            }
        }

        // Record test result
        std::lock_guard lock(resultsMutex_);
        TestResult result;
        result.name = testCase.name;
        result.passed = passed;
        result.skipped = false;
        if (!passed) {
            result.message = errorMessage;  // 使用 message 而不是 reason
        }
        getTestStats().results.push_back(result);

        // Update statistics
        if (passed) {
            getTestStats()
                .passedAsserts++;  // 使用 passedAsserts 而不是 passCount
        } else {
            getTestStats()
                .failedAsserts++;  // 使用 failedAsserts 而不是 failCount
        }
        getTestStats().totalTests++;  // 使用 totalTests 而不是 totalCount
    }

    /**
     * @brief Generate a report based on the test run results
     */
    void generateReport() {
        if (!config_.outputFormat) {
            return;  // No report requested
        }

        std::string filename = config_.outputPath;
        if (filename.empty()) {
            filename = "test_report";
            if (*config_.outputFormat == "json") {
                filename += ".json";
            } else if (*config_.outputFormat == "xml") {
                filename += ".xml";
            } else if (*config_.outputFormat == "html") {
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
            file << "{\n";
            file << "  \"summary\": {\n";
            file << "    \"total\": " << stats.totalTests
                 << ",\n";  // 使用 totalTests 而不是 totalCount
            file << "    \"passed\": " << stats.passedAsserts
                 << ",\n";  // 使用 passedAsserts 而不是 passCount
            file << "    \"failed\": " << stats.failedAsserts
                 << "\n";  // 使用 failedAsserts 而不是 failCount
            file << "  },\n";
            file << "  \"results\": [\n";

            bool first = true;
            for (const auto& result : stats.results) {
                if (!first) {
                    file << ",\n";
                }
                first = false;

                file << "    {\n";
                file << "      \"name\": \"" << result.name << "\",\n";
                file << "      \"passed\": "
                     << (result.passed ? "true" : "false") << ",\n";
                file << "      \"skipped\": "
                     << (result.skipped ? "true" : "false");
                if (!result.message.empty()) {  // 使用 message 而不是 reason
                    file << ",\n      \"reason\": \"" << result.message
                         << "\"\n";
                } else {
                    file << "\n";
                }
                file << "    }";
            }

            file << "\n  ]\n}";
        } else if (format == "xml") {
            file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            file << "<testsuites>\n";
            file << "  <testsuite tests=\"" << stats.totalTests
                 << "\" failures=\"" << stats.failedAsserts
                 << "\">\n";  // 使用 totalTests 和 failedAsserts

            for (const auto& result : stats.results) {
                file << "    <testcase name=\"" << result.name << "\"";
                if (result.skipped) {
                    file << ">\n      <skipped";
                    if (!result.message
                             .empty()) {  // 使用 message 而不是 reason
                        file << " message=\"" << result.message << "\"";
                    }
                    file << "/>\n    </testcase>\n";
                } else if (!result.passed) {
                    file << ">\n      <failure";
                    if (!result.message
                             .empty()) {  // 使用 message 而不是 reason
                        file << " message=\"" << result.message << "\"";
                    }
                    file << "/>\n    </testcase>\n";
                } else {
                    file << "/>\n";
                }
            }

            file << "  </testsuite>\n</testsuites>";
        } else {
            // Plain text format
            file << "Test Report\n";
            file << "===========\n\n";
            file << "Summary:\n";
            file << "  Total:  " << stats.totalTests
                 << "\n";  // 使用 totalTests 而不是 totalCount
            file << "  Passed: " << stats.passedAsserts
                 << "\n";  // 使用 passedAsserts 而不是 passCount
            file << "  Failed: " << stats.failedAsserts
                 << "\n\n";  // 使用 failedAsserts 而不是 failCount
            file << "Results:\n";

            for (const auto& result : stats.results) {
                if (result.skipped) {
                    file << "  SKIP: " << result.name;
                    if (!result.message
                             .empty()) {  // 使用 message 而不是 reason
                        file << " (" << result.message << ")";
                    }
                    file << "\n";
                } else if (result.passed) {
                    file << "  PASS: " << result.name << "\n";
                } else {
                    file << "  FAIL: " << result.name;
                    if (!result.message
                             .empty()) {  // 使用 message 而不是 reason
                        file << " (" << result.message << ")";
                    }
                    file << "\n";
                }
            }
        }

        file.close();

        if (config_.enableVerboseOutput) {
            std::cout << "Test report written to: " << filename << std::endl;
        }
    }
};

}  // namespace atom::test

#endif  // ATOM_TEST_RUNNER_HPP