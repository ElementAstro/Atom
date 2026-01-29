/**
 * @file test.hpp
 * @brief Core test framework definitions and assertions
 * @details Provides the fundamental test case structures, assertions, and test
 * execution utilities.
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_CORE_TEST_HPP
#define ATOM_TEST_CORE_TEST_HPP

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "atom/type/json.hpp"
#endif

#include "atom/macro.hpp"

namespace atom::test {

struct TestCase;

/**
 * @brief Filter tests by regex pattern
 * @param pattern Regex pattern to match test names
 * @return Vector of matching test cases
 */
auto filterTests(const std::regex& pattern) -> std::vector<TestCase>;

/**
 * @brief Filter tests by tag
 * @param tag Tag to filter by
 * @return Vector of test cases with matching tag
 */
auto filterTestsByTag(std::string_view tag) -> std::vector<TestCase>;

/**
 * @brief Run a filtered subset of tests
 * @param tests Vector of test cases to run
 * @param retryCount Number of times to retry failed tests
 * @param parallel Whether to run tests in parallel
 * @param numThreads Number of threads for parallel execution
 */
void runTestsFiltered(const std::vector<TestCase>& tests, int retryCount = 0,
                      bool parallel = false, int numThreads = 4);

/**
 * @brief Sort tests by their dependencies
 * @param tests Vector of test cases to sort
 * @return Dependency-sorted vector of test cases
 */
auto sortTestsByDependencies(const std::vector<TestCase>& tests)
    -> std::vector<TestCase>;

/**
 * @brief Test priority levels
 */
enum class TestPriority { Critical = 0, High = 1, Normal = 2, Low = 3 };

/**
 * @brief Test case definition structure
 */
struct alignas(128) TestCase {
    std::string name;
    std::function<void()> func;
    bool skip = false;
    bool async = false;
    double timeLimit = 0.0;
    std::vector<std::string> dependencies;
    std::vector<std::string> tags;
    TestPriority priority = TestPriority::Normal;
    std::string description;

    /**
     * @brief Execute the test function safely
     * @return True if test passed, false otherwise
     */
    [[nodiscard]] bool testFunction() const noexcept {
        try {
            func();
            return true;
        } catch (...) {
            return false;
        }
    }
};

/**
 * @brief Test execution result
 */
struct alignas(64) TestResult {
    std::string name;
    bool passed;
    bool skipped;
    std::string message;
    double duration;
    bool timedOut;
    std::string stackTrace;
    std::vector<std::pair<std::string, std::string>> properties;
};

/**
 * @brief Test suite container
 */
struct alignas(64) TestSuite {
    std::string name;
    std::vector<TestCase> testCases;
    std::string description;
};

/**
 * @brief Get global test suite collection
 * @return Reference to test suites vector
 */
ATOM_INLINE auto getTestSuites() -> std::vector<TestSuite>& {
    static std::vector<TestSuite> testSuites;
    return testSuites;
}

/**
 * @brief Get global test mutex for thread safety
 * @return Reference to test mutex
 */
ATOM_INLINE auto getTestMutex() -> std::mutex& {
    static std::mutex testMutex;
    return testMutex;
}

/**
 * @brief Register a new test case
 * @param name Test name
 * @param func Test function
 * @param async Run asynchronously
 * @param time_limit Time limit in milliseconds
 * @param skip Skip this test
 * @param dependencies Tests this depends on
 * @param tags Categorization tags
 */
ATOM_INLINE void registerTest(std::string name, std::function<void()> func,
                              bool async = false, double time_limit = 0.0,
                              bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {}) {
    TestCase testCase{std::move(name), std::move(func), skip,
                      async,           time_limit,      std::move(dependencies),
                      std::move(tags)};

    std::lock_guard lock(getTestMutex());
    auto& suites = getTestSuites();
    auto it = std::find_if(suites.begin(), suites.end(),
                           [](const TestSuite& s) { return s.name.empty(); });
    if (it != suites.end()) {
        it->testCases.emplace_back(std::move(testCase));
    } else {
        suites.emplace_back(TestSuite{"", {std::move(testCase)}});
    }
}

/**
 * @brief Register a complete test suite
 * @param suite_name Name of the test suite
 * @param cases Vector of test cases
 */
ATOM_INLINE void registerSuite(std::string suite_name,
                               std::vector<TestCase> cases) {
    std::lock_guard lock(getTestMutex());
    getTestSuites().emplace_back(std::move(suite_name), std::move(cases));
}

/**
 * @brief Test execution statistics
 */
struct alignas(64) TestStats {
    int totalTests = 0;
    int totalAsserts = 0;
    int passedAsserts = 0;
    int failedAsserts = 0;
    int skippedTests = 0;
    double totalDuration = 0.0;
    std::vector<TestResult> results;

    [[nodiscard]] int passedTests() const {
        return static_cast<int>(
            std::count_if(results.begin(), results.end(),
                          [](const TestResult& r) { return r.passed; }));
    }

    [[nodiscard]] int failedTests() const {
        return static_cast<int>(std::count_if(
            results.begin(), results.end(),
            [](const TestResult& r) { return !r.passed && !r.skipped; }));
    }
};

/**
 * @brief Get global test statistics
 * @return Reference to test statistics
 */
ATOM_INLINE auto getTestStats() -> TestStats& {
    static TestStats stats;
    return stats;
}

/**
 * @brief Reset global test statistics
 */
ATOM_INLINE void resetTestStats() {
    auto& stats = getTestStats();
    stats = TestStats{};
}

using Hook = std::function<void()>;

/**
 * @brief Test lifecycle hooks
 */
struct alignas(64) Hooks {
    Hook beforeEach;
    Hook afterEach;
    Hook beforeAll;
    Hook afterAll;
};

/**
 * @brief Get global test hooks
 * @return Reference to test hooks
 */
ATOM_INLINE auto getHooks() -> Hooks& {
    static Hooks hooks;
    return hooks;
}

/**
 * @brief Print colored console output
 * @param text Text to print
 * @param color_code ANSI color code
 */
ATOM_INLINE void printColored(std::string_view text,
                              std::string_view color_code) {
    std::cout << "\033[" << color_code << "m" << text << "\033[0m";
}

/**
 * @brief High-resolution timer for performance measurement
 */
struct Timer {
    std::chrono::high_resolution_clock::time_point startTime;

    Timer() { reset(); }

    void reset() { startTime = std::chrono::high_resolution_clock::now(); }

    [[nodiscard]] auto elapsed() const -> double {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::high_resolution_clock::now() - startTime)
            .count();
    }
};

/**
 * @brief Export test results to various formats
 * @param filename Base filename without extension
 * @param format Output format (json, xml, html)
 */
ATOM_INLINE void exportResults(std::string_view filename,
                               std::string_view format) {
    const auto& stats = getTestStats();
    nlohmann::json jsonReport;

    jsonReport["total_tests"] = stats.totalTests;
    jsonReport["total_asserts"] = stats.totalAsserts;
    jsonReport["passed_asserts"] = stats.passedAsserts;
    jsonReport["failed_asserts"] = stats.failedAsserts;
    jsonReport["skipped_tests"] = stats.skippedTests;
    jsonReport["test_results"] = nlohmann::json::array();

    for (const auto& result : stats.results) {
        nlohmann::json jsonResult;
        jsonResult["name"] = result.name;
        jsonResult["passed"] = result.passed;
        jsonResult["skipped"] = result.skipped;
        jsonResult["message"] = result.message;
        jsonResult["duration"] = result.duration;
        jsonResult["timed_out"] = result.timedOut;
        jsonReport["test_results"].emplace_back(std::move(jsonResult));
    }

    std::string filenameStr{filename};

    if (format == "json") {
        std::ofstream file(filenameStr + ".json");
        if (file) {
            file << jsonReport.dump(4);
            std::cout << "Test report saved to " << filenameStr << ".json\n";
        }
    } else if (format == "xml") {
        std::ofstream file(filenameStr + ".xml");
        if (file) {
            file << "<?xml version=\"1.0\"?>\n<testsuite>\n"
                 << "  <total_tests>" << stats.totalTests << "</total_tests>\n"
                 << "  <passed_asserts>" << stats.passedAsserts
                 << "</passed_asserts>\n"
                 << "  <failed_asserts>" << stats.failedAsserts
                 << "</failed_asserts>\n"
                 << "  <skipped_tests>" << stats.skippedTests
                 << "</skipped_tests>\n";

            for (const auto& result : stats.results) {
                file << "  <testcase name=\"" << result.name << "\">\n"
                     << "    <passed>" << (result.passed ? "true" : "false")
                     << "</passed>\n"
                     << "    <message>" << result.message << "</message>\n"
                     << "    <duration>" << result.duration << "</duration>\n"
                     << "    <timed_out>"
                     << (result.timedOut ? "true" : "false") << "</timed_out>\n"
                     << "  </testcase>\n";
            }
            file << "</testsuite>\n";
            std::cout << "Test report saved to " << filenameStr << ".xml\n";
        }
    } else if (format == "html") {
        std::ofstream file(filenameStr + ".html");
        if (file) {
            file << "<!DOCTYPE html><html><head><title>Test "
                    "Report</title></head><body>\n"
                 << "<h1>Test Report</h1>\n"
                 << "<p>Total Tests: " << stats.totalTests << "</p>\n"
                 << "<p>Passed Asserts: " << stats.passedAsserts << "</p>\n"
                 << "<p>Failed Asserts: " << stats.failedAsserts << "</p>\n"
                 << "<p>Skipped Tests: " << stats.skippedTests << "</p>\n"
                 << "<ul>\n";

            for (const auto& result : stats.results) {
                file << "  <li><strong>" << result.name << "</strong>: "
                     << (result.passed
                             ? "<span style='color:green;'>PASSED</span>"
                             : "<span style='color:red;'>FAILED</span>")
                     << " (" << result.duration << " ms)</li>\n";
            }
            file << "</ul>\n</body></html>";
            std::cout << "Test report saved to " << filenameStr << ".html\n";
        }
    }
}

/**
 * @brief Execute a single test case
 * @param test Test case to execute
 * @param retryCount Number of retry attempts on failure
 */
ATOM_INLINE void runTestCase(const TestCase& test, int retryCount = 0) {
    auto& stats = getTestStats();
    Timer timer;
    const auto& hooks = getHooks();

    if (test.skip) {
        printColored("SKIPPED\n", "1;33");
        std::lock_guard lock(getTestMutex());
        stats.skippedTests++;
        stats.totalTests++;
        stats.results.emplace_back(
            TestResult{test.name, false, true, "Test Skipped", 0.0, false});
        return;
    }

    std::string resultMessage;
    bool passed = false;
    bool timedOut = false;

    try {
        if (hooks.beforeEach) {
            hooks.beforeEach();
        }

        timer.reset();
        if (test.async) {
            auto future = std::async(std::launch::async, test.func);
            if (test.timeLimit > 0 && future.wait_for(std::chrono::milliseconds(
                                          static_cast<int>(test.timeLimit))) ==
                                          std::future_status::timeout) {
                timedOut = true;
                throw std::runtime_error("Test timed out");
            }
            future.get();
        } else {
            test.func();
        }
        passed = true;
        resultMessage = "PASSED";
    } catch (const std::exception& e) {
        resultMessage = e.what();
        if (retryCount > 0) {
            printColored("Retrying test...\n", "1;33");
            runTestCase(test, retryCount - 1);
            return;
        }
    }

    try {
        if (hooks.afterEach) {
            hooks.afterEach();
        }
    } catch (const std::exception& e) {
        if (passed) {
            passed = false;
            resultMessage = "After hook failed: " + std::string(e.what());
        }
    }

    std::lock_guard lock(getTestMutex());
    stats.totalTests++;
    stats.results.emplace_back(TestResult{
        test.name, passed, false, resultMessage, timer.elapsed(), timedOut});

    if (timedOut) {
        printColored(resultMessage + " (TIMEOUT)", "1;31");
    } else {
        printColored(resultMessage, passed ? "1;32" : "1;31");
    }
    std::cout << " (" << timer.elapsed() << " ms)\n";
}

/**
 * @brief Execute tests in parallel using thread pool
 * @param tests Vector of test cases
 * @param numThreads Number of worker threads
 */
ATOM_INLINE void runTestsInParallel(const std::vector<TestCase>& tests,
                                    int numThreads = 4) {
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &tests, numThreads]() {
            for (size_t j = i; j < tests.size(); j += numThreads) {
                runTestCase(tests[j]);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

/**
 * @brief Execute all registered tests
 * @param retryCount Number of retry attempts for failed tests
 * @param parallel Enable parallel execution
 * @param numThreads Number of threads for parallel execution
 */
ATOM_INLINE void runAllTests(int retryCount, bool parallel, int numThreads);

/**
 * @brief Execute tests with command line argument parsing
 * @param argc Argument count
 * @param argv Argument vector
 */
ATOM_INLINE void runTests(int argc, char* argv[]) {
    int retryCount = 0;
    bool parallel = false;
    int numThreads = 4;
    std::string exportFormat;
    std::string exportFilename;
    std::string filterPattern;
    std::string testTag;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--retry" && i + 1 < argc) {
            retryCount = std::stoi(argv[++i]);
        } else if (arg == "--parallel" && i + 1 < argc) {
            parallel = true;
            numThreads = std::stoi(argv[++i]);
        } else if (arg == "--export" && i + 2 < argc) {
            exportFormat = argv[++i];
            exportFilename = argv[++i];
        } else if (arg == "--filter" && i + 1 < argc) {
            filterPattern = argv[++i];
        } else if (arg == "--tag" && i + 1 < argc) {
            testTag = argv[++i];
        }
    }

    const auto& hooks = getHooks();
    if (hooks.beforeAll) {
        hooks.beforeAll();
    }

    if (!filterPattern.empty()) {
        std::regex pattern(filterPattern);
        auto filteredTests = filterTests(pattern);
        runTestsFiltered(filteredTests, retryCount, parallel, numThreads);
    } else if (!testTag.empty()) {
        auto filteredTests = filterTestsByTag(testTag);
        runTestsFiltered(filteredTests, retryCount, parallel, numThreads);
    } else {
        runAllTests(retryCount, parallel, numThreads);
    }

    if (hooks.afterAll) {
        hooks.afterAll();
    }

    if (!exportFormat.empty() && !exportFilename.empty()) {
        exportResults(exportFilename, exportFormat);
    }
}

/**
 * @brief Execute tests with default configuration
 */
ATOM_INLINE void runTests() { runTests(0, nullptr); }

ATOM_INLINE auto filterTests(const std::regex& pattern)
    -> std::vector<TestCase> {
    std::vector<TestCase> filtered;
    for (const auto& suite : getTestSuites()) {
        for (const auto& test : suite.testCases) {
            if (std::regex_search(test.name, pattern)) {
                filtered.push_back(test);
            }
        }
    }
    return filtered;
}

ATOM_INLINE auto filterTestsByTag(std::string_view tag)
    -> std::vector<TestCase> {
    std::vector<TestCase> filtered;
    for (const auto& suite : getTestSuites()) {
        for (const auto& test : suite.testCases) {
            if (std::find(test.tags.begin(), test.tags.end(), tag) !=
                test.tags.end()) {
                filtered.push_back(test);
            }
        }
    }
    return filtered;
}

ATOM_INLINE void runTestsFiltered(const std::vector<TestCase>& tests,
                                  int retryCount, bool parallel,
                                  int numThreads) {
    auto sortedTests = sortTestsByDependencies(tests);

    if (parallel) {
        runTestsInParallel(sortedTests, numThreads);
    } else {
        for (const auto& test : sortedTests) {
            runTestCase(test, retryCount);
        }
    }

    const auto& stats = getTestStats();
    std::cout
        << "=================================================================\n"
        << "Total tests: " << stats.totalTests << "\n"
        << "Total asserts: " << stats.totalAsserts << " | "
        << stats.passedAsserts << " passed | " << stats.failedAsserts
        << " failed | " << stats.skippedTests << " skipped\n";
}

ATOM_INLINE auto sortTestsByDependencies(const std::vector<TestCase>& tests)
    -> std::vector<TestCase> {
    std::map<std::string, TestCase> testMap;
    std::vector<TestCase> sortedTests;
    std::set<std::string> processed;

    for (const auto& test : tests) {
        testMap[test.name] = test;
    }

    std::function<void(const TestCase&)> resolveDependencies;
    resolveDependencies = [&](const TestCase& test) {
        if (!processed.contains(test.name)) {
            for (const auto& dep : test.dependencies) {
                if (testMap.contains(dep)) {
                    resolveDependencies(testMap[dep]);
                }
            }
            processed.insert(test.name);
            sortedTests.push_back(test);
        }
    };

    for (const auto& test : tests) {
        resolveDependencies(test);
    }

    return sortedTests;
}

ATOM_INLINE void runAllTests(int retryCount, bool parallel, int numThreads) {
    const auto& stats = getTestStats();
    Timer globalTimer;

    std::vector<TestCase> allTests;
    for (const auto& suite : getTestSuites()) {
        allTests.insert(allTests.end(), suite.testCases.begin(),
                        suite.testCases.end());
    }

    allTests = sortTestsByDependencies(allTests);

    if (parallel) {
        runTestsInParallel(allTests, numThreads);
    } else {
        for (const auto& test : allTests) {
            runTestCase(test, retryCount);
        }
    }

    std::cout
        << "=================================================================\n"
        << "Total tests: " << stats.totalTests << "\n"
        << "Total asserts: " << stats.totalAsserts << " | "
        << stats.passedAsserts << " passed | " << stats.failedAsserts
        << " failed | " << stats.skippedTests << " skipped\n"
        << "Total time: " << globalTimer.elapsed() << " ms\n";
}

/**
 * @brief Base assertion class for test verification
 */
struct alignas(64) Expect {
    bool result;
    const char* file;
    int line;
    std::string message;

    /**
     * @brief Construct an assertion with result tracking
     * @param result Assertion result
     * @param file Source file location
     * @param line Line number
     * @param msg Descriptive message
     */
    Expect(bool result, const char* file, int line, std::string msg)
        : result(result), file(file), line(line), message(std::move(msg)) {
        auto& stats = getTestStats();
        stats.totalAsserts++;
        if (!result) {
            stats.failedAsserts++;
            throw std::runtime_error(std::string(file) + ":" +
                                     std::to_string(line) + ": FAILED - " +
                                     message);
        }
        stats.passedAsserts++;
    }
};

/**
 * @brief Non-fatal assertion (logs failure but doesn't throw)
 */
struct ExpectNonFatal {
    bool result;
    const char* file;
    int line;
    std::string message;

    ExpectNonFatal(bool result, const char* file, int line, std::string msg)
        : result(result), file(file), line(line), message(std::move(msg)) {
        auto& stats = getTestStats();
        stats.totalAsserts++;
        if (!result) {
            stats.failedAsserts++;
            std::cerr << file << ":" << line << ": FAILED - " << message
                      << "\n";
        } else {
            stats.passedAsserts++;
        }
    }

    [[nodiscard]] explicit operator bool() const { return result; }
};

template <typename T>
concept StreamInsertable = requires(std::ostream& os, const T& v) {
    { os << v } -> std::same_as<std::ostream&>;
};

/**
 * @brief Approximate floating-point equality assertion
 */
ATOM_INLINE auto expectApprox(double lhs, double rhs, double epsilon,
                              const char* file, int line) -> Expect {
    bool result = std::abs(lhs - rhs) <= epsilon;
    return {result, file, line,
            "Expected " + std::to_string(lhs) + " approx equal to " +
                std::to_string(rhs)};
}

template <typename T, typename U>
auto expectEq(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs == rhs, file, line,
            "Expected " + std::to_string(lhs) + " == " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " == " << rhs;
            return Expect(lhs == rhs, file, line, stream.str());
        } else {
            return Expect(lhs == rhs, file, line,
                          "Expected values to be equal");
        }
    }
}

template <typename T, typename U>
auto expectNe(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs != rhs, file, line,
            "Expected " + std::to_string(lhs) + " != " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " != " << rhs;
            return Expect(lhs != rhs, file, line, stream.str());
        } else {
            return Expect(lhs != rhs, file, line,
                          "Expected values to be different");
        }
    }
}

template <typename T, typename U>
auto expectGt(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs > rhs, file, line,
            "Expected " + std::to_string(lhs) + " > " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " > " << rhs;
            return Expect(lhs > rhs, file, line, stream.str());
        } else {
            return Expect(lhs > rhs, file, line,
                          "Expected left value to be greater than right");
        }
    }
}

template <typename T, typename U>
auto expectLt(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs < rhs, file, line,
            "Expected " + std::to_string(lhs) + " < " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " < " << rhs;
            return Expect(lhs < rhs, file, line, stream.str());
        } else {
            return Expect(lhs < rhs, file, line,
                          "Expected left value to be less than right");
        }
    }
}

template <typename T, typename U>
auto expectGe(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs >= rhs, file, line,
            "Expected " + std::to_string(lhs) + " >= " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " >= " << rhs;
            return Expect(lhs >= rhs, file, line, stream.str());
        } else {
            return Expect(lhs >= rhs, file, line,
                          "Expected left value to be >= right");
        }
    }
}

template <typename T, typename U>
    requires std::is_convertible_v<
                 decltype(std::declval<T>() <= std::declval<U>()), bool>
auto expectLe(const T& lhs, const U& rhs, const char* file,
              int line) -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs <= rhs, file, line,
            "Expected " + std::to_string(lhs) + " <= " + std::to_string(rhs));
    } else {
        if constexpr (StreamInsertable<T> && StreamInsertable<U>) {
            std::stringstream stream;
            stream << "Expected " << lhs << " <= " << rhs;
            return Expect(lhs <= rhs, file, line, stream.str());
        } else {
            return Expect(lhs <= rhs, file, line,
                          "Expected left value to be <= right");
        }
    }
}

/**
 * @brief String containment assertion
 */
ATOM_INLINE auto expectContains(std::string_view str, std::string_view substr,
                                const char* file, int line) -> Expect {
    bool result = str.find(substr) != std::string_view::npos;
    return {result, file, line,
            "Expected \"" + std::string(str) + "\" to contain \"" +
                std::string(substr) + "\""};
}

/**
 * @brief Set equality assertion for iterable containers
 */
template <typename ContainerL, typename ContainerR>
ATOM_INLINE auto expectSetEq(const ContainerL& lhs, const ContainerR& rhs,
                             const char* file, int line) -> Expect {
    using LVal = std::decay_t<decltype(*std::begin(lhs))>;
    using RVal = std::decay_t<decltype(*std::begin(rhs))>;
    using Val = std::common_type_t<LVal, RVal>;

    std::set<Val> lhsSet(std::begin(lhs), std::end(lhs));
    std::set<Val> rhsSet(std::begin(rhs), std::end(rhs));
    return {lhsSet == rhsSet, file, line, "Expected sets to be equal"};
}

/**
 * @brief Predicate-based assertion
 */
template <typename T, typename Pred>
    requires std::is_invocable_r_v<bool, Pred, T>
auto expectThat(const T& value, Pred predicate, const char* file, int line,
                std::string_view message = "") -> Expect {
    bool result = predicate(value);
    return {
        result, file, line,
        message.empty() ? "Predicate failed for value" : std::string(message)};
}

/**
 * @brief Exception throwing assertion
 */
template <typename Func, typename ExceptionType = std::exception>
auto expectThrows(Func&& func, const char* file, int line) -> Expect {
    try {
        std::forward<Func>(func)();
        return {false, file, line, "Expected exception, but none was thrown"};
    } catch (const ExceptionType&) {
        return {true, file, line, "Exception thrown as expected"};
    } catch (...) {
        return {false, file, line, "Wrong exception type thrown"};
    }
}

/**
 * @brief Exception throwing with message matching assertion
 */
template <typename Func, typename ExceptionType = std::exception>
auto expectThrowsWithMessage(Func&& func, std::string_view expectedMessage,
                             const char* file, int line) -> Expect {
    try {
        std::forward<Func>(func)();
        return {false, file, line, "Expected exception, but none was thrown"};
    } catch (const ExceptionType& e) {
        std::string_view actualMessage = e.what();
        if (actualMessage.find(expectedMessage) != std::string_view::npos) {
            return {true, file, line, "Exception thrown with expected message"};
        }
        return {false, file, line,
                "Exception message mismatch. Expected: \"" +
                    std::string(expectedMessage) + "\", Got: \"" +
                    std::string(actualMessage) + "\""};
    } catch (...) {
        return {false, file, line, "Wrong exception type thrown"};
    }
}

/**
 * @brief No exception thrown assertion
 */
template <typename Func>
auto expectNoThrow(Func&& func, const char* file, int line) -> Expect {
    try {
        std::forward<Func>(func)();
        return {true, file, line, "No exception thrown as expected"};
    } catch (const std::exception& e) {
        return {false, file, line,
                "Unexpected exception thrown: " + std::string(e.what())};
    } catch (...) {
        return {false, file, line, "Unknown exception thrown"};
    }
}

/**
 * @brief True assertion
 */
ATOM_INLINE auto expectTrue(bool value, const char* file, int line,
                            const char* expr) -> Expect {
    return {value, file, line,
            "Expected true, but got false for: " + std::string(expr)};
}

/**
 * @brief False assertion
 */
ATOM_INLINE auto expectFalse(bool value, const char* file, int line,
                             const char* expr) -> Expect {
    return {!value, file, line,
            "Expected false, but got true for: " + std::string(expr)};
}

/**
 * @brief Null pointer assertion
 */
template <typename T>
auto expectNull(T* ptr, const char* file, int line) -> Expect {
    return {ptr == nullptr, file, line, "Expected nullptr, but got non-null"};
}

/**
 * @brief Non-null pointer assertion
 */
template <typename T>
auto expectNotNull(T* ptr, const char* file, int line) -> Expect {
    return {ptr != nullptr, file, line, "Expected non-null, but got nullptr"};
}

/**
 * @brief Near equality assertion for floating-point
 */
ATOM_INLINE auto expectNear(double lhs, double rhs, double absTolerance,
                            const char* file, int line) -> Expect {
    double diff = std::abs(lhs - rhs);
    bool result = diff <= absTolerance;
    return {result, file, line,
            "Expected " + std::to_string(lhs) + " near " + std::to_string(rhs) +
                " (tolerance: " + std::to_string(absTolerance) +
                ", actual diff: " + std::to_string(diff) + ")"};
}

/**
 * @brief Range assertion
 */
template <typename T>
auto expectInRange(const T& value, const T& min, const T& max, const char* file,
                   int line) -> Expect {
    bool result = (value >= min && value <= max);
    std::stringstream stream;
    stream << "Expected " << value << " in range [" << min << ", " << max
           << "]";
    return {result, file, line, stream.str()};
}

/**
 * @brief Empty container assertion
 */
template <typename Container>
auto expectEmpty(const Container& container, const char* file,
                 int line) -> Expect {
    return {container.empty(), file, line,
            "Expected empty container, but size is " +
                std::to_string(container.size())};
}

/**
 * @brief Non-empty container assertion
 */
template <typename Container>
auto expectNotEmpty(const Container& container, const char* file,
                    int line) -> Expect {
    return {!container.empty(), file, line,
            "Expected non-empty container, but it is empty"};
}

/**
 * @brief Size assertion for containers
 */
template <typename Container>
auto expectSize(const Container& container, size_t expectedSize,
                const char* file, int line) -> Expect {
    size_t actualSize = container.size();
    return {actualSize == expectedSize, file, line,
            "Expected size " + std::to_string(expectedSize) + ", but got " +
                std::to_string(actualSize)};
}

/**
 * @brief Starts with assertion for strings
 */
ATOM_INLINE auto expectStartsWith(std::string_view str, std::string_view prefix,
                                  const char* file, int line) -> Expect {
    bool result = str.length() >= prefix.length() &&
                  str.substr(0, prefix.length()) == prefix;
    return {result, file, line,
            "Expected \"" + std::string(str) + "\" to start with \"" +
                std::string(prefix) + "\""};
}

/**
 * @brief Ends with assertion for strings
 */
ATOM_INLINE auto expectEndsWith(std::string_view str, std::string_view suffix,
                                const char* file, int line) -> Expect {
    bool result = str.length() >= suffix.length() &&
                  str.substr(str.length() - suffix.length()) == suffix;
    return {result, file, line,
            "Expected \"" + std::string(str) + "\" to end with \"" +
                std::string(suffix) + "\""};
}

/**
 * @brief Regex match assertion for strings
 */
ATOM_INLINE auto expectMatches(std::string_view str, std::string_view pattern,
                               const char* file, int line) -> Expect {
    std::regex regexPattern{std::string(pattern)};
    std::string strCopy(str);
    bool result = std::regex_search(strCopy, regexPattern);
    return {result, file, line,
            "Expected \"" + std::string(str) + "\" to match pattern \"" +
                std::string(pattern) + "\""};
}

/**
 * @brief All elements satisfy predicate assertion
 */
template <typename Container, typename Predicate>
auto expectAllOf(const Container& container, Predicate pred, const char* file,
                 int line) -> Expect {
    bool result = std::all_of(container.begin(), container.end(), pred);
    return {result, file, line,
            result ? "All elements satisfy predicate"
                   : "Not all elements satisfy predicate"};
}

/**
 * @brief Any element satisfies predicate assertion
 */
template <typename Container, typename Predicate>
auto expectAnyOf(const Container& container, Predicate pred, const char* file,
                 int line) -> Expect {
    bool result = std::any_of(container.begin(), container.end(), pred);
    return {result, file, line,
            result ? "At least one element satisfies predicate"
                   : "No element satisfies predicate"};
}

/**
 * @brief No element satisfies predicate assertion
 */
template <typename Container, typename Predicate>
auto expectNoneOf(const Container& container, Predicate pred, const char* file,
                  int line) -> Expect {
    bool result = std::none_of(container.begin(), container.end(), pred);
    return {result, file, line,
            result ? "No element satisfies predicate"
                   : "At least one element satisfies predicate"};
}

/**
 * @brief Container is sorted assertion
 */
template <typename Container>
auto expectSorted(const Container& container, const char* file,
                  int line) -> Expect {
    bool result = std::is_sorted(container.begin(), container.end());
    return {result, file, line,
            result ? "Container is sorted" : "Container is not sorted"};
}

/**
 * @brief Container has unique elements assertion
 */
template <typename Container>
auto expectUnique(const Container& container, const char* file,
                  int line) -> Expect {
    std::set<typename Container::value_type> uniqueSet(container.begin(),
                                                       container.end());
    bool result = uniqueSet.size() == container.size();
    return {result, file, line,
            result ? "All elements are unique"
                   : "Container has duplicate elements"};
}

/**
 * @brief Container contains element assertion
 */
template <typename Container, typename T>
auto expectContainsElement(const Container& container, const T& element,
                           const char* file, int line) -> Expect {
    bool result = std::find(container.begin(), container.end(), element) !=
                  container.end();
    return {result, file, line,
            result ? "Container contains element"
                   : "Container does not contain element"};
}

/**
 * @brief Type check assertion
 */
template <typename ExpectedType, typename T>
auto expectType([[maybe_unused]] const T& value, const char* file,
                int line) -> Expect {
    bool result = std::is_same_v<T, ExpectedType>;
    return {result, file, line,
            result ? "Type matches expected" : "Type does not match expected"};
}

/**
 * @brief Same type assertion
 */
template <typename T1, typename T2>
auto expectSameType([[maybe_unused]] const T1& val1,
                    [[maybe_unused]] const T2& val2, const char* file,
                    int line) -> Expect {
    bool result = std::is_same_v<T1, T2>;
    return {result, file, line,
            result ? "Types are the same" : "Types are different"};
}

/**
 * @brief Pointer equality assertion
 */
template <typename T>
auto expectPtrEq(const T* ptr1, const T* ptr2, const char* file,
                 int line) -> Expect {
    bool result = ptr1 == ptr2;
    std::stringstream stream;
    stream << "Expected pointers to be equal: "
           << static_cast<const void*>(ptr1) << " vs "
           << static_cast<const void*>(ptr2);
    return {result, file, line, stream.str()};
}

/**
 * @brief Pointer inequality assertion
 */
template <typename T>
auto expectPtrNe(const T* ptr1, const T* ptr2, const char* file,
                 int line) -> Expect {
    bool result = ptr1 != ptr2;
    std::stringstream stream;
    stream << "Expected pointers to be different: "
           << static_cast<const void*>(ptr1) << " vs "
           << static_cast<const void*>(ptr2);
    return {result, file, line, stream.str()};
}

/**
 * @brief Double comparison with relative tolerance
 */
ATOM_INLINE auto expectRelativelyNear(double lhs, double rhs,
                                      double relTolerance, const char* file,
                                      int line) -> Expect {
    double maxVal = std::max(std::abs(lhs), std::abs(rhs));
    double diff = std::abs(lhs - rhs);
    bool result = diff <= relTolerance * maxVal;
    return {result, file, line,
            "Expected " + std::to_string(lhs) + " relatively near " +
                std::to_string(rhs) +
                " (rel tolerance: " + std::to_string(relTolerance) +
                ", actual rel diff: " + std::to_string(diff / maxVal) + ")"};
}

/**
 * @brief RAII test suite builder for organized test registration
 */
class TestSuiteBuilder {
public:
    explicit TestSuiteBuilder(std::string name) : suiteName_(std::move(name)) {}

    ~TestSuiteBuilder() {
        if (!testCases_.empty()) {
            registerSuite(std::move(suiteName_), std::move(testCases_));
        }
    }

    /**
     * @brief Add test to suite with fluent interface
     */
    TestSuiteBuilder& addTest(std::string name, std::function<void()> func,
                              bool async = false, double timeLimit = 0.0,
                              bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {}) {
        testCases_.emplace_back(
            TestCase{.name = std::move(name),
                     .func = std::move(func),
                     .skip = skip,
                     .async = async,
                     .timeLimit = timeLimit,
                     .dependencies = std::move(dependencies),
                     .tags = std::move(tags)});
        return *this;
    }

    TestSuiteBuilder(const TestSuiteBuilder&) = delete;
    TestSuiteBuilder& operator=(const TestSuiteBuilder&) = delete;
    TestSuiteBuilder(TestSuiteBuilder&&) = delete;
    TestSuiteBuilder& operator=(TestSuiteBuilder&&) = delete;

private:
    std::string suiteName_;
    std::vector<TestCase> testCases_;
};

/**
 * @brief Scoped trace for better error context
 */
class ScopedTrace {
public:
    ScopedTrace(const char* file, int line, std::string_view message)
        : file_(file), line_(line), message_(message) {
        getTraceStack().emplace_back(file_, line_, std::string(message_));
    }

    ~ScopedTrace() {
        if (!getTraceStack().empty()) {
            getTraceStack().pop_back();
        }
    }

    ScopedTrace(const ScopedTrace&) = delete;
    ScopedTrace& operator=(const ScopedTrace&) = delete;

    static auto getTraceStack()
        -> std::vector<std::tuple<const char*, int, std::string>>& {
        static std::vector<std::tuple<const char*, int, std::string>> stack;
        return stack;
    }

    static auto formatTrace() -> std::string {
        std::ostringstream oss;
        for (const auto& [file, line, msg] : getTraceStack()) {
            oss << "  " << file << ":" << line << ": " << msg << "\n";
        }
        return oss.str();
    }

private:
    const char* file_;
    int line_;
    std::string_view message_;
};

/**
 * @brief Record custom property for test result
 */
ATOM_INLINE void recordProperty(const std::string& key,
                                const std::string& value) {
    auto& stats = getTestStats();
    if (!stats.results.empty()) {
        stats.results.back().properties.emplace_back(key, value);
    }
}

}  // namespace atom::test

// Assertion macros
#define expect_eq(lhs, rhs) atom::test::expectEq(lhs, rhs, __FILE__, __LINE__)
#define expect_ne(lhs, rhs) atom::test::expectNe(lhs, rhs, __FILE__, __LINE__)
#define expect_gt(lhs, rhs) atom::test::expectGt(lhs, rhs, __FILE__, __LINE__)
#define expect_lt(lhs, rhs) atom::test::expectLt(lhs, rhs, __FILE__, __LINE__)
#define expect_ge(lhs, rhs) atom::test::expectGe(lhs, rhs, __FILE__, __LINE__)
#define expect_le(lhs, rhs) atom::test::expectLe(lhs, rhs, __FILE__, __LINE__)
#define expect_approx(lhs, rhs, eps) \
    atom::test::expectApprox(lhs, rhs, eps, __FILE__, __LINE__)
#define expect_contains(str, substr) \
    atom::test::expectContains(str, substr, __FILE__, __LINE__)
#define expect_set_eq(lhs, rhs) \
    atom::test::expectSetEq(lhs, rhs, __FILE__, __LINE__)
#define expect_that(val, pred, msg) \
    atom::test::expectThat(val, pred, __FILE__, __LINE__, msg)
#define expect_throws(func) atom::test::expectThrows(func, __FILE__, __LINE__)
#define expect_throws_as(func, ex) \
    atom::test::expectThrows<decltype(func), ex>(func, __FILE__, __LINE__)
#define expect_throws_with_message(func, msg) \
    atom::test::expectThrowsWithMessage(func, msg, __FILE__, __LINE__)
#define expect_no_throw(func) \
    atom::test::expectNoThrow(func, __FILE__, __LINE__)
#define expect_true(expr) \
    atom::test::expectTrue(static_cast<bool>(expr), __FILE__, __LINE__, #expr)
#define expect_false(expr) \
    atom::test::expectFalse(static_cast<bool>(expr), __FILE__, __LINE__, #expr)
#define expect_null(ptr) atom::test::expectNull(ptr, __FILE__, __LINE__)
#define expect_not_null(ptr) atom::test::expectNotNull(ptr, __FILE__, __LINE__)
#define expect_near(lhs, rhs, tol) \
    atom::test::expectNear(lhs, rhs, tol, __FILE__, __LINE__)
#define expect_in_range(val, min, max) \
    atom::test::expectInRange(val, min, max, __FILE__, __LINE__)
#define expect_empty(container) \
    atom::test::expectEmpty(container, __FILE__, __LINE__)
#define expect_not_empty(container) \
    atom::test::expectNotEmpty(container, __FILE__, __LINE__)
#define expect_size(container, size) \
    atom::test::expectSize(container, size, __FILE__, __LINE__)
#define expect_starts_with(str, prefix) \
    atom::test::expectStartsWith(str, prefix, __FILE__, __LINE__)
#define expect_ends_with(str, suffix) \
    atom::test::expectEndsWith(str, suffix, __FILE__, __LINE__)
#define expect_matches(str, pattern) \
    atom::test::expectMatches(str, pattern, __FILE__, __LINE__)
#define expect_all_of(container, pred) \
    atom::test::expectAllOf(container, pred, __FILE__, __LINE__)
#define expect_any_of(container, pred) \
    atom::test::expectAnyOf(container, pred, __FILE__, __LINE__)
#define expect_none_of(container, pred) \
    atom::test::expectNoneOf(container, pred, __FILE__, __LINE__)
#define expect_sorted(container) \
    atom::test::expectSorted(container, __FILE__, __LINE__)
#define expect_unique(container) \
    atom::test::expectUnique(container, __FILE__, __LINE__)
#define expect_contains_element(container, element) \
    atom::test::expectContainsElement(container, element, __FILE__, __LINE__)
#define expect_type(value, type) \
    atom::test::expectType<type>(value, __FILE__, __LINE__)
#define expect_same_type(val1, val2) \
    atom::test::expectSameType(val1, val2, __FILE__, __LINE__)

#define FAIL(msg) atom::test::Expect(false, __FILE__, __LINE__, msg)
#define SUCCEED() atom::test::Expect(true, __FILE__, __LINE__, "Success")

#define SCOPED_TRACE(message) \
    atom::test::ScopedTrace scoped_trace_##__LINE__(__FILE__, __LINE__, message)

#define RECORD_PROPERTY(key, value) atom::test::recordProperty(key, value)

/**
 * @brief String literal operator for intuitive test case creation
 */
ATOM_INLINE auto operator""_test(const char* name,
                                 [[maybe_unused]] std::size_t size) {
    return [name](std::function<void()> func, bool async = false,
                  double time_limit = 0.0, bool skip = false,
                  std::vector<std::string> const& dependencies = {},
                  std::vector<std::string> const& tags = {}) {
        return atom::test::registerTest(name, std::move(func), async,
                                        time_limit, skip, dependencies, tags);
    };
}

#endif  // ATOM_TEST_CORE_TEST_HPP
