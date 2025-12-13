/**
 * @file atom_test.hpp
 * @brief Unified entry point for Atom Test Framework
 * @details This header provides a single include for all testing functionality
 *
 * @section features Features
 * - Core testing framework with assertions
 * - Test fixtures for setup/teardown
 * - Parameterized tests for data-driven testing
 * - Typed tests for template code testing
 * - Mock objects, spies, stubs, and fakes
 * - GTest-style matchers (EXPECT_THAT, matchers)
 * - Death tests for crash testing
 * - Conditional test skipping with platform detection
 * - Timeout support and flaky test handling
 * - Multiple report formats (Console, JSON, XML, HTML, Markdown)
 * - Test environments for global setup/teardown
 * - Benchmarking utilities
 * - Fuzz testing support
 * - Performance profiling
 *
 * @section usage Basic Usage
 * @code
 * #include <atom/tests/atom_test.hpp>
 *
 * // Simple test
 * TEST(MySuite, MyTest) {
 *     EXPECT_EQ(1 + 1, 2);
 *     EXPECT_THAT("hello world", HasSubstr("world"));
 * }
 *
 * // Fixture-based test
 * class MyFixture : public atom::test::TestFixture {
 * protected:
 *     void SetUp() override { value = 42; }
 *     int value = 0;
 * };
 *
 * TEST_F(MyFixture, ValueTest) {
 *     ASSERT_EQ(value, 42);
 * }
 *
 * // Parameterized test
 * class ParamTest : public atom::test::ParameterizedTest<int> {};
 * INSTANTIATE_TEST_SUITE_P(Numbers, ParamTest, atom::test::Values({1, 2, 3}));
 *
 * TEST_P(ParamTest, IsPositive) {
 *     EXPECT_TRUE(GetParam() > 0);
 * }
 *
 * // Typed test
 * template <typename T>
 * class TypedFixture : public atom::test::TypedTestFixture<T> {};
 * TYPED_TEST_SUITE(TypedFixture, atom::test::NumericTypes);
 *
 * TYPED_TEST(TypedFixture, Addition) {
 *     TypeParam a = 1, b = 2;
 *     EXPECT_EQ(a + b, 3);
 * }
 *
 * // Main function
 * int main(int argc, char** argv) {
 *     return atom::test::runAllTests(argc, argv);
 * }
 * @endcode
 *
 * @author Max Qian
 * @copyright GPL3 License
 */

#ifndef ATOM_TEST_HPP
#define ATOM_TEST_HPP

// ============================================================================
// Core Testing Framework
// ============================================================================

// Core test definitions and assertions
#include "atom/tests/core/test.hpp"

// Test registry
#include "atom/tests/core/test_registry.hpp"

// Test runner
#include "atom/tests/core/test_runner.hpp"

// Command-line interface
#include "atom/tests/core/test_cli.hpp"

// ============================================================================
// Assertions and Matchers
// ============================================================================

// GTest-style matchers
#include "atom/tests/assertions/matchers.hpp"

// String-specific matchers
#include "atom/tests/assertions/string_matchers.hpp"

// Death tests
#include "atom/tests/assertions/death_test.hpp"

// ============================================================================
// Test Fixtures
// ============================================================================

// Base fixture and TEST/TEST_F macros
#include "atom/tests/fixtures/test_fixture.hpp"

// Parameterized tests
#include "atom/tests/fixtures/test_parameterized.hpp"

// Typed tests
#include "atom/tests/fixtures/typed_test.hpp"

// ============================================================================
// Mocking Framework
// ============================================================================

// Mock functions, spies, stubs, fakes
#include "atom/tests/mocking/test_mock.hpp"

// Mock argument matchers
#include "atom/tests/mocking/mock_matchers.hpp"

// ============================================================================
// Test Utilities
// ============================================================================

// Conditional skipping and timeout
#include "atom/tests/utilities/test_skip.hpp"

// Test data factories and generators
#include "atom/tests/utilities/test_data.hpp"

// Global test environments
#include "atom/tests/utilities/test_environment.hpp"

// ============================================================================
// Test Reporters
// ============================================================================

// Test reporters (Console, JSON, XML, HTML, Markdown)
#include "atom/tests/reporters/test_reporter.hpp"

// Reporter charts and visualizations
#include "atom/tests/reporters/test_reporter_charts.hpp"

// ============================================================================
// Performance Testing
// ============================================================================

// Benchmarking
#include "atom/tests/performance/benchmark.hpp"

// Fuzz testing
#include "atom/tests/performance/fuzz.hpp"

// Performance profiling
#include "atom/tests/performance/perf.hpp"

namespace atom::test {

// ============================================================================
// Main Entry Points
// ============================================================================

/**
 * @brief Run all registered tests with command-line argument parsing
 * @param argc Argument count from main()
 * @param argv Argument values from main()
 * @return 0 if all tests pass, non-zero otherwise
 */
inline auto runAllTests(int argc, char** argv) -> int {
    // Set up global environments
    EnvironmentGuard envGuard;

    auto parser = createDefaultParser();
    auto parseResult = parser.parse(argc, argv);

    if (!parseResult.success) {
        std::cerr << "Error parsing arguments: " << parseResult.errorMessage
                  << std::endl;
        return 1;
    }

    if (parseResult.helpRequested) {
        parser.printHelp();
        return 0;
    }

    TestRunnerConfig config;
    parser.applyToConfig(config);

    TestRunner runner(config);
    auto stats = runner.runAll();

    return stats.failedAsserts > 0 ? 1 : 0;
}

/**
 * @brief Run all registered tests with default configuration
 * @return 0 if all tests pass, non-zero otherwise
 */
inline auto runAllTests() -> int {
    EnvironmentGuard envGuard;
    TestRunner runner;
    auto stats = runner.runAll();
    return stats.failedAsserts > 0 ? 1 : 0;
}

/**
 * @brief Run tests with custom configuration
 * @param config Test runner configuration
 * @return Test statistics
 */
inline auto runTests(const TestRunnerConfig& config) -> TestStats {
    EnvironmentGuard envGuard;
    TestRunner runner(config);
    return runner.runAll();
}

/**
 * @brief Quick test runner for simple use cases
 * @param filter Optional regex filter for test names
 * @param verbose Enable verbose output
 * @return 0 if all tests pass, non-zero otherwise
 */
inline auto quickRun(const std::string& filter = "",
                     bool verbose = false) -> int {
    TestRunnerConfig config;
    if (!filter.empty()) {
        config.testFilter = filter;
    }
    config.enableVerboseOutput = verbose;

    TestRunner runner(config);
    auto stats = runner.runAll();
    return stats.failedAsserts > 0 ? 1 : 0;
}

/**
 * @brief List all registered tests
 * @param filter Optional regex filter
 */
inline void listTests(const std::string& filter = "") {
    auto& suites = getTestSuites();
    std::regex pattern(filter.empty() ? ".*" : filter);

    std::cout << "Registered Tests:\n";
    std::cout << "=================\n";

    int totalCount = 0;
    for (const auto& suite : suites) {
        for (const auto& test : suite.testCases) {
            if (std::regex_search(test.name, pattern)) {
                std::cout << "  ";
                if (test.skip) {
                    std::cout << "[DISABLED] ";
                }
                std::cout << test.name;
                if (!test.tags.empty()) {
                    std::cout << " [";
                    for (size_t i = 0; i < test.tags.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << test.tags[i];
                    }
                    std::cout << "]";
                }
                std::cout << "\n";
                totalCount++;
            }
        }
    }

    std::cout << "\nTotal: " << totalCount << " test(s)\n";
}

/**
 * @brief Run tests by tag
 * @param tag Tag to filter by
 * @param config Optional configuration
 * @return Test statistics
 */
inline auto runTestsByTag(const std::string& tag,
                          TestRunnerConfig config = {}) -> TestStats {
    auto tests = filterTestsByTag(tag);
    if (tests.empty()) {
        std::cout << "No tests found with tag: " << tag << "\n";
        return {};
    }

    runTestsFiltered(tests, config.maxRetries, config.enableParallel,
                     config.numThreads);
    return getTestStats();
}

/**
 * @brief Create a test suite builder for fluent API
 * @param suiteName Name of the test suite
 * @return Lambda for adding tests
 */
inline auto suite(const std::string& suiteName) {
    return [suiteName](std::initializer_list<
                       std::pair<std::string, std::function<void()>>> tests) {
        for (const auto& [name, func] : tests) {
            registerTest(suiteName + "." + name, func);
        }
    };
}

/**
 * @brief RAII guard for test setup/teardown
 */
class TestGuard {
public:
    explicit TestGuard(std::function<void()> teardown)
        : teardown_(std::move(teardown)) {}

    ~TestGuard() {
        if (teardown_) {
            try {
                teardown_();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    TestGuard(const TestGuard&) = delete;
    auto operator=(const TestGuard&) -> TestGuard& = delete;
    TestGuard(TestGuard&& other) noexcept
        : teardown_(std::move(other.teardown_)) {
        other.teardown_ = nullptr;
    }
    auto operator=(TestGuard&&) -> TestGuard& = delete;

private:
    std::function<void()> teardown_;
};

/**
 * @brief Create a test guard for RAII cleanup
 * @param teardown Function to call on scope exit
 * @return TestGuard object
 */
inline auto makeGuard(std::function<void()> teardown) -> TestGuard {
    return TestGuard(std::move(teardown));
}

// ============================================================================
// GTest-style Initialization
// ============================================================================

/**
 * @brief Initialize the test framework (GTest compatibility)
 * @param argc Pointer to argument count
 * @param argv Pointer to argument values
 */
inline void InitGoogleTest(int* argc, char** argv) {
    (void)argc;
    (void)argv;
    // No-op for compatibility - initialization happens in runAllTests
}

/**
 * @brief Run all tests (GTest compatibility)
 * @return 0 if all tests pass, non-zero otherwise
 */
inline int RUN_ALL_TESTS() { return runAllTests(); }

}  // namespace atom::test

// Convenience type aliases
namespace atom_test = atom::test;

// GTest-style global functions
#define InitGoogleTest atom::test::InitGoogleTest
#define RUN_ALL_TESTS() atom::test::RUN_ALL_TESTS()

#endif  // ATOM_TEST_HPP
