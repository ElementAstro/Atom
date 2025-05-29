#ifndef ATOM_TEST_HPP
#define ATOM_TEST_HPP

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
};

/**
 * @brief Test suite container
 */
struct alignas(64) TestSuite {
    std::string name;
    std::vector<TestCase> testCases;
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
    std::vector<TestResult> results;
};

/**
 * @brief Get global test statistics
 * @return Reference to test statistics
 */
ATOM_INLINE auto getTestStats() -> TestStats& {
    static TestStats stats;
    return stats;
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
        stats.results.emplace_back(test.name, false, true, "Test Skipped", 0.0,
                                   false);
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
    stats.results.emplace_back(test.name, passed, false, resultMessage,
                               timer.elapsed(), timedOut);

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
ATOM_INLINE void runAllTests(int retryCount = 0, bool parallel = false,
                             int numThreads = 4);

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
 * @brief Approximate floating-point equality assertion
 * @param lhs Left operand
 * @param rhs Right operand
 * @param epsilon Tolerance value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
ATOM_INLINE auto expectApprox(double lhs, double rhs, double epsilon,
                              const char* file, int line) -> Expect {
    bool result = std::abs(lhs - rhs) <= epsilon;
    return {result, file, line,
            "Expected " + std::to_string(lhs) + " approx equal to " +
                std::to_string(rhs)};
}

template <typename T, typename U>
auto expectEq(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs == rhs, file, line,
            "Expected " + std::to_string(lhs) + " == " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " == " << rhs;
        return Expect(lhs == rhs, file, line, stream.str());
    }
}

template <typename T, typename U>
auto expectNe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs != rhs, file, line,
            "Expected " + std::to_string(lhs) + " != " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " != " << rhs;
        return Expect(lhs != rhs, file, line, stream.str());
    }
}

template <typename T, typename U>
auto expectGt(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs > rhs, file, line,
            "Expected " + std::to_string(lhs) + " > " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " > " << rhs;
        return Expect(lhs > rhs, file, line, stream.str());
    }
}

/**
 * @brief String containment assertion
 * @param str String to search in
 * @param substr Substring to find
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
ATOM_INLINE auto expectContains(std::string_view str, std::string_view substr,
                                const char* file, int line) -> Expect {
    bool result = str.find(substr) != std::string_view::npos;
    return {result, file, line,
            "Expected \"" + std::string(str) + "\" to contain \"" +
                std::string(substr) + "\""};
}

/**
 * @brief Set equality assertion for vectors
 * @param lhs Left vector
 * @param rhs Right vector
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T>
ATOM_INLINE auto expectSetEq(const std::vector<T>& lhs,
                             const std::vector<T>& rhs, const char* file,
                             int line) -> Expect {
    std::set<T> lhsSet(lhs.begin(), lhs.end());
    std::set<T> rhsSet(rhs.begin(), rhs.end());
    return {lhsSet == rhsSet, file, line, "Expected sets to be equal"};
}

template <typename T, typename U>
auto expectLt(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs < rhs, file, line,
            "Expected " + std::to_string(lhs) + " < " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " < " << rhs;
        return Expect(lhs < rhs, file, line, stream.str());
    }
}

template <typename T, typename U>
auto expectGe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs >= rhs, file, line,
            "Expected " + std::to_string(lhs) + " >= " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " >= " << rhs;
        return Expect(lhs >= rhs, file, line, stream.str());
    }
}

template <typename T, typename U>
    requires std::is_convertible_v<
        decltype(std::declval<T>() <= std::declval<U>()), bool>
auto expectLe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(
            lhs <= rhs, file, line,
            "Expected " + std::to_string(lhs) + " <= " + std::to_string(rhs));
    } else {
        std::stringstream stream;
        stream << "Expected " << lhs << " <= " << rhs;
        return Expect(lhs <= rhs, file, line, stream.str());
    }
}

/**
 * @brief Predicate-based assertion
 * @param value Value to test
 * @param predicate Test predicate
 * @param file Source file
 * @param line Line number
 * @param message Custom message
 * @return Assertion result
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
 * @param func Function expected to throw
 * @param file Source file
 * @param line Line number
 * @return Assertion result
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
     * @param name Test name
     * @param func Test function
     * @param async Async execution flag
     * @param timeLimit Time limit in milliseconds
     * @param skip Skip flag
     * @param dependencies Test dependencies
     * @param tags Test tags
     * @return Reference for method chaining
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

}  // namespace atom::test

#define expect(expr) atom::test::Expect(expr, __FILE__, __LINE__, #expr)
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

/**
 * @brief String literal operator for intuitive test case creation
 * @param name Test name
 * @param size String length (auto-deduced)
 * @return Test registration function
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

#endif  // ATOM_TEST_HPP
