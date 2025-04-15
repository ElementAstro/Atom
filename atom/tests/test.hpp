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
#include <stdexcept>
#include <string>
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

auto filterTests(const std::regex& pattern) -> std::vector<TestCase>;

/**
 * @brief Filter tests by tag
 * @param tag Tag to filter by
 * @return Vector of test cases with matching tag
 */
auto filterTestsByTag(const std::string& tag) -> std::vector<TestCase>;

void runTestsFiltered(const std::vector<TestCase>& tests, int retryCount = 0,
                      bool parallel = false, int numThreads = 4);

auto sortTestsByDependencies(const std::vector<TestCase>& tests)
    -> std::vector<TestCase>;

/**
 * @brief Test case definition structure
 * @details Represents a single test case with all metadata
 */
struct TestCase {
    std::string name;
    std::function<void()> func;
    bool skip = false;                      // Whether to skip the test
    bool async = false;                     // Whether to run asynchronously
    double timeLimit = 0.0;                 // Time threshold for test
    std::vector<std::string> dependencies;  // Tests this one depends on
    std::vector<std::string> tags;          // Tags for categorizing tests

    /**
     * @brief Executes the test function and returns whether it passed
     * @details This wraps the void-returning func in a try-catch block and returns success/failure
     * @return True if the test passed, false otherwise
     */
    [[nodiscard]] bool testFunction() const {
        try {
            func();
            return true;  // Test passed if no exceptions were thrown
        } catch (const std::exception&) {
            return false;  // Test failed if an exception was thrown
        }
    }
} ATOM_ALIGNAS(128);

/**
 * @brief Test result information
 * @details Stores the result of running a test case
 */
struct TestResult {
    std::string name;
    bool passed;
    bool skipped;
    std::string message;
    double duration;
    bool timedOut;
} ATOM_ALIGNAS(128);

/**
 * @brief Test suite structure
 * @details A collection of related test cases
 */
struct TestSuite {
    std::string name;
    std::vector<TestCase> testCases;
} ATOM_ALIGNAS(64);

/**
 * @brief Get the global test suite collection
 * @return Reference to the vector of test suites
 */
ATOM_INLINE auto getTestSuites() -> std::vector<TestSuite>& {
    static std::vector<TestSuite> testSuites;
    return testSuites;
}

/**
 * @brief Get the global test mutex
 * @return Reference to the test mutex
 */
ATOM_INLINE auto getTestMutex() -> std::mutex& {
    static std::mutex testMutex;
    return testMutex;
}

/**
 * @brief Register a new test case
 * @param name Test name
 * @param func Test function
 * @param async Whether to run the test asynchronously
 * @param time_limit Time limit in milliseconds (0 = no limit)
 * @param skip Whether to skip the test
 * @param dependencies Tests that this test depends on
 * @param tags Optional tags for categorizing the test
 */
ATOM_INLINE void registerTest(const std::string& name,
                              std::function<void()> func, bool async = false,
                              double time_limit = 0.0, bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {}) {
    TestCase testCase{name,           std::move(func), skip,
                      async,          time_limit,      std::move(dependencies),
                      std::move(tags)};

    std::lock_guard lock(getTestMutex());
    // Find default suite or create one
    auto& suites = getTestSuites();
    auto it = std::find_if(suites.begin(), suites.end(),
                           [](const TestSuite& s) { return s.name.empty(); });
    if (it != suites.end()) {
        it->testCases.push_back(std::move(testCase));
    } else {
        suites.push_back({"", {std::move(testCase)}});
    }
}

/**
 * @brief Register a test suite with multiple test cases
 * @param suite_name Name of the suite
 * @param cases Vector of test cases
 */
ATOM_INLINE void registerSuite(const std::string& suite_name,
                               std::vector<TestCase> cases) {
    std::lock_guard lock(getTestMutex());
    getTestSuites().push_back({suite_name, std::move(cases)});
}

/**
 * @brief Test statistics structure
 * @details Tracks aggregate statistics about test execution
 */
struct TestStats {
    int totalTests = 0;
    int totalAsserts = 0;
    int passedAsserts = 0;
    int failedAsserts = 0;
    int skippedTests = 0;
    std::vector<TestResult> results;
} ATOM_ALIGNAS(64);

/**
 * @brief Get the global test statistics
 * @return Reference to the test statistics
 */
ATOM_INLINE auto getTestStats() -> TestStats& {
    static TestStats stats;
    return stats;
}

/**
 * @brief Function type for test lifecycle hooks
 */
using Hook = std::function<void()>;

/**
 * @brief Test lifecycle hooks structure
 * @details Functions called at different phases of the test lifecycle
 */
struct Hooks {
    Hook beforeEach;  // Called before each test
    Hook afterEach;   // Called after each test
    Hook beforeAll;   // Called once before all tests
    Hook afterAll;    // Called once after all tests
} ATOM_ALIGNAS(128);

/**
 * @brief Get the global test hooks
 * @return Reference to the test hooks
 */
ATOM_INLINE auto getHooks() -> Hooks& {
    static Hooks hooks;
    return hooks;
}

/**
 * @brief Print colored text to the console
 * @param text Text to print
 * @param color_code ANSI color code
 */
ATOM_INLINE void printColored(const std::string& text,
                              const std::string& color_code) {
    std::cout << "\033[" << color_code << "m" << text << "\033[0m";
}

/**
 * @brief High-resolution timer class
 * @details Used to measure test execution time
 */
struct Timer {
    std::chrono::high_resolution_clock::time_point startTime;

    /**
     * @brief Construct a timer and start it
     */
    Timer() { reset(); }

    /**
     * @brief Reset the timer to the current time
     */
    void reset() { startTime = std::chrono::high_resolution_clock::now(); }

    /**
     * @brief Get the elapsed time in milliseconds
     * @return Elapsed time in milliseconds
     */
    [[nodiscard]] auto elapsed() const -> double {
        return std::chrono::duration<double, std::milli>(
                   std::chrono::high_resolution_clock::now() - startTime)
            .count();
    }
};

/**
 * @brief Export test results to a file
 * @param filename Base filename without extension
 * @param format Output format (json, xml, html)
 */
ATOM_INLINE void exportResults(const std::string& filename,
                               const std::string& format) {
    auto& stats = getTestStats();
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
        jsonReport["test_results"].push_back(jsonResult);
    }

    if (format == "json") {
        std::ofstream file(filename + ".json");
        file << jsonReport.dump(4);
        file.close();
        std::cout << "Test report saved to " << filename << ".json\n";
    } else if (format == "xml") {
        std::ofstream file(filename + ".xml");
        file << "<?xml version=\"1.0\"?>\n<testsuite>\n";
        file << "  <total_tests>" << stats.totalTests << "</total_tests>\n";
        file << "  <passed_asserts>" << stats.passedAsserts
             << "</passed_asserts>\n";
        file << "  <failed_asserts>" << stats.failedAsserts
             << "</failed_asserts>\n";
        file << "  <skipped_tests>" << stats.skippedTests
             << "</skipped_tests>\n";
        for (const auto& result : stats.results) {
            file << "  <testcase name=\"" << result.name << "\">\n";
            file << "    <passed>" << (result.passed ? "true" : "false")
                 << "</passed>\n";
            file << "    <message>" << result.message << "</message>\n";
            file << "    <duration>" << result.duration << "</duration>\n";
            file << "    <timed_out>" << (result.timedOut ? "true" : "false")
                 << "</timed_out>\n";
            file << "  </testcase>\n";
        }
        file << "</testsuite>\n";
        file.close();
        std::cout << "Test report saved to " << filename << ".xml\n";
    } else if (format == "html") {
        std::ofstream file(filename + ".html");
        file << "<!DOCTYPE html><html><head><title>Test Report</title></head>"
                "<body>\n";
        file << "<h1>Test Report</h1>\n";
        file << "<p>Total Tests: " << stats.totalTests << "</p>\n";
        file << "<p>Passed Asserts: " << stats.passedAsserts << "</p>\n";
        file << "<p>Failed Asserts: " << stats.failedAsserts << "</p>\n";
        file << "<p>Skipped Tests: " << stats.skippedTests << "</p>\n";
        file << "<ul>\n";
        for (const auto& result : stats.results) {
            file << "  <li><strong>" << result.name << "</strong>: "
                 << (result.passed ? "<span style='color:green;'>PASSED</span>"
                                   : "<span style='color:red;'>FAILED</span>")
                 << " (" << result.duration << " ms)</li>\n";
        }
        file << "</ul>\n";
        file << "</body></html>";
        file.close();
        std::cout << "Test report saved to " << filename << ".html\n";
    }
}

/**
 * @brief Run a single test case
 * @param test Test case to run
 * @param retryCount Number of times to retry on failure
 */
ATOM_INLINE void runTestCase(const TestCase& test, int retryCount = 0) {
    auto& stats = getTestStats();
    Timer timer;
    auto& hooks = getHooks();

    if (test.skip) {
        printColored("SKIPPED\n", "1;33");
        std::lock_guard lock(getTestMutex());
        stats.skippedTests++;
        stats.totalTests++;
        stats.results.push_back(
            {std::string(test.name), false, true, "Test Skipped", 0.0, false});
        return;
    }

    std::string resultMessage;
    bool passed = false;
    bool timedOut = false;

    try {
        // Execute beforeEach hook if set
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

    // Execute afterEach hook if set
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
    stats.results.push_back({std::string(test.name), passed, false,
                             resultMessage, timer.elapsed(), timedOut});

    if (timedOut) {
        printColored(resultMessage + " (TIMEOUT)", "1;31");
    } else {
        printColored(resultMessage, passed ? "1;32" : "1;31");
    }
    std::cout << " (" << timer.elapsed() << " ms)\n";
}

/**
 * @brief Run tests in parallel using multiple threads
 * @param tests Vector of test cases to run
 * @param numThreads Number of threads to use
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
 * @brief Run all tests with configuration options
 * @param retryCount Number of times to retry failed tests
 * @param parallel Whether to run tests in parallel
 * @param numThreads Number of threads for parallel execution
 */
ATOM_INLINE void runAllTests(int retryCount = 0, bool parallel = false,
                             int numThreads = 4);

/**
 * @brief Run tests with command line arguments
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
        std::string arg = argv[i];
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

    auto& hooks = getHooks();
    if (hooks.beforeAll) {
        hooks.beforeAll();
    }

    if (!filterPattern.empty()) {
        std::regex pattern(filterPattern);
        auto filteredTests = filterTests(pattern);
        runAllTests(retryCount, parallel, numThreads);
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
 * @brief Run tests with default settings
 */
ATOM_INLINE void runTests() { runTests(0, nullptr); }

/**
 * @brief Filter tests by name using regex
 * @param pattern Regex pattern to match test names
 * @return Vector of matching test cases
 */
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

/**
 * @brief Filter tests by tag
 * @param tag Tag to filter by
 * @return Vector of test cases with matching tag
 */
ATOM_INLINE auto filterTestsByTag(const std::string& tag)
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

/**
 * @brief Run a filtered subset of tests
 * @param tests Vector of test cases to run
 * @param retryCount Number of times to retry failed tests
 * @param parallel Whether to run tests in parallel
 * @param numThreads Number of threads for parallel execution
 */
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

    auto& stats = getTestStats();
    std::cout << "============================================================="
                 "==================\n";
    std::cout << "Total tests: " << stats.totalTests << "\n";
    std::cout << "Total asserts: " << stats.totalAsserts << " | "
              << stats.passedAsserts << " passed | " << stats.failedAsserts
              << " failed | " << stats.skippedTests << " skipped\n";
}

/**
 * @brief Sort tests by dependencies
 * @param tests Vector of test cases to sort
 * @return Sorted vector of test cases
 */
ATOM_INLINE auto sortTestsByDependencies(const std::vector<TestCase>& tests)
    -> std::vector<TestCase> {
    std::map<std::string, TestCase> testMap;
    std::vector<TestCase> sortedTests;
    std::set<std::string> processed;

    // 首先构建名称到测试用例的映射
    for (const auto& test : tests) {
        testMap[test.name] = test;
    }

    // 递归处理依赖关系
    std::function<void(const TestCase&)> resolveDependencies;
    resolveDependencies = [&](const TestCase& test) {
        // 如果测试已经处理过，则跳过
        if (!processed.contains(test.name)) {
            // 先处理该测试的所有依赖
            for (const auto& dep : test.dependencies) {
                if (testMap.contains(dep)) {
                    resolveDependencies(testMap[dep]);
                }
            }
            // 将当前测试标记为已处理并添加到结果中
            processed.insert(test.name);
            sortedTests.push_back(test);
        }
    };

    // 对每个测试用例应用依赖解析
    for (const auto& test : tests) {
        resolveDependencies(test);
    }

    return sortedTests;
}

/**
 * @brief Run all tests
 * @param retryCount Number of times to retry failed tests
 * @param parallel Whether to run tests in parallel
 * @param numThreads Number of threads for parallel execution
 */
ATOM_INLINE void runAllTests(int retryCount, bool parallel, int numThreads) {
    auto& stats = getTestStats();
    Timer globalTimer;

    std::vector<TestCase> allTests;
    for (const auto& suite : getTestSuites()) {
        allTests.insert(allTests.end(), suite.testCases.begin(),
                        suite.testCases.end());
    }

    // Sort tests by dependencies
    allTests = sortTestsByDependencies(allTests);

    if (parallel) {
        runTestsInParallel(allTests, numThreads);
    } else {
        for (const auto& test : allTests) {
            runTestCase(test, retryCount);
        }
    }

    std::cout << "============================================================="
                 "==================\n";
    std::cout << "Total tests: " << stats.totalTests << "\n";
    std::cout << "Total asserts: " << stats.totalAsserts << " | "
              << stats.passedAsserts << " passed | " << stats.failedAsserts
              << " failed | " << stats.skippedTests << " skipped\n";
    std::cout << "Total time: " << globalTimer.elapsed() << " ms\n";
}

/**
 * @brief Test assertion base class
 * @details Handles basic assertion logic and statistics tracking
 */
struct alignas(64) Expect {
    bool result;
    const char* file;
    int line;
    std::string message;

    /**
     * @brief Construct an assertion
     * @param result Result of the assertion
     * @param file Source file where the assertion was made
     * @param line Line number where the assertion was made
     * @param msg Message describing the assertion
     */
    Expect(bool result, const char* file, int line, std::string msg)
        : result(result), file(file), line(line), message(msg) {
        auto& stats = getTestStats();
        stats.totalAsserts++;
        if (!result) {
            stats.failedAsserts++;
            throw std::runtime_error(std::string(file) + ":" +
                                     std::to_string(line) + ": FAILED - " +
                                     std::string(msg));
        }
        stats.passedAsserts++;
    }
};

/**
 * @brief Approximate equality assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param epsilon Maximum allowed difference
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

/**
 * @brief Equality assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
auto expectEq(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    return Expect(lhs == rhs, file, line,
                  std::string("Expected ") + std::to_string(lhs) +
                      " == " + std::to_string(rhs));
}

/**
 * @brief Inequality assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
auto expectNe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    return Expect(lhs != rhs, file, line,
                  std::string("Expected ") + std::to_string(lhs) +
                      " != " + std::to_string(rhs));
}

/**
 * @brief Greater-than assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
auto expectGt(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    return Expect(lhs > rhs, file, line,
                  std::string("Expected ") + std::to_string(lhs) + " > " +
                      std::to_string(rhs));
}

/**
 * @brief String contains assertion
 * @param str String to search in
 * @param substr Substring to search for
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
ATOM_INLINE auto expectContains(const std::string& str,
                                const std::string& substr, const char* file,
                                int line) -> Expect {
    bool result = str.contains(substr);
    return {result, file, line,
            "Expected \"" + str + "\" to contain \"" + substr + "\""};
}

/**
 * @brief Set equality assertion
 * @param lhs Left-hand set
 * @param rhs Right-hand set
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
    bool result = lhsSet == rhsSet;
    return {result, file, line, "Expected sets to be equal"};
}

/**
 * @brief Less-than assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
auto expectLt(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    return Expect(lhs < rhs, file, line,
                  std::string("Expected ") + std::to_string(lhs) + " < " +
                      std::to_string(rhs));
}

/**
 * @brief Greater-than-or-equal assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
auto expectGe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    return Expect(lhs >= rhs, file, line,
                  std::string("Expected ") + std::to_string(lhs) +
                      " >= " + std::to_string(rhs));
}

/**
 * @brief Less-than-or-equal assertion
 * @param lhs Left-hand value
 * @param rhs Right-hand value
 * @param file Source file
 * @param line Line number
 * @return Assertion result
 */
template <typename T, typename U>
    requires std::is_convertible_v<
        decltype(std::declval<T>() <= std::declval<U>()), bool>
auto expectLe(const T& lhs, const U& rhs, const char* file, int line)
    -> Expect {
    if constexpr (std::is_arithmetic_v<T> && std::is_arithmetic_v<U>) {
        return Expect(lhs <= rhs, file, line,
                      std::string("Expected ") + std::to_string(lhs) +
                          " <= " + std::to_string(rhs));
    } else {
        using namespace std::string_literals;
        std::stringstream stream;
        stream << "Expected " << lhs << " <= " << rhs;
        return Expect(lhs <= rhs, file, line, stream.str());
    }
}

/**
 * @brief Predicate-based assertion
 * @param value Value to test
 * @param predicate Predicate function
 * @param file Source file
 * @param line Line number
 * @param message Custom message
 * @return Assertion result
 */
template <typename T, typename Pred>
    requires std::is_invocable_r_v<bool, Pred, T>
auto expectThat(const T& value, Pred predicate, const char* file, int line,
                const std::string& message = "") -> Expect {
    bool result = predicate(value);
    return {result, file, line,
            message.empty() ? "Predicate failed for value" : message};
}

/**
 * @brief Exception assertion
 * @param func Function that should throw
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
 * @brief Test suite builder class
 * @details Uses RAII pattern to organize tests in suites
 */
class TestSuiteBuilder {
public:
    /**
     * @brief Construct a test suite builder
     * @param name Suite name
     */
    explicit TestSuiteBuilder(std::string name) : suiteName_(std::move(name)) {}

    /**
     * @brief Destructor - registers the suite
     */
    ~TestSuiteBuilder() {
        if (!testCases_.empty()) {
            registerSuite(suiteName_, std::move(testCases_));
        }
    }

    /**
     * @brief Add a test to the suite
     * @param name Test name
     * @param func Test function
     * @param async Whether to run asynchronously
     * @param timeLimit Time limit in milliseconds
     * @param skip Whether to skip the test
     * @param dependencies Test dependencies
     * @param tags Optional test tags
     * @return Reference to this builder for method chaining
     */
    TestSuiteBuilder& addTest(std::string name, std::function<void()> func,
                              bool async = false, double timeLimit = 0.0,
                              bool skip = false,
                              std::vector<std::string> dependencies = {},
                              std::vector<std::string> tags = {}) {
        testCases_.push_back(TestCase{.name = std::move(name),
                                      .func = std::move(func),
                                      .skip = skip,
                                      .async = async,
                                      .timeLimit = timeLimit,
                                      .dependencies = std::move(dependencies),
                                      .tags = std::move(tags)});
        return *this;
    }

    // Disable copy and move operations
    TestSuiteBuilder(const TestSuiteBuilder&) = delete;
    TestSuiteBuilder& operator=(const TestSuiteBuilder&) = delete;
    TestSuiteBuilder(TestSuiteBuilder&&) = delete;
    TestSuiteBuilder& operator=(TestSuiteBuilder&&) = delete;

private:
    std::string suiteName_;            ///< Suite name
    std::vector<TestCase> testCases_;  ///< Test cases in this suite
};

}  // namespace atom::test

// Assertion macros
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
 * @brief String literal operator for creating test cases
 *
 * Example usage:
 * ```cpp
 * "Integer equality test"_test([]() {
 *     expect_eq(1, 1);
 * });
 * ```
 *
 * @param name Test name
 * @param size Name length (automatically deduced)
 * @return Function object for test configuration
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
