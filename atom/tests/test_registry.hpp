#ifndef ATOM_TEST_REGISTRY_HPP
#define ATOM_TEST_REGISTRY_HPP

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "atom/tests/test.hpp"

namespace atom::test {

/**
 * @brief Central test registry class
 * @details Manages registration and access to all test suites and test cases
 */
class TestRegistry {
public:
    /**
     * @brief Get the global test registry instance
     * @return Reference to the global TestRegistry object
     */
    static auto instance() -> TestRegistry& {
        static TestRegistry registry;
        return registry;
    }

    /**
     * @brief Register a test suite
     * @param suite The test suite to register
     */
    void registerSuite(TestSuite suite) {
        std::lock_guard<std::mutex> lock(mutex_);
        suites_.emplace_back(std::move(suite));
    }

    /**
     * @brief Register an individual test case
     * @param testCase The test case to register
     * @param suiteName Optional suite name (added to default suite if empty)
     */
    void registerTest(TestCase testCase, std::string_view suiteName = {}) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!suiteName.empty()) {
            auto it = std::find_if(suites_.begin(), suites_.end(),
                                   [suiteName](const TestSuite& s) {
                                       return s.name == suiteName;
                                   });
            if (it != suites_.end()) {
                it->testCases.emplace_back(std::move(testCase));
                return;
            }

            suites_.emplace_back(std::string(suiteName),
                                 std::vector<TestCase>{std::move(testCase)});
        } else {
            auto it =
                std::find_if(suites_.begin(), suites_.end(),
                             [](const TestSuite& s) { return s.name.empty(); });
            if (it != suites_.end()) {
                it->testCases.emplace_back(std::move(testCase));
            } else {
                suites_.emplace_back(
                    "", std::vector<TestCase>{std::move(testCase)});
            }
        }
    }

    /**
     * @brief Get all registered test suites
     * @return Const reference to all test suites
     */
    [[nodiscard]] auto getSuites() const -> const std::vector<TestSuite>& {
        return suites_;
    }

    /**
     * @brief Get mutable reference to all registered test suites
     * @return Mutable reference to all test suites
     */
    auto getSuites() -> std::vector<TestSuite>& { return suites_; }

    /**
     * @brief Clear all registered tests
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        suites_.clear();
    }

    /**
     * @brief Find a test case by name
     * @param name Test case name
     * @return Pointer to the found test case, nullptr if not found
     */
    [[nodiscard]] auto findTestByName(std::string_view name) const
        -> const TestCase* {
        for (const auto& suite : suites_) {
            auto it = std::find_if(
                suite.testCases.begin(), suite.testCases.end(),
                [name](const TestCase& test) { return test.name == name; });
            if (it != suite.testCases.end()) {
                return &(*it);
            }
        }
        return nullptr;
    }

    /**
     * @brief Find a test suite by name
     * @param name Test suite name
     * @return Pointer to the found test suite, nullptr if not found
     */
    [[nodiscard]] auto findSuiteByName(std::string_view name) const
        -> const TestSuite* {
        auto it = std::find_if(
            suites_.begin(), suites_.end(),
            [name](const TestSuite& suite) { return suite.name == name; });
        return (it != suites_.end()) ? &(*it) : nullptr;
    }

    /**
     * @brief Filter tests by tag
     * @param tag Tag to filter by
     * @return Vector of test cases that include the specified tag
     */
    [[nodiscard]] auto findTestsByTag(std::string_view tag) const
        -> std::vector<const TestCase*> {
        std::vector<const TestCase*> result;

        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                if (std::find(test.tags.begin(), test.tags.end(), tag) !=
                    test.tags.end()) {
                    result.push_back(&test);
                }
            }
        }

        return result;
    }

    /**
     * @brief Get all available test tags in the registry
     * @return Vector of unique tag strings
     */
    [[nodiscard]] auto getAllTags() const -> std::vector<std::string> {
        std::unordered_set<std::string> tagSet;

        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                tagSet.insert(test.tags.begin(), test.tags.end());
            }
        }

        std::vector<std::string> allTags(tagSet.begin(), tagSet.end());
        std::sort(allTags.begin(), allTags.end());

        return allTags;
    }

    /**
     * @brief Count the total number of registered test cases
     * @return Total test count
     */
    [[nodiscard]] auto getTotalTestCount() const -> size_t {
        size_t count = 0;
        for (const auto& suite : suites_) {
            count += suite.testCases.size();
        }
        return count;
    }

    /**
     * @brief Get the total number of test suites
     * @return Total suite count
     */
    [[nodiscard]] auto getTotalSuiteCount() const -> size_t {
        return suites_.size();
    }

    /**
     * @brief Check if a test case exists by name
     * @param name Test case name
     * @return True if the test exists, false otherwise
     */
    [[nodiscard]] auto hasTest(std::string_view name) const -> bool {
        return findTestByName(name) != nullptr;
    }

    /**
     * @brief Check if a test suite exists by name
     * @param name Test suite name
     * @return True if the suite exists, false otherwise
     */
    [[nodiscard]] auto hasSuite(std::string_view name) const -> bool {
        return findSuiteByName(name) != nullptr;
    }

    /**
     * @brief Run all tests in a specific suite
     * @param suiteName Name of the suite to run
     * @return True if all tests passed, false otherwise
     */
    [[nodiscard]] auto runSuite(std::string_view suiteName) -> bool {
        const TestSuite* suite = findSuiteByName(suiteName);
        if (!suite) {
            return false;
        }

        return std::all_of(
            suite->testCases.begin(), suite->testCases.end(),
            [](const TestCase& test) { return test.testFunction(); });
    }

    /**
     * @brief Get tests from a specific suite
     * @param suiteName Name of the suite
     * @return Vector of pointers to test cases in the suite
     */
    [[nodiscard]] auto getTestsFromSuite(std::string_view suiteName) const
        -> std::vector<const TestCase*> {
        const TestSuite* suite = findSuiteByName(suiteName);
        if (!suite) {
            return {};
        }

        std::vector<const TestCase*> tests;
        tests.reserve(suite->testCases.size());

        for (const auto& test : suite->testCases) {
            tests.push_back(&test);
        }

        return tests;
    }

    /**
     * @brief Filter tests by multiple tags (AND logic)
     * @param tags Vector of tags that all must be present
     * @return Vector of test cases that include all specified tags
     */
    [[nodiscard]] auto findTestsByTags(const std::vector<std::string>& tags)
        const -> std::vector<const TestCase*> {
        if (tags.empty()) {
            return {};
        }

        std::vector<const TestCase*> result;

        for (const auto& suite : suites_) {
            for (const auto& test : suite.testCases) {
                bool hasAllTags = std::all_of(
                    tags.begin(), tags.end(), [&test](const std::string& tag) {
                        return std::find(test.tags.begin(), test.tags.end(),
                                         tag) != test.tags.end();
                    });

                if (hasAllTags) {
                    result.push_back(&test);
                }
            }
        }

        return result;
    }

private:
    std::vector<TestSuite> suites_;
    mutable std::mutex mutex_;

    TestRegistry() = default;

public:
    TestRegistry(const TestRegistry&) = delete;
    auto operator=(const TestRegistry&) -> TestRegistry& = delete;
    TestRegistry(TestRegistry&&) = delete;
    auto operator=(TestRegistry&&) -> TestRegistry& = delete;
};

/**
 * @brief Global convenience functions for test registration
 */

/**
 * @brief Register a test suite with the global registry
 * @param suite The test suite to register
 */
inline void registerTestSuite(TestSuite suite) {
    TestRegistry::instance().registerSuite(std::move(suite));
}

/**
 * @brief Register a test case with the global registry
 * @param testCase The test case to register
 * @param suiteName Optional suite name
 */
inline void registerTestCase(TestCase testCase,
                             std::string_view suiteName = {}) {
    TestRegistry::instance().registerTest(std::move(testCase), suiteName);
}

/**
 * @brief Clear all tests from the global registry
 */
inline void clearAllTests() { TestRegistry::instance().clear(); }

}  // namespace atom::test

#endif  // ATOM_TEST_REGISTRY_HPP